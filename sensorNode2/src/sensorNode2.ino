#include "Particle.h"
/*
 * sensorNode2.ino
 * Description: code to flash to the "sensor node 2" argon for assignment 1
 * Author: Tom Schwenke
 * Date: 15/04/2020
 */

// This example does not require the cloud so you can run it in manual mode or
// normal cloud-connected mode
SYSTEM_MODE(MANUAL);

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

/* Service UUID for sensor node 1. It is advertised as one service, 
   with 4 characteristics for each of its 4 sensors */
const char* sensorNode2ServiceUuid("97728ad9-a998-4629-b855-ee2658ca01f7");


/*Temperature sensor variables */
const int temperaturePin = A0; //pin reading output of temp sensor
//duration in millis to wait between reads
const uint16_t TEMPERATURE_READ_DELAY = 5000;
unsigned long lastTemperatureUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* temperatureSensorUuid("bc7f18d9-2c43-408e-be25-62f40645987c");
BleCharacteristic temperatureSensorCharacteristic("temp",
BleCharacteristicProperty::NOTIFY, temperatureSensorUuid, sensorNode2ServiceUuid);


/* Light sensor variables */
const int lightPin = A1; //pin reading output of sensor
//duration in millis to wait between reads
const uint16_t LIGHT_READ_DELAY = 5000;
unsigned long lastLightUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* lightSensorUuid("ea5248a4-43cc-4198-a4aa-79200a750835");
BleCharacteristic lightSensorCharacteristic("light",
BleCharacteristicProperty::NOTIFY, lightSensorUuid, sensorNode2ServiceUuid);

/* Sound sensor variables */
const int soundPin = A2; //pin reading output of sensor
//duration in millis to wait between reads
const uint16_t SOUND_READ_DELAY = 5000;
unsigned long lastSoundUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* soundSensorUuid("88ba2f5d-1e98-49af-8697-d0516df03be9");
BleCharacteristic soundSensorCharacteristic("sound",
BleCharacteristicProperty::NOTIFY, soundSensorUuid, sensorNode2ServiceUuid);

/* Distance sensor variables */
const int humanDetectorPin = D0; //pin reading output of temp sensor
//duration in millis to wait between reads
const uint16_t HUMAN_DETECTOR_READ_DELAY = 5000;
unsigned long lastHumanDetectorUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* humanDetectorUuid("b482d551-c3ae-4dde-b125-ce244d7896b0");
BleCharacteristic humanDetectorCharacteristic("pir",
BleCharacteristicProperty::NOTIFY, humanDetectorUuid, sensorNode2ServiceUuid);
uint8_t lastHumandDetectorValue = 0;


/*debug variables */
double temperatureCloud = 0;
double lightCloud = 0;
double soundCloud = 0;
double humanDetectorCloud = 0;

/* Initial setup */
void setup() {
    
    (void)logHandler; // Does nothing, just to eliminate the unused variable warning
    
    /* setup debug variables */
    Particle.variable("temperature", temperatureCloud);
    Particle.variable("light", lightCloud);
    Particle.variable("sound", soundCloud);
    Particle.variable("humanDetector", humanDetectorCloud);
    
    /* Setup bluetooth characteristics and advertise sensorNode1Service to be connected to by the clusterhead */
    BLE.on();//activate BT

    //add characteristics
    BLE.addCharacteristic(temperatureSensorCharacteristic);
    BLE.addCharacteristic(lightSensorCharacteristic);
    BLE.addCharacteristic(soundSensorCharacteristic);
    BLE.addCharacteristic(humanDetectorCharacteristic);

    //data to be advertised
    BleAdvertisingData advData;
    advData.appendServiceUUID(sensorNode2ServiceUuid);

    // Continuously advertise when not connected to clusterhead
    Log.info("Start advertising");
    BLE.advertise(&advData);
}

void loop() {
    //only begin using sensors when this node has connected to a cluster head
    if(BLE.connected()){
        long currentTime = millis();//record current time
        /* Check if it's time to take another reading for each sensor 
           If it is, update "lastUpdate" time, then read and update the appropriate characteristic
           A change in the characteristic will notify the connected cluster head
        */
        //temperature
        if(currentTime - lastTemperatureUpdate >= TEMPERATURE_READ_DELAY){
            lastTemperatureUpdate = currentTime;
            uint16_t getValue = readTemperature();
            temperatureSensorCharacteristic.setValue(getValue);
            // temperatureCloud = getValue;
            Log.info("Temperature: %u", getValue);
        }
        //light
        if(currentTime - lastLightUpdate >= LIGHT_READ_DELAY){
            lastLightUpdate = currentTime;
            uint16_t getValue = readLight();
            lightSensorCharacteristic.setValue(getValue);
            lightCloud = getValue;
            Log.info("Light: %u", getValue);
        }
        //sound
        if(currentTime - lastSoundUpdate >= SOUND_READ_DELAY){
            lastSoundUpdate = currentTime;
            uint16_t getValue = readSound();
            soundSensorCharacteristic.setValue(getValue);
            soundCloud = getValue;
            Log.info("Sound: %u", getValue);
        }
        //human detector
        if(currentTime - lastHumanDetectorUpdate >= HUMAN_DETECTOR_READ_DELAY){
            lastHumanDetectorUpdate = currentTime;
            uint8_t getValue = readHumanDetector();
            Log.info("Previous Human detector state: %u", lastHumandDetectorValue);
            //only send an update if the value has changed since last read,
            //i.e. a human has been detected or lost
            if(getValue != lastHumandDetectorValue){
                humanDetectorCharacteristic.setValue(getValue);//send the value which was read
                humanDetectorCloud = getValue;//update cloud variable
                lastHumandDetectorValue = getValue;//update seen/unseen state
            }
            Log.info("Human detector: %u", getValue);
        }
    }
    else{
        Log.info("not connected yet... ");
        delay(500);
    }
}

/* Read the value on the temperature sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint16_t readTemperature(){
    //do any transformation logic we might want
    //return analogRead(temperaturePin);
    return 23;
}

/* Read the value on the light sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint16_t readLight(){
   //do any transformation logic we might want
   //return analogRead(lightPin);
   return 4053;
}

/* Read the value on the sound sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint16_t readSound(){
   //do any transformation logic we might want
   //return analogRead(soundPin);
   return 0x0010;
}

/* Reads the PIR sensor. Returns 1 if signal is HIGH, 0 if LOW */
uint8_t readHumanDetector(){
   //TODO @edward 
   //placeholder output value to use if sensor isnt connected
   if(lastHumandDetectorValue == 0){
       return 1;
   }
   else{
       return 0;
   }
}