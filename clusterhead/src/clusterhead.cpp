/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "c:/Users/tschw/repos/elec4740Group6/clusterhead/src/clusterhead.ino"
#include "Particle.h"
#include "dct.h"
#include <string>
#include <LiquidCrystal.h>
#include <cstdlib>
#include "MQTT5.h"
#include <vector>
/*
 * clusterhead.ino
 * Description: code to flash to the "clusterhead" argon for assignment 3
 * Author: Tom Schwenke
 * Date: 07/05/2020
 */

// This example does not require the cloud so you can run it in manual mode or
// normal cloud-connected mode
void mqttFailure(MQTT5_REASON_CODE reason);
void mqttPacketReceived(char* topic, uint8_t* payload, uint16_t payloadLength, bool dup, MQTT5_QOS qos, bool retain);
int sprinklerSwitch(String activate);
int forceMqttPublish(String s);
int setLowSoilMoistureThreshold(String threshold);
int setHighSoilMoistureThreshold(String threshold);
int setTemperatureThreshold(String threshold);
int setAirHumidityThreshold(String threshold);
int setSunnyLightThreshold(String threshold);
void setup();
void loop();
void switchSprinkler();
bool publishMqtt();
void onTemperatureReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onMoistureReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onLightReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onRainsteamReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onLiquidLevelReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onHumanDetectorReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
#line 17 "c:/Users/tschw/repos/elec4740Group6/clusterhead/src/clusterhead.ino"
SYSTEM_MODE(SEMI_AUTOMATIC);

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

/* holds the starting millis of this execution of the main loop, 
so we can work out how long to wait before beginning the next loop */
unsigned long loopStart = 0;

/* Data tracking variables */
int8_t currentTemperature = 100;//-127;
int8_t currentHumidity = 90;//-1;
uint8_t currentLight = 80;//0;
uint16_t currentMoisture = 70;//-1;

uint8_t currentRainsteam = 60;//-1;
uint8_t currentLiquid = 50;//-1;
int8_t currentHumanDetect = 0;//-1;
uint8_t initWateringStatus = 1;//0;
bool isWatering = false;    //Is the solenoid active or not?
std::vector<int32_t> wateringEventTimes = {};

/* Watering system threshold variables */
int LOW_SOIL_MOISTURE_THRESHOLD = 75;
int HIGH_SOIL_MOISTURE_THRESHOLD = 80;
int TEMPERATURE_THRESHOLD = 32;
int AIR_HUMIDITY_THRESHOLD = 100;
int SUNNY_LIGHT_THRESHOLD = 68;

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

// void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
const size_t SCAN_RESULT_MAX = 30;
BleScanResult scanResults[SCAN_RESULT_MAX];

//MQTT client used to publish MQTT messages
MQTT5 client;
const int PUBLISH_DELAY = 15*1000;//15 minute publish delay
unsigned long lastPublishTime = millis();

//functions used to handle MQTT
void mqttFailure(MQTT5_REASON_CODE reason) {
    // See codes here: https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031
    char *buf;
    size_t sz;
    sz = snprintf(NULL, 0, "Failure due to reason %d", (int) reason);
    buf = (char *)malloc(sz + 1); /* make sure you check for != NULL in real code */
    snprintf(buf, sz+1, "Failure due to reason %d", (int) reason);
    
    Log.info("Failure due to reason: %d", (int) reason);
}

void mqttPacketReceived(char* topic, uint8_t* payload, uint16_t payloadLength, bool dup, MQTT5_QOS qos, bool retain) {
    char content[payloadLength + 1];
    memcpy(content, payload, payloadLength);
    content[payloadLength] = 0;
    Log.info("Topic: %s. Message: %s", topic, payload);
}

/* Particle functions for Particle console control */
/* Manually turn sprinkler system on/off */
int sprinklerSwitch(String activate){
    if(activate.equalsIgnoreCase("on")){
        //turn on sprinkler if it is off
        if(!isWatering){
            switchSprinkler();
        }
    }
    else if (activate.equalsIgnoreCase("off")){
        //turn off sprinkler if it is on
        if(isWatering){
            switchSprinkler();
        }
    }
    else{
        return 0;
    }
    return 1;
}

int forceMqttPublish(String s){
    if(publishMqtt()){
        return 1;
    }
    return 0;
}

/* Threshold modification functions */
int setLowSoilMoistureThreshold(String threshold){
    LOW_SOIL_MOISTURE_THRESHOLD = threshold.toInt();
    return 1;
}
int setHighSoilMoistureThreshold(String threshold){
    HIGH_SOIL_MOISTURE_THRESHOLD = threshold.toInt();
    return 1;
}
int setTemperatureThreshold(String threshold){
    TEMPERATURE_THRESHOLD = threshold.toInt();
    return 1;
}
int setAirHumidityThreshold(String threshold){
    AIR_HUMIDITY_THRESHOLD = threshold.toInt();
    return 1;
}
int setSunnyLightThreshold(String threshold){
    SUNNY_LIGHT_THRESHOLD = threshold.toInt();
    return 1;
}

void setup() {
    WiFi.on();
    WiFi.connect();

    //setup MQTT
    client.onConnectFailed(mqttFailure);
    client.onPublishFailed(mqttFailure);
    client.onSubscribeFailed(mqttFailure);
    client.onPacketReceived(mqttPacketReceived);

    
    //  You can also use IP address:
     uint8_t server[] = {192, 168, 1, 1};
    //  instead of domain:
    //  "test.mosquitto.org"
    
    if (client.connect(/*"test.mosquitto.org"*/server, 1883, "elec4740g6client") && client.awaitPackets()) {
       client.publish("elec4740g6/test", "Hello world", strlen("Hello world"));
       Log.info("@@@@@@@@@@@@@@@@@@@@@@@@@@MQTT connected succsessfully!");
    //    Particle.publish("MQTT conneccted successfully!", PRIVATE);
    }
    else{
        Log.info("@@@@@@@@@@@@@@@@@@@@@@@@@@@@MQTT connection failure :(");
    }

    const uint8_t val = 0x01;
    dct_write_app_data(&val, DCT_SETUP_DONE_OFFSET, 1);
    (void)logHandler; // Does nothing, just to eliminate the unused variable warning

    BLE.on();
    
    
    //map functions to be called whenever new data is received for a characteristic
    temperatureSensorCharacteristic.onDataReceived(onTemperatureReceived, NULL);
    humiditySensorCharacteristic.onDataReceived(onHumidityReceived, NULL);
    lightSensorCharacteristic.onDataReceived(onLightReceived, NULL);
    moistureSensorCharacteristic.onDataReceived(onMoistureReceived, NULL);

    rainsteamSensorCharacteristic.onDataReceived(onRainsteamReceived, NULL);
    liquidSensorCharacteristic.onDataReceived(onLiquidLevelReceived, NULL);
    humanDetectorCharacteristic.onDataReceived(onHumanDetectorReceived, NULL);
    // commented out, as this value sends from here rather than receives
    // solenoidVoltageCharacteristic.onDataReceived(onSolenoidReceived, NULL);

    //setup particle functions
    // Particle.function("sprinklerSwitch",sprinklerSwitch);
    // Particle.function("forceMqttPublish", forceMqttPublish);
    // Particle.function("setLowSoilMoistureThreshold",setLowSoilMoistureThreshold);
    // Particle.function("setHighSoilMoistureThreshold",setHighSoilMoistureThreshold);
    // Particle.function("setTemperatureThreshold",setTemperatureThreshold);
    // Particle.function("setAirHumidityThreshold",setAirHumidityThreshold);
    // Particle.function("setSunnyLightThreshold",setSunnyLightThreshold);
}

void loop() {
    //TEST
    // client.publish("elec4740g6/data","test"); 

    //do stuff if both sensors have been connected
    if ((sensorNode1.connected()) && (sensorNode2.connected())) {   //Add this back in when required!
        //record start time of this loop
        loopStart = millis();
        if(isWatering == false)
        {
            if((currentMoisture < LOW_SOIL_MOISTURE_THRESHOLD)    
            && (currentLight > SUNNY_LIGHT_THRESHOLD)
            && (currentTemperature > TEMPERATURE_THRESHOLD)
            && (currentHumanDetect == 0x00)
            )
            {
                switchSprinkler();
            }
        }
        if(isWatering)
        {
            if ((currentHumanDetect == 0x01)
            || (currentRainsteam > 80) //Needs to be replaced with correct values.
            || (currentMoisture > HIGH_SOIL_MOISTURE_THRESHOLD)
            || (currentHumidity > AIR_HUMIDITY_THRESHOLD)
            )
            {
                switchSprinkler();
            }
        }
        //check if it's time for an MQTT publish
        if(loopStart - lastPublishTime >= PUBLISH_DELAY){
            lastPublishTime = loopStart;
            //test mqtt connection
            if(client.connected()){
                Log.info("@@@@@@@@@@@@@@@@@@@@@@@@@@MQTT is connected!");
            }
            else{
                Log.info("@@@@@@@@@@@@@@@@@@@@@@@@@@@@MQTT not connected.");
                uint8_t server[] = {192, 168, 1, 1};
                if (client.connect(/*"test.mosquitto.org"*/server, 1883, "elec4740g6client") && client.awaitPackets()) {
                    client.publish("elec4740g6/test", "Hello world", strlen("Hello world"));
                    Log.info("@@@@@@@@@@@@@@@@@@@@@@@@@@MQTT connected succsessfully!");
                }
                else{
                    Log.info("@@@@@@@@@@@@@@@@@@@@@@@@@@@@MQTT connection failure :(");
                }
            }

            if(publishMqtt()){
                Log.info("============ mqtt publish successful.");
            }
            else{
                Log.info("============ mqtt publish failed.");
            }
        }

        //Sensor logic for watering
        //Change this to send when it changes not constantly
        // if(isWatering == false)
        // {
        //     //solenoidVoltageCharacteristic.setValue(0);
        // }
        // if(isWatering == true)
        // {
        //     //solenoidVoltageCharacteristic.setValue(1);

        // }
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

void switchSprinkler(){
    //turn sprinkler off if it was on
    if(isWatering){
        solenoidVoltageCharacteristic.setValue(0);
    }
    //alternatively, turn sprinkler on if it was off
    else{
        solenoidVoltageCharacteristic.setValue(1);
    }
    //record the time this switch occurred
    wateringEventTimes.push_back((int32_t) Time.now());
    //flip the watering flag
    isWatering = !isWatering;
}

bool publishMqtt(){
    Log.info("============Attempting to publish mqtt...");
    //initialise transmission buffer
    uint16_t payloadLength = 9+wateringEventTimes.size()*2;
    char buf[payloadLength];

    //add timestamp
    int32_t epochSeconds = Time.now();
    memcpy(buf, &epochSeconds, 4);

    //add sensor values
    //memcpy(buf+4, &currentMoisture, 1);
    //memcpy(buf+5, &currentLight, 1);
    //memcpy(buf+6, &currentTemperature, 1);
    //memcpy(buf+7, &currentHumidity, 1);
    
     buf[4] = currentMoisture;
     if (currentLight > SUNNY_LIGHT_THRESHOLD)
     {
        buf[5] = 1;
     }
     else
     {
        buf[5] = 0;
     }
     buf[6] = currentTemperature;
     buf[7] = currentHumidity;

    //add watering events' durations
    //memcpy(buf+8, &initWateringStatus, 1);
     buf[8] = initWateringStatus;
    
    /*
    //add sensor values
    buf[4] = (char) currentMoisture;
    buf[5] = (char) currentLight;
    buf[6] = (char) currentTemperature;
    buf[7] = (char) currentHumidity;

    //add watering events' durations
    buf[8] = (char) initWateringStatus;
    */
    for(uint i = 0; i < wateringEventTimes.size(); i ++){
        uint16_t duration = (uint16_t) wateringEventTimes.at(i) - epochSeconds;
        memcpy(buf+9+(2*i), &duration, sizeof(duration));
    }
    /*
    Log.info("currentMoisture: %d", currentMoisture);
    Log.info("currentLight: %d", currentLight);
    Log.info("currentTemperature: %d", currentTemperature);
    Log.info("currentHumidity: %d", currentHumidity);
    Log.info("initWateringStatus: %d", initWateringStatus);

    for(int i = 0; i < payloadLength; i++){
        Log.info("Byte %d: %u", i, buf[i]);
    }
    Log.info("buffer size: %d", strlen(buf));
    */
    //reset watering event log for next time period
    wateringEventTimes.clear();
    //save init watering status for next transmission
    if(isWatering){
        initWateringStatus = 1;
    }
    else{
        initWateringStatus = 0;
    }

    //publish buffer via MQTT
    return client.publish("elec4740g6/data", buf, payloadLength);
}

/* These functions are where we do something with the data (in bytes) we've received via bluetooth */

void onTemperatureReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    int8_t receivedTemp;
    
    //read the temp
    memcpy(&receivedTemp, &data[0], sizeof(receivedTemp));

    Log.info("Sensor 1 - Temperature: %u degrees Celsius", receivedTemp);
    currentTemperature = receivedTemp;

    //Log.info("Current Temp: %u", currentTemperature);
}

void onHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint8_t receivedHumidity;
    memcpy(&receivedHumidity, &data[0], sizeof(receivedHumidity));
    Log.info("Sensor 1 - Humidity: %u%%", receivedHumidity);
    currentHumidity = receivedHumidity;

    //Log.info("Current Humd: %u", currentHumidity);
}

void onMoistureReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the current sensor reading
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    
    Log.info("Sensor 1 - Soil moisture: %u", twoByteValue);
    currentMoisture = twoByteValue;

    //Log.info("Current moisture: %u", currentMoisture);
}

void onLightReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the current sensor reading
    int16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(twoByteValue));
    
    Log.info("Sensor 1 - Light: %u", twoByteValue);
    currentLight = twoByteValue;
    
    //Log.info("currentLight: %u", currentLight);
}

void onRainsteamReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    int8_t rainsteam;
    memcpy(&rainsteam, &data[0], sizeof(rainsteam));
    Log.info("Sensor 2 - Rainsteam: %d ", rainsteam);
}

void onLiquidLevelReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint16_t twoByteValue;

    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    Log.info("Sensor 2 - Liquid level: %u ", twoByteValue);
}


void onHumanDetectorReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint8_t humanSeen;

    memcpy(&humanSeen, &data[0], sizeof(uint8_t));
    currentHumanDetect = humanSeen;
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
}