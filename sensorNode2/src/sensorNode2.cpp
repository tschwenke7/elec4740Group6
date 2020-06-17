/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "d:/UoN/ELEC4470/Repo/elec4740Group6/sensorNode2/src/sensorNode2.ino"

#include "Particle.h"
#include "dct.h"
#include <chrono>

/*
 * sensorNode2.ino
 * Description: code to flash to the "sensor node 2" argon for assignment 2
 * Author: Tom Schwenke, Edward Ingle
 * Date: 20/05/2020
 */

void setup();
void loop();
void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
uint64_t getCurrentTime();
int8_t readRainsteamAna();
uint16_t readLiquid();
uint8_t readHumanDetector();
uint16_t readCurrent();
#line 13 "d:/UoN/ELEC4470/Repo/elec4740Group6/sensorNode2/src/sensorNode2.ino"
SYSTEM_MODE(SEMI_AUTOMATIC);     //In automatic mode so it can connect to cloud

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

/* Service UUID for sensor node 1. It is advertised as one service, 
   with 6 characteristics for each of its 5 sensors and 1 actuator */
const char* sensorNode2ServiceUuid("97728ad9-a998-4629-b855-ee2658ca01f7");


/*rainsteam sensor variables */
const int rainsteamPin = A4; //pin reading output of rainsteam sensor
//duration in millis to wait between reads
const uint16_t RAINSTEAM_READ_DELAY = 1000;
unsigned long lastRainsteamUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* rainsteamSensorUuid("bc7f18d9-2c43-408e-be25-62f40645987c");
BleCharacteristic rainsteamSensorCharacteristic("rainsteam",
BleCharacteristicProperty::NOTIFY, rainsteamSensorUuid, sensorNode2ServiceUuid);

/* liquid sensor variables */
const int liquidPin = A5;//A2; //pin reading output of sensor
//duration in millis to wait between reads
const uint16_t LIQUID_READ_DELAY = 1000;
unsigned long lastLiquidUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* liquidSensorUuid("88ba2f5d-1e98-49af-8697-d0516df03be9");
BleCharacteristic liquidSensorCharacteristic("liquid",
BleCharacteristicProperty::NOTIFY, liquidSensorUuid, sensorNode2ServiceUuid);

/* Human detection sensor variables */
const int humanDetectorPin = D3; //pin reading output of temp sensor
//duration in millis to wait between reads
const uint16_t HUMAN_DETECTOR_READ_DELAY = 2000;
unsigned long lastHumanDetectorUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* humanDetectorUuid("b482d551-c3ae-4dde-b125-ce244d7896b0");
BleCharacteristic humanDetectorCharacteristic("pir",
BleCharacteristicProperty::NOTIFY, humanDetectorUuid, sensorNode2ServiceUuid);
uint8_t lastHumandDetectorValue = 0;

/* Solenoid actuator characteristic */
const int solenoidPin = D4; 
//advertised bluetooth characteristic
const char* solenoidVoltageUuid("97017674-9615-4fba-9712-6829f2045836");
BleCharacteristic solenoidVoltageCharacteristic("ledVoltage",
BleCharacteristicProperty::WRITE_WO_RSP, solenoidVoltageUuid, sensorNode2ServiceUuid, onDataReceived);


/*debug variables */
double rainsteamCloud = 0;
double lightCloud = 0;
double liquidCloud = 0;
double humanDetectorCloud = 0;

/* Initial setup */
void setup() {
    const uint8_t val = 0x01;
    dct_write_app_data(&val, DCT_SETUP_DONE_OFFSET, 1);
    (void)logHandler; // Does nothing, just to eliminate the unused variable warning
    
    /* setup debug variables */
    Particle.variable("rainsteam", rainsteamCloud);
    Particle.variable("light", lightCloud);
    Particle.variable("liquid", liquidCloud);
    Particle.variable("humanDetector", humanDetectorCloud);
    
    /* Setup bluetooth characteristics and advertise sensorNode1Service to be connected to by the clusterhead */
    BLE.on();//activate BT

    //add characteristics
    BLE.addCharacteristic(rainsteamSensorCharacteristic);
    BLE.addCharacteristic(liquidSensorCharacteristic);
    BLE.addCharacteristic(humanDetectorCharacteristic);
    BLE.addCharacteristic(solenoidVoltageCharacteristic);

    //data to be advertised
    BleAdvertisingData advData;
    advData.appendServiceUUID(sensorNode2ServiceUuid);

    // Continuously advertise when not connected to clusterhead
    Log.info("Start advertising");
    BLE.advertise(&advData);

    //configure pins for input/output
    pinMode(humanDetectorPin, INPUT);
    //pinMode(ledPin, OUTPUT);
    pinMode(solenoidPin, OUTPUT);
}

void loop() {
    //only begin using sensors when this node has connected to a cluster head
    if(true){   //BLE.connected()){

        digitalWrite(solenoidPin, HIGH);        //Should write high to the solenoid pin

        long currentTime = millis();//record current time
        /* Check if it's time to take another reading for each sensor 
           If it is, update "lastUpdate" time, then read and update the appropriate characteristic
           A change in the characteristic will notify the connected cluster head
        */
        //rainsteam
        if(currentTime - lastRainsteamUpdate >= RAINSTEAM_READ_DELAY){
            lastRainsteamUpdate = currentTime;
            int8_t getValue = readRainsteamAna();
            rainsteamSensorCharacteristic.setValue(getValue);

            //store data in buffer
            uint8_t* transmission[9];
            memcpy(transmission, &getValue, sizeof(getValue));
            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            memcpy(transmission + sizeof(getValue), &sendTime, sizeof(sendTime));

            //send bluetooth transmission
            liquidSensorCharacteristic.setValue(transmission);

            //log reading
            rainsteamCloud = getValue;
            Log.info("Rain steam: %u", getValue);
        }
        //liquid
        if(currentTime - lastLiquidUpdate >= LIQUID_READ_DELAY){
            lastLiquidUpdate = currentTime;
            uint16_t getValue = readLiquid();

            //store data in buffer
            uint8_t* transmission[10];
            memcpy(transmission, &getValue, sizeof(getValue));
            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            memcpy(transmission + sizeof(getValue), &sendTime, sizeof(sendTime));

            //send bluetooth transmission
            liquidSensorCharacteristic.setValue(transmission);

            //log reading
            liquidCloud = getValue;
            Log.info("liquid: %u", getValue);
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

/** Function called whenver a value for ledVoltageCharacteristic is received via bluetooth.
 *  Updates the voltage supplied to the LED actuator to the received value */
void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the 2-byte value to set the fan pin adc to
    uint8_t ledVoltage;
    memcpy(&ledVoltage, &data[0], sizeof(uint8_t));

    Log.info("The LED voltage has been set via BT to %u", ledVoltage);

    //set the PWM output to the LED
    //analogWrite(ledPin, ledVoltage, ledHz);

}

/** Returns the current rainsteam in microseconds */
uint64_t getCurrentTime(){
    return Time.now();
    // return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/* Read the value on the rainsteam sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
int8_t readRainsteamAna(){
    // Read rainsteam pin
	uint16_t t = analogRead(rainsteamPin);

    //cloud debug stuff - can delete after testing
	char str[2];
	sprintf(str, "%u", t);
	Particle.publish("Rainsteam Analog", str, PUBLIC);
	
    //convert pin value to celsius
	//int8_t degC = (int8_t) t*0.08 - 273;
    Log.info("Read Rainsteam Analog: %d raw output, not degrees celsius", t);

	//return degC;
    return t;
}

/* Read the value on the liquid sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint16_t readLiquid(){
    uint16_t getS = analogRead(liquidPin);

    Log.info("Read liquid: %u", getS);

    //cloud debug stuff - can delete after testing
	char str[2];
	sprintf(str, "%u", getS);
	Particle.publish("liquid", str, PUBLIC);
	
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

/** Function called whenver a value for solenoidVoltageCharacteristic is received via bluetooth.
 *  Updates the voltage supplied to the LED actuator to the received value */
/*
void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the 2-byte value to set the fan pin adc to
    uint8_t solenoidVoltage;
    memcpy(&solenoidVoltage, &data[0], sizeof(uint8_t));

    Log.info("The LED voltage has been set via BT to %u", solenoidVoltage);

    //set the PWM output to the LED
    //analogWrite(solenoidPin, ledVoltage, ledHz);
}
*/