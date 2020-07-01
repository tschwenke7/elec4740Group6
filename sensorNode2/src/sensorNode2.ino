
#include "Particle.h"
#include "dct.h"
#include <chrono>

/*
 * sensorNode2.ino
 * Description: code to flash to the "sensor node 2" argon for assignment 3
 * Author: Tom Schwenke, Edward Ingle
 * Date: 20/05/2020
 */

SYSTEM_MODE(MANUAL);     //In automatic mode so it can connect to cloud

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

/* Service UUID for sensor node 1. It is advertised as one service, 
   with 6 characteristics for each of its 5 sensors and 1 actuator */
const char* sensorNode2ServiceUuid("97728ad9-a998-4629-b855-ee2658ca01f7");


/*rainsteam sensor variables */
const int rainsteamPin = A4; //pin reading output of rainsteam sensor
//duration in millis to wait between reads
const uint16_t RAINSTEAM_READ_DELAY = 5000;
unsigned long lastRainsteamUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* rainsteamSensorUuid("bc7f18d9-2c43-408e-be25-62f40645987c");
BleCharacteristic rainsteamSensorCharacteristic("rainsteam",
BleCharacteristicProperty::NOTIFY, rainsteamSensorUuid, sensorNode2ServiceUuid);
//Array of last recorded for short term averages
int16_t rainsteamArray[5];
int16_t rainsteamAssigned = 0;                                 //Tracks the number of assigned . When  assigned == Array.length, it sends the value to the clusterhead.
int16_t rainsteamArraySize = sizeof(rainsteamArray)/sizeof(rainsteamArray[0]); //Holds array size for readability.

/* liquid sensor variables */
const int liquidPin = A5;//A2; //pin reading output of sensor
//duration in millis to wait between reads
const uint16_t LIQUID_READ_DELAY = 5000;
unsigned long lastLiquidUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* liquidSensorUuid("88ba2f5d-1e98-49af-8697-d0516df03be9");
BleCharacteristic liquidSensorCharacteristic("liquid",
BleCharacteristicProperty::NOTIFY, liquidSensorUuid, sensorNode2ServiceUuid);
uint16_t getLiquid = -1;
//Array of last recorded for short term averages
int16_t liquidArray[5];
int16_t liquidAssigned = 0;                                 //Tracks the number of assigned . When  assigned == Array.length, it sends the value to the clusterhead.
int16_t liquidArraySize = sizeof(liquidArray)/sizeof(liquidArray[0]); //Holds array size for readability.

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
bool solenoidIsOn = false;  //True if solenoid is turned on, false otherwise.

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
        //TEST CODE : COMMENT OUT WHEN COMPLETE
        //
        /*
        if(getLiquid < 3200)
        {
            digitalWrite(solenoidPin, HIGH); 

        }
        else
        {
            digitalWrite(solenoidPin, LOW); 

        }
        */
        //Log.info("Solenoid Pin: %b", digitalRead(solenoidPin));
        
        if(solenoidIsOn)
        {
            digitalWrite(solenoidPin, HIGH);        //Should write high to the solenoid pin
        }
        else
        {
            digitalWrite(solenoidPin, LOW);
        }
        long currentTime = millis();//record current time
        /* Check if it's time to take another reading for each sensor 
           If it is, update "lastUpdate" time, then read and update the appropriate characteristic
           A change in the characteristic will notify the connected cluster head
        */
        //rainsteam
        if(currentTime - lastRainsteamUpdate >= RAINSTEAM_READ_DELAY){
            lastRainsteamUpdate = currentTime;
            uint16_t getValue = readRainsteamAna();
            double getProcessedValue = (double) getValue;
            getProcessedValue = getProcessedValue/4095*100;
            getValue = (uint16_t) getProcessedValue;
            Log.info("[postprocess] Read rainsteam : %u analog read", getValue);
            if(rainsteamAssigned == rainsteamArraySize)
            {
                //1: calculates average
                int rainsteamAverage = 0;
                for(int i = 0; i < rainsteamArraySize; i++)
                {
                    rainsteamAverage += rainsteamArray[i];
                }

                rainsteamAverage = (uint16_t) (rainsteamAverage / rainsteamArraySize);
                rainsteamSensorCharacteristic.setValue(rainsteamAverage);
                //log reading
                rainsteamCloud = getValue;
                Log.info("1 Rain steam: %u", rainsteamArray[0]);
                Log.info("2 Rain steam: %u", rainsteamArray[1]);
                Log.info("3 Rain steam: %u", rainsteamArray[2]);
                Log.info("4 Rain steam: %u", rainsteamArray[3]);
                Log.info("5 Rain steam: %u", rainsteamArray[4]);
                Log.info("Average Rain steam: %u", rainsteamAverage);
                rainsteamAssigned = 0;
            }
            else        //if the  array is not full, adds the last value to the end.
            {
                rainsteamArray[rainsteamAssigned] = getValue;
                rainsteamAssigned++;
            }
            
            //rainsteamSensorCharacteristic.setValue(getValue);

            //send bluetooth transmission
            //rainsteamSensorCharacteristic.setValue(getValue);

        }
        //liquid
        if(currentTime - lastLiquidUpdate >= LIQUID_READ_DELAY){
            lastLiquidUpdate = currentTime;
            uint16_t getValue = readLiquid();
            double getProcessedValue = (double) getValue;
            getProcessedValue = getProcessedValue/4095*100;
            getValue = (uint16_t) getProcessedValue;
            //Log.info("[postprocess] Read Liquid : %u analog read", getValue);

            if(liquidAssigned == liquidArraySize)
            {
                //1: calculates average
                int liquidAverage = 0;
                for(int i = 0; i < liquidArraySize; i++)
                {
                    liquidAverage += liquidArray[i];
                }

                liquidAverage = (uint16_t) (liquidAverage / liquidArraySize);
                liquidSensorCharacteristic.setValue(liquidAverage);
                //log reading
                liquidCloud = getValue;
                Log.info("1  Liquid: %u", liquidArray[0]);
                Log.info("2  Liquid: %u", liquidArray[1]);
                Log.info("3  Liquid: %u", liquidArray[2]);
                Log.info("4  Liquid: %u", liquidArray[3]);
                Log.info("5  Liquid: %u", liquidArray[4]);
                Log.info("Average Liquid: %u", liquidAverage);
                liquidAssigned = 0;
            }
            else        //if the  array is not full, adds the last value to the end.
            {
                liquidArray[liquidAssigned] = getValue;
                liquidAssigned++;
            }

            //send bluetooth transmission
            //liquidSensorCharacteristic.setValue(getValue);

            //log reading
            //liquidCloud = getValue;
        }
        //human detector
        if(currentTime - lastHumanDetectorUpdate >= HUMAN_DETECTOR_READ_DELAY){
            lastHumanDetectorUpdate = currentTime;
            uint8_t getValue = readHumanDetector();
            Log.info("Previous Human detector state: %u", lastHumandDetectorValue);
            //only send an update if the value has changed since last read,
            //i.e. a human has been detected or lost
            if(getValue != lastHumandDetectorValue){

                //send bluetooth transmission
                humanDetectorCharacteristic.setValue(getValue);//send the value which was read
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

/** Returns the current rainsteam in microseconds */
uint64_t getCurrentTime(){
    return Time.now();
    // return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/* Read the value on the rainsteam sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
int16_t readRainsteamAna(){
    // Read rainsteam pin
	uint16_t t = analogRead(rainsteamPin);

    //cloud debug stuff - can delete after testing
	char str[2];
	sprintf(str, "%u", t);
	Particle.publish("Rainsteam Analog", str, PUBLIC);
	
    //convert pin value to celsius
	//int8_t degC = (int8_t) t*0.08 - 273;
    Log.info("Read Rainsteam Analog: %u ", t);

	//return degC;
    return t;
}

/* Read the value on the liquid sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint16_t readLiquid(){
    uint16_t getS = analogRead(liquidPin);
    getLiquid = getS;

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

/** Function called whenver a value for solenoidVoltageCharacteristic is received via bluetooth.
 *  Updates the voltage supplied to the LED actuator to the received value */

void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the 2-byte value to set the fan pin adc to
    uint8_t solenoidVoltage;
    memcpy(&solenoidVoltage, &data[0], sizeof(uint8_t));

    Log.info("Solenoid updated via bluetooth to %u", solenoidVoltage);
    
    solenoidVoltageCharacteristic.setValue(solenoidVoltage);
    //Should be 0 to turn off, 1 to turn on.
    if(solenoidVoltage == 1)
    {
        solenoidIsOn = true;
    }
    else
    {
        solenoidIsOn = false;   //Turns it off in all other cases.
    }
}