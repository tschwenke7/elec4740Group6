#include "Particle.h"
/*
 * clusterhead.ino
 * Description: code to flash to the "clusterhead" argon for assignment 1
 * Author: Tom Schwenke
 * Date: 14/04/2020
 */

// This example does not require the cloud so you can run it in manual mode or
// normal cloud-connected mode
// SYSTEM_MODE(MANUAL);

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

//bluetooth devices we want to connect to and their service ids
BlePeerDevice sensorNode1; //"754ebf5e-ce31-4300-9fd5-a8fb4ee4a811"
BlePeerDevice sensorNode2;
BleUuid sensorNode1ServiceUuid("754ebf5e-ce31-4300-9fd5-a8fb4ee4a811");
BleUuid sensorNode2ServiceUuid("97728ad9-a998-4629-b855-ee2658ca01f7");

//characteristics we want to track
//for sensor node 1
BleCharacteristic temperatureSensorCharacteristic1;
BleCharacteristic lightSensorCharacteristic1;
BleCharacteristic humiditySensorCharacteristic;
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
    (void)logHandler; // Does nothing, just to eliminate the unused variable warning

    BLE.on();
    
    //map functions to be called whenever new data is received for a characteristic
    temperatureSensorCharacteristic1.onDataReceived(onTemperatureReceived1, NULL);
    lightSensorCharacteristic1.onDataReceived(onLightReceived1, NULL);
    humiditySensorCharacteristic.onDataReceived(onHumidityReceived, NULL);
    distanceSensorCharacteristic.onDataReceived(onDistanceReceived, NULL);
    temperatureSensorCharacteristic2.onDataReceived(onTemperatureReceived2, NULL);
    lightSensorCharacteristic2.onDataReceived(onLightReceived2, NULL);
    soundSensorCharacteristic.onDataReceived(onSoundReceived, NULL);
    humanDetectorCharacteristic.onDataReceived(onHumanDetectorReceived, NULL);
}

/* function which executes while scanning on each bluetooth device which is discovered, to decide whether to conneect */
void scanResultCallback(const BleScanResult *scanResult, void *context);

void loop() {
    //do stuff if both sensors have been connected
    if (sensorNode1.connected() /*&& sensorNode2.connected()*/) {
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
            Log.info("Target UUID: " + sensorNode1ServiceUuid.toString());

            //Check if it matches UUID for sensor node 1
            if (len > 0 && foundService == sensorNode1ServiceUuid){
                Log.info("Found sensor node 1.");
                if(sensorNode1.connected() == false){
                    sensorNode1 = BLE.connect(scanResults[i].address);
                    if(sensorNode1.connected()){
                        Log.info("Successfully connected to sensor node 1!");
                        //map characteristics from this service to the variables in this program, so they're handled by our "on<X>Received" functions
                        sensorNode1.getCharacteristicByUUID(temperatureSensorCharacteristic1, "bc7f18d9-2c43-408e-be25-62f40645987c");
                        sensorNode1.getCharacteristicByUUID(lightSensorCharacteristic1, "ea5248a4-43cc-4198-a4aa-79200a750835");
                        sensorNode1.getCharacteristicByUUID(humiditySensorCharacteristic, "99a0d2f9-1cfa-42b3-b5ba-1b4d4341392f");
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
        }

        if (count > 0) {
            Log.info("%d devices found", count);
        }
    }
}

/* These functions are where we do something with the data (in bytes) we've received via bluetooth */
void onTemperatureReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    Log.info("Temp1 received.");
}
void onLightReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    Log.info("Light1 received.");
}
void onHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    Log.info("Humididty received.");
}
void onDistanceReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    Log.info("Distance received.");
}
void onTemperatureReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    
}
void onLightReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    
}
void onSoundReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    
}
void onHumanDetectorReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    
}

// void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context) {
//     uint8_t flags = data[0];

//     uint16_t rate;
//     if (flags & 0x01) {
//         // Rate is 16 bits
//         memcpy(&rate, &data[1], sizeof(uint16_t));
//     }
//     else {
//         // Rate is 8 bits (normal case)
//         rate = data[1];
//     }
//     if (rate != lastRate) {
//         lastRate = rate;
//         updateDisplay = true;
//     }

//     Log.info("heart rate=%u", rate);
// }

