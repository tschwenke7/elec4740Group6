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
    int8_t receivedTemp;
    uint8_t receivedHumidity;
    uint64_t sentTime;

    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(receivedTemp) + sizeof(receivedHumidity), sizeof(sentTime));
    uint64_t delay = calculateTransmissionDelay(sentTime);

    //split the first two bytes into temperature and humidity
    memcpy(&receivedTemp, &data[0], sizeof(receivedTemp));
    memcpy(&receivedHumidity, &data[0] + sizeof(receivedTemp), sizeof(receivedHumidity));

    //print full buffer
    char byteX;
    Log.info("@@@@Temp/humidity buffer contains:");
    for(int i = 0; i < 10; i++){
        memcpy(&byteX, &data[i], sizeof(byteX));
        Log.info("Byte %d : %x",i,byteX);
    }

    Log.info("Sensor 1 - Temperature: %d degrees Celsius", receivedTemp);
    Log.info("Sensor 1 - Humidity: %u %%", receivedHumidity);
    
    // Log.info("Temp/humidity transmission delay: %llu seconds", delay);
}

void onLightReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the light sensor reading
    uint16_t twoByteValue;
    uint64_t sentTime;

    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));

    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(twoByteValue), sizeof(sentTime));
    
    Log.info("Sensor 1 - Light: %u Lux", twoByteValue);
    // Log.info("Transmission delay: %llu seconds", calculateTransmissionDelay(sentTime));
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