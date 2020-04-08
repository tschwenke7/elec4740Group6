#include "Particle.h"
/*
 * clusterHead.ino
 * Description: code to flash to the clusterhead argon for assignment 1
 * Author: Tom Schwenke
 * Date: 08/04/2020
 */

// This example does not require the cloud so you can run it in manual mode or
// normal cloud-connected mode
// SYSTEM_MODE(MANUAL);

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

//bluetooth devices we want to connect to and their service ids
BlePeerDevice sensorNode1; //"754ebf5e-ce31-4300-9fd5-a8fb4ee4a811"
BlePeerDevice sensorNode2;
const char* sensorNode1ServiceUuid("754ebf5e-ce31-4300-9fd5-a8fb4ee4a811");
const char* sensorNode2ServiceUuid("97728ad9-a998-4629-b855-ee2658ca01f7");

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
    if (sensorNode1.connected() && sensorNode2.connected()) {
        //do stuff here
    }
    //if we haven't connected both, then scan for them
    else {
        // We are not connected to our sensors, scan for them
        int count = BLE.scan(scanResultCallback, NULL);
        if (count > 0) {
            Log.info("%d devices found", count);
        }
    }
}

/* function which executes while scanning on each bluetooth device which is discovered, to decide whether to conneect */
void scanResultCallback(const BleScanResult *scanResult, void *context) {
    //print info about the found bluetooth device
    Log.info("MAC: %02X:%02X:%02X:%02X:%02X:%02X | RSSI: %dBm",
            scanResult->address[0], scanResult->address[1], scanResult->address[2],
            scanResult->address[3], scanResult->address[4], scanResult->address[5], scanResult->rssi);

    String name = scanResult->advertisingData.deviceName();
    if (name.length() > 0) {
        Log.info("deviceName: %s", name.c_str());
    }
    
    /* Connect if it is one of our sensors */
    //After connecting, this is how long to wait without connection before determining that the other side is no longer available
    //Connection timeout is in units of 10 milliseconds. e.g. 1000 = 10 seconds 
    uint16_t timeoutInterval = 1000;
    //read the serviceUUID being advertised by this device
    BleUuid foundServiceUUID;
    size_t svcCount = scanResult->advertisingData.serviceUUID(&foundServiceUUID, 1);
    
    //check if it matches sensorNode1
    if (sensorNode1.connected() == false && svcCount > 0 && foundServiceUUID == sensorNode1ServiceUuid) {
        sensorNode1 = BLE.connect(scanResult->address,24 ,0, timeoutInterval);//24 and 0 are default values
        if(sensorNode1.connected()){
            Log.info("Successfully connected to sensor node 1!");
            //map characteristics from this service to the variables in this program, so they're handled by our "on<X>Received" functions
            sensorNode1.getCharacteristicByUUID(temperatureSensorCharacteristic1, "bc7f18d9-2c43-408e-be25-62f40645987c");
            sensorNode1.getCharacteristicByUUID(lightSensorCharacteristic1, "bc7f18d9-2c43-408e-be25-62f40645987c");
            sensorNode1.getCharacteristicByUUID(humiditySensorCharacteristic, "bc7f18d9-2c43-408e-be25-62f40645987c");
            sensorNode1.getCharacteristicByUUID(distanceSensorCharacteristic, "bc7f18d9-2c43-408e-be25-62f40645987c");
        }
        else{
            Log.info("Failed to connect to sensor node 1.");
        }
    }
    //check if it matches sensorNode2
    else if (sensorNode2.connected() == false && svcCount > 0 && foundServiceUUID == sensorNode2ServiceUuid) {
        sensorNode2 = BLE.connect(scanResult->address,24 ,0, timeoutInterval);
        if(sensorNode1.connected()){
            Log.info("Successfully connected to sensor node 2!");
        }
        else{
            Log.info("Failed to connect to sensor node 2.");
        }
    }
    else{
        Log.info("Ignored a bluetooth device which wasn't one of ours.");
    }
}

/* These functions are where we do something with the data (in bytes) we've received via bluetooth */
void onTemperatureReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){

}
void onLightReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    
}
void onHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    
}
void onDistanceReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    
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

