#include "Particle.h"
#include "dct.h"
#include <chrono>
#include <string>
#include <LiquidCrystal.h>
#include <cstdlib>
#include "MQTT5.h"
/*
 * clusterhead.ino
 * Description: code to flash to the "clusterhead" argon for assignment 3
 * Author: Tom Schwenke
 * Date: 07/05/2020
 */

// This example does not require the cloud so you can run it in manual mode or
// normal cloud-connected mode
SYSTEM_MODE(AUTOMATIC);

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

/* holds the starting millis of this execution of the main loop, 
so we can work out how long to wait before beginning the next loop */
unsigned long loopStart = 0;

/* Data tracking variables */
int currentSound = 0;
uint16_t currentLight = 0;
uint8_t currentDistance = 0;

//Gets temp and humidity from sn1, and light from sn2 for logic
int8_t getTempsn1 = -127;
int8_t getHumidsn1 = -1;

int16_t getLightsn2 = -1;
uint8_t getHumanDetectsn2 = 0x00;

//true if the difference between the last two distance readings was more than 1cm
bool moving = false;

/* Bluetooth variables */
//bluetooth devices we want to connect to and their service ids
BlePeerDevice sensorNode1;
BlePeerDevice sensorNode2;
BleUuid sensorNode1ServiceUuid("754ebf5e-ce31-4300-9fd5-a8fb4ee4a811");
BleUuid sensorNode2ServiceUuid("97728ad9-a998-4629-b855-ee2658ca01f7");

//characteristics we want to track
//for sensor node 1
BleCharacteristic temperatureSensorCharacteristic;
BleCharacteristic humiditySensorCharacteristic;
BleCharacteristic lightSensorCharacteristic;
BleCharacteristic moistureSensorCharacteristic;

//for sensor node 2
BleCharacteristic rainsteamSensorCharacteristic;
BleCharacteristic liquidSensorCharacteristic;
BleCharacteristic humanDetectorCharacteristic;
BleCharacteristic solenoidVoltageCharacteristic;

//Sensor logic
bool isWatering = false;    //Is the solenoid active or not?


// void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
const size_t SCAN_RESULT_MAX = 30;
BleScanResult scanResults[SCAN_RESULT_MAX];

//MQTT client used to publish MQTT messages
MQTT5 client;
//functions used to handle MQTT
void mqttFailure(MQTT5_REASON_CODE reason) {
    // See codes here: https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031
    char *buf;
    size_t sz;
    sz = snprintf(NULL, 0, "Failure due to reason %d", (int) reason);
    buf = (char *)malloc(sz + 1); /* make sure you check for != NULL in real code */
    snprintf(buf, sz+1, "Failure due to reason %d", (int) reason);
    
   Particle.publish(buf, PRIVATE);
}

void mqttPacketReceived(char* topic, uint8_t* payload, uint16_t payloadLength, bool dup, MQTT5_QOS qos, bool retain) {
    char content[payloadLength + 1];
    memcpy(content, payload, payloadLength);
    content[payloadLength] = 0;
    Log.info("Topic: %s. Message: %s", topic, payload);
}

void setup() {

    //setup MQTT
    client.onConnectFailed(mqttFailure);
    client.onPublishFailed(mqttFailure);
    client.onSubscribeFailed(mqttFailure);
    client.onPacketReceived(mqttPacketReceived);

    if (client.connect("test.mosquitto.org", 1883, "client123") && client.awaitPackets()) {
       client.publish("elec4740g6/data", "Hello world");
       Particle.publish("MQTT conneccted successfully!", PRIVATE);
    }
    else{
        Particle.publish("MQTT connection failure :(", PRIVATE);
    }

    const uint8_t val = 0x01;
    dct_write_app_data(&val, DCT_SETUP_DONE_OFFSET, 1);
    (void)logHandler; // Does nothing, just to eliminate the unused variable warning

    BLE.on();
    
    
    //map functions to be called whenever new data is received for a characteristic
    temperatureSensorCharacteristic.onDataReceived(onTemperatureReceived1, NULL);
    humiditySensorCharacteristic.onDataReceived(onHumidityReceived, NULL);
    lightSensorCharacteristic.onDataReceived(onLightReceived, NULL);
    moistureSensorCharacteristic.onDataReceived(onMoistureReceived, NULL);

    rainsteamSensorCharacteristic.onDataReceived(onRainsteamReceived2, NULL);
    liquidSensorCharacteristic.onDataReceived(onLightReceived2, NULL);
    humanDetectorCharacteristic.onDataReceived(onHumanDetectorReceived, NULL);
    solenoidVoltageCharacteristic.onDataReceived(onSolenoidReceived2, NULL);

}

void loop() { 
    //do stuff if both sensors have been connected
    if ((sensorNode1.connected()) || (sensorNode2.connected())) {   //Add this back in when required!
        //record start time of this loop
        loopStart = millis();

        //TEST
        client.publish("elec4740g6/data","test");
        
        //Sensor logic for watering
        //Change this to send when it changes not constantly
        if(isWatering == false)
        {
            //solenoidVoltageCharacteristic.setValue(0);
        }
        if(isWatering == true)
        {
            //solenoidVoltageCharacteristic.setValue(1);

        }
    }
    //if we haven't connected both, then scan for them
    else {
        Log.info("About to scan...");
        int count = BLE.scan(scanResults, SCAN_RESULT_MAX);
        for (int i = 0; i < count; i++) {
            BleUuid foundService;
            size_t len;

            //Read the service UUID of this BT device
            len = scanResults[i].advertisingData.serviceUUID(&foundService, 1);

            Log.info("Found a bluetooth device.");
            Log.info("Address: " + scanResults[i].address.toString());
            Log.info("Found UUID: " + foundService.toString());
            Log.info("SensorNode1 UUID: " + sensorNode1ServiceUuid.toString());
            Log.info("SensorNode2 UUID: " + sensorNode2ServiceUuid.toString());

            //Check if it matches UUID for sensor node 1
            if (len > 0 && foundService == sensorNode1ServiceUuid){
                Log.info("Found sensor node 1.");
                if(sensorNode1.connected() == false){
                    sensorNode1 = BLE.connect(scanResults[i].address);
                    if(sensorNode1.connected()){
                        Log.info("Successfully connected to sensor node 1!");
                        //map characteristics from this service to the variables in this program, so they're handled by our "on<X>Received" functions
                        sensorNode1.getCharacteristicByUUID(temperatureSensorCharacteristic, "29fba3f5-4ce8-46bc-8d75-77806db22c31");
                        sensorNode1.getCharacteristicByUUID(humiditySensorCharacteristic, "99a0d2f9-1cfa-42b3-b5ba-1b4d4341392f");
                        sensorNode1.getCharacteristicByUUID(lightSensorCharacteristic, "45be4a56-48f5-483c-8bb1-d3fee433c23c");
                        sensorNode1.getCharacteristicByUUID(moistureSensorCharacteristic, "ea5248a4-43cc-4198-a4aa-79200a750835");
                    }
                    else{
                        Log.info("Failed to connect to sensor node 1.");
                    }
                }
                else{
                    Log.info("Sensor node 1 already connected.");
                }
            }

            //Check if it matches UUID for sensor node 2
            else if (len > 0 && foundService == sensorNode2ServiceUuid){
                Log.info("Found sensor node 2.");
                if(sensorNode2.connected() == false){
                    sensorNode2 = BLE.connect(scanResults[i].address);
                    if(sensorNode2.connected()){
                        Log.info("Successfully connected to sensor node 2!");
                        //map characteristics from this service to the variables in this program, so they're handled by our "on<X>Received" functions
                        sensorNode2.getCharacteristicByUUID(rainsteamSensorCharacteristic, "bc7f18d9-2c43-408e-be25-62f40645987c");
                        sensorNode2.getCharacteristicByUUID(liquidSensorCharacteristic, "88ba2f5d-1e98-49af-8697-d0516df03be9");
                        sensorNode2.getCharacteristicByUUID(humanDetectorCharacteristic, "b482d551-c3ae-4dde-b125-ce244d7896b0");
                        sensorNode2.getCharacteristicByUUID(solenoidVoltageCharacteristic, "97017674-9615-4fba-9712-6829f2045836");

                    }
                    else{
                        Log.info("Failed to connect to sensor node 2.");
                    }
                }
                else{
                    Log.info("Sensor node 2 already connected.");
                }
            }
        }

        if (count > 0) {
            Log.info("%d devices found", count);
        }
    }
}

/* These functions are where we do something with the data (in bytes) we've received via bluetooth */

void onTemperatureReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    int8_t receivedTemp;
    uint64_t sentTime;

    
    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(receivedTemp), sizeof(sentTime));
    //read the temp
    memcpy(&receivedTemp, &data[0], sizeof(receivedTemp));
    

    Log.info("Sensor 1 - Temperature: %u degrees Celsius", receivedTemp);
    getTempsn1 = receivedTemp;
    // Log.info("Temp/humidity transmission delay: %llu seconds", calculateTransmissionDelay(sentTime));
}

void onHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint8_t receivedHumidity;
    memcpy(&receivedHumidity, &data[0], sizeof(receivedHumidity));
    Log.info("Sensor 1 - Humidity: %u%%", receivedHumidity);
}

void onMoistureReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the current sensor reading
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    
    Log.info("Sensor 1 - Moisture: %u", twoByteValue);
}

void onSolenoidReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the current sensor reading
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    
    Log.info("Sensor 2 - Solenoid: %u ", twoByteValue);
}

void onCurrentReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the current sensor reading
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    
    Log.info("Sensor 2 - Current: %u Amps", twoByteValue);
}

void onLightReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the current sensor reading
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    
    Log.info("Sensor 1 - Light: %u Lux", twoByteValue);
}

void onRainsteamReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    int8_t rainsteam;
    uint64_t sentTime;

    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(rainsteam), sizeof(sentTime));

    memcpy(&rainsteam, &data[0], sizeof(rainsteam));
    Log.info("Sensor 2 - Rainsteam: %d ", rainsteam);
    // Log.info("Transmission delay: %llu seconds", calculateTransmissionDelay(sentTime));
}

void onLightReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint16_t twoByteValue;
    uint64_t sentTime;

    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(twoByteValue), sizeof(sentTime));

    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    Log.info("Sensor 2 - Liquid: %u ", twoByteValue);

    currentLight = twoByteValue;
    // Log.info("Transmission delay: %llu seconds", calculateTransmissionDelay(sentTime));
}


void onHumanDetectorReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint8_t humanSeen;
    uint64_t sentTime;

    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(uint8_t), sizeof(sentTime));

    memcpy(&humanSeen, &data[0], sizeof(uint8_t));
    Log.info("Sensor 2 - Human detector: %u", humanSeen);
    if(humanSeen == 0x00){
        Log.info("Sensor 2 - Human lost...");
    }
    else if (humanSeen == 0x01){
        Log.info("Sensor 2 - Human detected!");
    }
    else{
        Log.info("Sensor 2 - Invalid human detector message. Expected 0 or 1, received %u", humanSeen);
    }
    // Log.info("Transmission delay: %llu seconds", calculateTransmissionDelay(sentTime));
}