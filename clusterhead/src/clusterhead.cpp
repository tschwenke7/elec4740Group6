/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "c:/Users/tschw/repos/elec4740Group6/clusterhead/src/clusterhead.ino"
#include "Particle.h"
#include "dct.h"
/*
 * clusterhead.ino
 * Description: code to flash to the "clusterhead" argon for assignment 1
 * Author: Tom Schwenke
 * Date: 15/04/2020
 */

// This example does not require the cloud so you can run it in manual mode or
// normal cloud-connected mode
void setup();
void loop();
void onTempAndHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onLightReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onDistanceReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onTemperatureReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onLightReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onSoundReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onHumanDetectorReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
#line 12 "c:/Users/tschw/repos/elec4740Group6/clusterhead/src/clusterhead.ino"
SYSTEM_MODE(AUTOMATIC);

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

//bluetooth devices we want to connect to and their service ids
BlePeerDevice sensorNode1; //"754ebf5e-ce31-4300-9fd5-a8fb4ee4a811"
BlePeerDevice sensorNode2;
BleUuid sensorNode1ServiceUuid("754ebf5e-ce31-4300-9fd5-a8fb4ee4a811");
BleUuid sensorNode2ServiceUuid("97728ad9-a998-4629-b855-ee2658ca01f7");

//characteristics we want to track
//for sensor node 1
BleCharacteristic tempAndHumiditySensorCharacteristic;
BleCharacteristic lightSensorCharacteristic1;
BleCharacteristic distanceSensorCharacteristic;

//for sensor node 2
BleCharacteristic temperatureSensorCharacteristic2;
BleCharacteristic lightSensorCharacteristic2;
BleCharacteristic soundSensorCharacteristic;
BleCharacteristic humanDetectorCharacteristic;

// void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
const size_t SCAN_RESULT_MAX = 30;
BleScanResult scanResults[SCAN_RESULT_MAX];

void setup() {
    const uint8_t val = 0x01;
    dct_write_app_data(&val, DCT_SETUP_DONE_OFFSET, 1);
    (void)logHandler; // Does nothing, just to eliminate the unused variable warning

    BLE.on();
    
    //map functions to be called whenever new data is received for a characteristic
    tempAndHumiditySensorCharacteristic.onDataReceived(onTempAndHumidityReceived, NULL);
    lightSensorCharacteristic1.onDataReceived(onLightReceived1, NULL);
    distanceSensorCharacteristic.onDataReceived(onDistanceReceived, NULL);
    temperatureSensorCharacteristic2.onDataReceived(onTemperatureReceived2, NULL);
    lightSensorCharacteristic2.onDataReceived(onLightReceived2, NULL);
    soundSensorCharacteristic.onDataReceived(onSoundReceived, NULL);
    humanDetectorCharacteristic.onDataReceived(onHumanDetectorReceived, NULL);
}

void loop() { 
    //do stuff if both sensors have been connected
    if (sensorNode1.connected() && sensorNode2.connected()) {
        //do stuff here
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
                        sensorNode1.getCharacteristicByUUID(lightSensorCharacteristic1, "ea5248a4-43cc-4198-a4aa-79200a750835");
                        sensorNode1.getCharacteristicByUUID(tempAndHumiditySensorCharacteristic, "99a0d2f9-1cfa-42b3-b5ba-1b4d4341392f");
                        sensorNode1.getCharacteristicByUUID(distanceSensorCharacteristic, "45be4a56-48f5-483c-8bb1-d3fee433c23c");
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
void onTempAndHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //split the first two bytes into temperature and humidity
    uint8_t receivedTemp;
    uint8_t receivedHumidity;
    
    memcpy(&receivedTemp, &data[0], sizeof(receivedTemp));
    memcpy(&receivedHumidity, &data[0] + sizeof(receivedTemp), sizeof(receivedHumidity));
    
    Log.info("Sensor 1 - Temperature: %u", receivedTemp);
    Log.info("Sensor 1 - Humidity: %u", receivedHumidity);
}
void onLightReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    Log.info("Sensor 1 - Light: %u Lux", twoByteValue);
}
void onHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    Log.info("Sensor 1 - Humidity: %u", twoByteValue);
}
void onDistanceReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint8_t byteValue;
    memcpy(&byteValue, &data[0], sizeof(uint8_t));
    Log.info("Sensor 1 - Distance: %u cm", byteValue);
}
void onTemperatureReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    Log.info("Sensor 2 - Temperature: %u", twoByteValue);
}
void onLightReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    Log.info("Sensor 2 - Light: %u", twoByteValue);
}
void onSoundReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    Log.info("Sensor 2 - Sound: %u", twoByteValue);
}
void onHumanDetectorReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint16_t humanSeen;
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
}