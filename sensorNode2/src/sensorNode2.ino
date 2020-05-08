
#include "Particle.h"
#include "dct.h"
#include <chrono>

/*
 * sensorNode2.ino
 * Description: code to flash to the "sensor node 2" argon for assignment 1
 * Author: Tom Schwenke, Edward Ingle
 * Date: 07/05/2020
 */

SYSTEM_MODE(AUTOMATIC);     //In automatic mode so it can connect to cloud

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

/* Service UUID for sensor node 1. It is advertised as one service, 
   with 4 characteristics for each of its 4 sensors */
const char* sensorNode2ServiceUuid("97728ad9-a998-4629-b855-ee2658ca01f7");


/*Temperature sensor variables */
const int temperaturePin = A0; //pin reading output of temp sensor
//duration in millis to wait between reads
const uint16_t TEMPERATURE_READ_DELAY = 2000;
unsigned long lastTemperatureUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* temperatureSensorUuid("bc7f18d9-2c43-408e-be25-62f40645987c");
BleCharacteristic temperatureSensorCharacteristic("temp",
BleCharacteristicProperty::NOTIFY, temperatureSensorUuid, sensorNode2ServiceUuid);

/* Light sensor variables */
const int lightPin = A5; //pin reading output of sensor
//duration in millis to wait between reads
const uint16_t LIGHT_READ_DELAY = 2000;
unsigned long lastLightUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* lightSensorUuid("ea5248a4-43cc-4198-a4aa-79200a750835");
BleCharacteristic lightSensorCharacteristic("light",
BleCharacteristicProperty::NOTIFY, lightSensorUuid, sensorNode2ServiceUuid);

/* Sound sensor variables */
const int soundPin = A4;//A2; //pin reading output of sensor
//duration in millis to wait between reads
const uint16_t SOUND_READ_DELAY = 2000;
unsigned long lastSoundUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* soundSensorUuid("88ba2f5d-1e98-49af-8697-d0516df03be9");
BleCharacteristic soundSensorCharacteristic("sound",
BleCharacteristicProperty::NOTIFY, soundSensorUuid, sensorNode2ServiceUuid);

/* Human Distance sensor variables */
const int humanDetectorPin = D4; //pin reading output of temp sensor
//duration in millis to wait between reads
const uint16_t HUMAN_DETECTOR_READ_DELAY = 2000;
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
    const uint8_t val = 0x01;
    dct_write_app_data(&val, DCT_SETUP_DONE_OFFSET, 1);
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

    pinMode(humanDetectorPin,INPUT);    
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
            int8_t getValue = readTemperatureAna();
            temperatureSensorCharacteristic.setValue(getValue);

            //store data in buffer
            uint8_t* transmission[9];
            memcpy(transmission, &getValue, sizeof(getValue));
            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            memcpy(transmission + sizeof(getValue), &sendTime, sizeof(sendTime));

            //send bluetooth transmission
            soundSensorCharacteristic.setValue(transmission);

            //log reading
            temperatureCloud = getValue;
            Log.info("Temperature: %u", getValue);
        }
        //light
        if(currentTime - lastLightUpdate >= LIGHT_READ_DELAY){
            lastLightUpdate = currentTime;
            uint16_t getValue = readLight();

            //store data in buffer
            uint8_t* transmission[10];
            memcpy(transmission, &getValue, sizeof(getValue));
            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            memcpy(transmission + sizeof(getValue), &sendTime, sizeof(sendTime));

            lightSensorCharacteristic.setValue(transmission);
            lightCloud = getValue;
            Log.info("Light: %u", getValue);
        }
        //sound
        if(currentTime - lastSoundUpdate >= SOUND_READ_DELAY){
            lastSoundUpdate = currentTime;
            uint16_t getValue = readSound();

            //store data in buffer
            uint8_t* transmission[10];
            memcpy(transmission, &getValue, sizeof(getValue));
            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            memcpy(transmission + sizeof(getValue), &sendTime, sizeof(sendTime));

            //send bluetooth transmission
            soundSensorCharacteristic.setValue(transmission);

            //log reading
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
                //store data in buffer
                uint8_t* transmission[9];
                memcpy(transmission, &getValue, sizeof(getValue));
                //record and append the sending time
                uint64_t sendTime = getCurrentTime();
                memcpy(transmission + sizeof(getValue), &sendTime, sizeof(sendTime));

                //send bluetooth transmission
                humanDetectorCharacteristic.setValue(transmission);//send the value which was read
                lastHumandDetectorValue = getValue;//update seen/unseen state

                //log reading
                humanDetectorCloud = getValue;//update cloud variable
            }
            Log.info("Human detector: %u", getValue);
        }
        delay(100);
    }
    else{
        Log.info("not connected yet... ");
        delay(500);
    }
}

/** Returns the current temperature in microseconds */
uint64_t getCurrentTime(){
    return Time.now();
    // return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/* Read the value on the temperature sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
int8_t readTemperatureAna(){
    // Read temperature as Celsius
	int8_t t = analogRead(temperaturePin);   //Normally returns float
	char str[2];
	sprintf(str, "%u", t);
	Particle.publish("temperatureAna", str, PUBLIC);
	
	int8_t degC = (int8_t) t*0.8 - 273;
	return degC;
}

/* Read the value on the light sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint16_t readLight(){
    //do any transformation logic we might want
    uint16_t getL = analogRead(lightPin);
	char str[2];
	sprintf(str, "%u", getL);
	Particle.publish("light", str, PUBLIC);
    
	uint16_t getLasLux =  (uint16_t) (getL - 1382.758621)/3.793103448;
    return getLasLux;
}

/* Read the value on the sound sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint16_t readSound(){
    uint16_t getS = analogRead(soundPin);
	char str[2];
	sprintf(str, "%u", getS);
	Particle.publish("sound", str, PUBLIC);
    return getS;
}

/* Reads the PIR sensor. Returns 1 if signal is HIGH, 0 if LOW */
uint8_t readHumanDetector(){
    byte state = digitalRead(humanDetectorPin);
	char str[1];
	sprintf(str, "%u", state);
	Particle.publish("humanDetector", str, PUBLIC);
    return (uint8_t) state;
}