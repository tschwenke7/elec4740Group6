
#include "Particle.h"
#include "dct.h"
#include <chrono>

/*
 * sensorNode2.ino
 * Description: code to flash to the "sensor node 2" argon for assignment 2
 * Author: Tom Schwenke, Edward Ingle
 * Date: 20/05/2020
 */

SYSTEM_MODE(AUTOMATIC);     //In automatic mode so it can connect to cloud

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

/* Service UUID for sensor node 1. It is advertised as one service, 
   with 6 characteristics for each of its 5 sensors and 1 actuator */
const char* sensorNode2ServiceUuid("97728ad9-a998-4629-b855-ee2658ca01f7");


/*Temperature sensor variables */
const int temperaturePin = A0; //pin reading output of temp sensor
//duration in millis to wait between reads
const uint16_t TEMPERATURE_READ_DELAY = 30000;
unsigned long lastTemperatureUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* temperatureSensorUuid("bc7f18d9-2c43-408e-be25-62f40645987c");
BleCharacteristic temperatureSensorCharacteristic("temp",
BleCharacteristicProperty::NOTIFY, temperatureSensorUuid, sensorNode2ServiceUuid);

/* Light sensor variables */
const int lightPin = A5; //pin reading output of sensor
//duration in millis to wait between reads
const uint16_t LIGHT_READ_DELAY = 5000;
unsigned long lastLightUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* lightSensorUuid("ea5248a4-43cc-4198-a4aa-79200a750835");
BleCharacteristic lightSensorCharacteristic("light",
BleCharacteristicProperty::NOTIFY, lightSensorUuid, sensorNode2ServiceUuid);

/* Sound sensor variables */
const int soundPin = A4;//A2; //pin reading output of sensor
//duration in millis to wait between reads
const uint16_t SOUND_READ_DELAY = 5000;
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

/* Current sensor characteristic */
const int currentSensorPin = -1; //TODO: update this
const uint16_t CURRENT_READ_DELAY = 5000;//time between sampling current
unsigned long lastCurrentUpdate = 0;
//advertised bluetooth characteristic
const char* currentSensorUuid("2822a610-32d6-45e1-b9fb-247138fc8df7");
BleCharacteristic currentSensorCharacteristic("current",
BleCharacteristicProperty::NOTIFY, currentSensorUuid, sensorNode2ServiceUuid);

/* LED actuator characteristic */
const int ledPin = -1; //TODO: update this
const uint ledHz= 50;
//advertised bluetooth characteristic
const char* ledVoltageUuid("97017674-9615-4fba-9712-6829f2045836");
BleCharacteristic ledVoltageCharacteristic("ledVoltage",
BleCharacteristicProperty::WRITE_WO_RSP, ledVoltageUuid, sensorNode2ServiceUuid, onDataReceived);

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
    BLE.addCharacteristic(currentSensorCharacteristic);
    BLE.addCharacteristic(ledVoltageCharacteristic);

    //data to be advertised
    BleAdvertisingData advData;
    advData.appendServiceUUID(sensorNode2ServiceUuid);

    // Continuously advertise when not connected to clusterhead
    Log.info("Start advertising");
    BLE.advertise(&advData);

    //configure pins for input/output
    pinMode(humanDetectorPin, INPUT);
    pinMode(ledPin, OUTPUT);
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
        //current
        if(currentTime - lastCurrentUpdate >= CURRENT_READ_DELAY){
           lastCurrentUpdate = currentTime;
           uint16_t current = readCurrent();

           //send bluetooth transmission
           currentSensorCharacteristic.setValue(current);
        }

        delay(100);
    }
    else{
        Log.info("not connected yet... ");
        delay(500);
    }
}

/** Function called whenver a value for ledVoltageCharacteristic is received via bluetooth.
 *  Updates the voltage supplied to the LED actuator to the received value */
void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the 2-byte value to set the fan pin adc to
    uint16_t ledVoltage;
    memcpy(&ledVoltage, &data[0], sizeof(uint16_t));

    Log.info("The LED voltage has been set via BT to %u", ledVoltage);

    if(ledVoltage < 4095){
        //set the PWM output to the LED
        analogWrite(ledPin, ledVoltage, ledHz);
    }
    else{
        Log.info("Invalid LED voltage - should be less than 4095.");
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
    // Read temperature pin
	uint16_t t = analogRead(temperaturePin);

    //cloud debug stuff - can delete after testing
	char str[2];
	sprintf(str, "%u", t);
	Particle.publish("temperatureAna", str, PUBLIC);
	
    //convert pin value to celsius
	int8_t degC = (int8_t) t*0.08 - 273;
    Log.info("Read temperature: %d degrees celsius", degC);

	return degC;
}

/* Read the value on the light sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint16_t readLight(){
    //do any transformation logic we might want
    uint16_t getL = analogRead(lightPin);

    //cloud debug stuff - can delete after testing
	char str[2];
	sprintf(str, "%u", getL);
	Particle.publish("light", str, PUBLIC);
    
    //convert to lux
	uint16_t lux =  (uint16_t) (getL - 1382.758621)/3.793103448 + 30;
    Log.info("Read light: %u lux", lux);
    return lux;
}

/* Read the value on the sound sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint16_t readSound(){
    uint16_t getS = analogRead(soundPin);

    Log.info("Read sound: %u", getS);

    //cloud debug stuff - can delete after testing
	char str[2];
	sprintf(str, "%u", getS);
	Particle.publish("sound", str, PUBLIC);
	
    return getS;
}

/* Reads the PIR sensor. Returns 1 if signal is HIGH, 0 if LOW */
uint8_t readHumanDetector(){
    byte state = digitalRead(humanDetectorPin);

    Log.info("Read PIR: %u", state);

    //cloud debug stuff - can delete after testing
	char str[1];
	sprintf(str, "%u", state);
	Particle.publish("humanDetector", str, PUBLIC);

    return (uint8_t) state;
}

/* Read the value on the current sensor pin */
uint16_t readCurrent(){
    //TODO: work out how to actually read this
    uint16_t current = 0;

    //cloud data - can delete when not testing
    char str[2];
    sprintf(str, "%u", current);
	Particle.publish("Current (not currently implemented)", str, PUBLIC);

    Log.info("Read current (not currently implemented): %u", current);
    return current;
}