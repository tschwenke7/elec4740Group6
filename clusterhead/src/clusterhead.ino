#include "Particle.h"
#include "dct.h"
#include <chrono>
/*
 * clusterhead.ino
 * Description: code to flash to the "clusterhead" argon for assignment 1
 * Author: Tom Schwenke
 * Date: 07/05/2020
 */

// This example does not require the cloud so you can run it in manual mode or
// normal cloud-connected mode
SYSTEM_MODE(AUTOMATIC);

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

/* Data tracking variables */

/* Alarm variables */

//The active status of each of the 4 possible alarm conditions. True if active, false if inactive
boolean alarmActive [4] = {false, false, false, false};
/* The absolute time in seconds each of the 4 alarms was activated.*/
long long alarmActivatedTimes[4] = {0,0,0,0};
//after this number of seconds without another alarm-worthy event, the alarm will automatically reset.
const uint8_t ALARM_COOLOFF_DELAY;
//number of seconds since each alarm stopped being triggered. If one reaches ALARM_COOLOFF_DELAY, reset that alarm
uint8_t alarmCooloffCounters [4] = {0, 0, 0, 0};

//alarm 1 will be triggered if an object is detected within this many centimetres
const int DISTANCE_THRESHOLD = 25;
//light must go below this value of Lux to trigger alarms 2 or 3
const int LIGHT_THRESHOLD = 100;
//no alarm below t0, alarm 2 may trigger between t0-t1, alarm 3 between t1-t2, alarm 4 if > t2
const int SOUND_VOLUME_THRESHOLDS [3] = {55, 70, 80};
//sound must continue for t0 seconds to trigger alarm 2, or t1 seconds for alarm 3
const int SOUND_DURATION_THRESHOLDS [2] = {30, 10};


/* Bluetooth variables */
//bluetooth devices we want to connect to and their service ids
BlePeerDevice sensorNode1;
BlePeerDevice sensorNode2;
BleUuid sensorNode1ServiceUuid("754ebf5e-ce31-4300-9fd5-a8fb4ee4a811");
BleUuid sensorNode2ServiceUuid("97728ad9-a998-4629-b855-ee2658ca01f7");

//characteristics we want to track
//for sensor node 1
BleCharacteristic temperatureSensorCharacteristic1;
BleCharacteristic humiditySensorCharacteristic;
BleCharacteristic distanceSensorCharacteristic;
BleCharacteristic currentSensorCharacteristic1;
BleCharacteristic fanSpeedCharacteristic;

//for sensor node 2
BleCharacteristic temperatureSensorCharacteristic2;
BleCharacteristic lightSensorCharacteristic2;
BleCharacteristic soundSensorCharacteristic;
BleCharacteristic humanDetectorCharacteristic;
BleCharacteristic currentSensorCharacteristic2;
BleCharacteristic ledVoltageCharacteristic;

// void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
const size_t SCAN_RESULT_MAX = 30;
BleScanResult scanResults[SCAN_RESULT_MAX];

void setup() {
    const uint8_t val = 0x01;
    dct_write_app_data(&val, DCT_SETUP_DONE_OFFSET, 1);
    (void)logHandler; // Does nothing, just to eliminate the unused variable warning

    BLE.on();
    
    //map functions to be called whenever new data is received for a characteristic
    temperatureSensorCharacteristic1.onDataReceived(onTemperatureReceived1, NULL);
    humiditySensorCharacteristic.onDataReceived(onHumidityReceived, NULL);
    distanceSensorCharacteristic.onDataReceived(onDistanceReceived, NULL);
    currentSensorCharacteristic1.onDataReceived(onCurrentReceived1, NULL);

    temperatureSensorCharacteristic2.onDataReceived(onTemperatureReceived2, NULL);
    lightSensorCharacteristic2.onDataReceived(onLightReceived2, NULL);
    soundSensorCharacteristic.onDataReceived(onSoundReceived, NULL);
    humanDetectorCharacteristic.onDataReceived(onHumanDetectorReceived, NULL);
    currentSensorCharacteristic2.onDataReceived(onCurrentReceived2, NULL);
}

void loop() { 
    //do stuff if both sensors have been connected
    if (sensorNode1.connected() && sensorNode2.connected()) {
        //monitor alarms
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
                        sensorNode1.getCharacteristicByUUID(temperatureSensorCharacteristic1, "bc7f18d9-2c43-408e-be25-62f40645987c");
                        sensorNode1.getCharacteristicByUUID(humiditySensorCharacteristic, "99a0d2f9-1cfa-42b3-b5ba-1b4d4341392f");
                        sensorNode1.getCharacteristicByUUID(distanceSensorCharacteristic, "45be4a56-48f5-483c-8bb1-d3fee433c23c");
                        sensorNode1.getCharacteristicByUUID(currentSensorCharacteristic1, "2822a610-32d6-45e1-b9fb-247138fc8df7");
                        sensorNode1.getCharacteristicByUUID(fanSpeedCharacteristic, "29fba3f5-4ce8-46bc-8d75-77806db22c31");
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
                        sensorNode2.getCharacteristicByUUID(temperatureSensorCharacteristic2, "bc7f18d9-2c43-408e-be25-62f40645987c");
                        sensorNode2.getCharacteristicByUUID(lightSensorCharacteristic2, "ea5248a4-43cc-4198-a4aa-79200a750835");
                        sensorNode2.getCharacteristicByUUID(soundSensorCharacteristic, "88ba2f5d-1e98-49af-8697-d0516df03be9");
                        sensorNode2.getCharacteristicByUUID(humanDetectorCharacteristic, "b482d551-c3ae-4dde-b125-ce244d7896b0");
                        sensorNode2.getCharacteristicByUUID(currentSensorCharacteristic2, "2822a610-32d6-45e1-b9fb-247138fc8df7");
                        sensorNode2.getCharacteristicByUUID(ledVoltageCharacteristic, "97017674-9615-4fba-9712-6829f2045836");

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

/* Functions to control the functionality of clusterhead actuators */

/* Alarm triggered by 
void startAlarm1(){
    //Blue LED flashing, 0.5 Hz frequency
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
    // Log.info("Temp/humidity transmission delay: %llu seconds", calculateTransmissionDelay(sentTime));
}

void onHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint8_t receivedHumidity;
    memcpy(&receivedHumidity, &data[0], sizeof(receivedHumidity));
    Log.info("Sensor 1 - Humidity: %u%%", receivedHumidity);
}

void onCurrentReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the current sensor reading
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    
    Log.info("Sensor 1 - Current: %u Amps", twoByteValue);
}

void onCurrentReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the current sensor reading
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    
    Log.info("Sensor 2 - Current: %u Amps", twoByteValue);
}

void onDistanceReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint8_t byteValue;
    uint64_t sentTime;

    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(byteValue), sizeof(sentTime));

    memcpy(&byteValue, &data[0], sizeof(uint8_t));
    Log.info("Sensor 1 - Distance: %u cm", byteValue);
    // Log.info("Transmission delay: %llu seconds", calculateTransmissionDelay(sentTime));
}

void onTemperatureReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    int8_t temperature;
    uint64_t sentTime;

    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(temperature), sizeof(sentTime));

    memcpy(&temperature, &data[0], sizeof(temperature));
    Log.info("Sensor 2 - Temperature: %d degrees Celsius", temperature);
    // Log.info("Transmission delay: %llu seconds", calculateTransmissionDelay(sentTime));
}

void onLightReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint16_t twoByteValue;
    uint64_t sentTime;

    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(twoByteValue), sizeof(sentTime));

    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    Log.info("Sensor 2 - Light: %u Lux", twoByteValue);
    // Log.info("Transmission delay: %llu seconds", calculateTransmissionDelay(sentTime));
}

void onSoundReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint16_t twoByteValue;
    uint64_t sentTime;

    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(twoByteValue), sizeof(sentTime));

    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    Log.info("Sensor 2 - Sound: %u dB", twoByteValue);
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

uint64_t calculateTransmissionDelay(uint64_t sentTime){
    return Time.now() - sentTime;
    // return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - sentTime;
}