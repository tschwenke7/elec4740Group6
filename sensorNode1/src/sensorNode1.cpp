/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "d:/UoN/ELEC4470/Repo/elec4740Group6/sensorNode1/src/sensorNode1.ino"
#include "Particle.h"
#include "dct.h"
#include <HC-SR04.h>
#include <Grove_Temperature_And_Humidity_Sensor.h>
#include <chrono>
/*
 * sensorNode1.ino
 * Description: code to flash to the "sensor node 1" argon for assignment 3
 * Author: Tom Schwenke, Edward Ingle
 * Date: 20/05/2020
 */

void setup();
void loop();
uint64_t getCurrentTime();
int8_t readTemperature();
uint8_t readHumidity();
uint16_t readLight();
uint16_t readMoisture();
#line 13 "d:/UoN/ELEC4470/Repo/elec4740Group6/sensorNode1/src/sensorNode1.ino"
DHT dht(D0);        //DHT for temperature/humidity 

SYSTEM_MODE(MANUAL); //Put into Automatic mode so the argon can connect to the cloud

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

/* Service UUID for sensor node 1. It is advertised as one service, 
   with 5 characteristics for each of its 4 sensors and its 1 actuator*/
const char* sensorNode1ServiceUuid("754ebf5e-ce31-4300-9fd5-a8fb4ee4a811");


/*Temperature sensor variables */
//duration in millis to wait between reads
const uint32_t TEMPERATURE_READ_DELAY =  1000;//300000;//1000; remember, this is milliseconds
unsigned long lastTemperatureUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* temperatureSensorUuid("29fba3f5-4ce8-46bc-8d75-77806db22c31");
BleCharacteristic temperatureSensorCharacteristic("temp",
BleCharacteristicProperty::NOTIFY, temperatureSensorUuid, sensorNode1ServiceUuid);
//Array of last recorded for short term averages
int16_t temperatureArray[5];
int16_t temperatureAssigned = 0;                                 //Tracks the number of assigned . When temp assigned == tempArray.length, it sends the temperature to the clusterhead.
int16_t temperatureArraySize = sizeof(temperatureArray)/sizeof(temperatureArray[0]); //Holds array size for readability.

/*Humidity sensor variables */
// const int temperaturePin = A0; //pin reading output of temp sensor
//duration in millis to wait between reads
const uint32_t HUMIDITY_READ_DELAY = 1000;//300000;
unsigned long lastHumidityUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* humiditySensorUuid("99a0d2f9-1cfa-42b3-b5ba-1b4d4341392f");
BleCharacteristic humiditySensorCharacteristic("humid",
BleCharacteristicProperty::NOTIFY, humiditySensorUuid, sensorNode1ServiceUuid);
//Array of last recorded moistures for short term averages
int16_t humidityArray[5];
int16_t humidityAssigned = 0;                                 //Tracks the number of assigned humidity. When temp assigned == tempArray.length, it sends the temperature to the clusterhead.
int16_t humidityArraySize = sizeof(humidityArray)/sizeof(humidityArray[0]); //Holds array size for readability.

/* Light sensor variables */
const int lightPin = A2; //pin reading output of sensor
//duration in millis to wait between reads
const uint32_t LIGHT_READ_DELAY =  1000;// 300000;
unsigned long lastLightUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* lightSensorUuid("45be4a56-48f5-483c-8bb1-d3fee433c23c");
BleCharacteristic lightSensorCharacteristic("light",
BleCharacteristicProperty::NOTIFY, lightSensorUuid, sensorNode1ServiceUuid);
//Array of last recorded moistures for short term averages
int16_t lightArray[5];
int16_t lightAssigned = 0;                                 //Tracks the number of assigned moistures. When temp assigned == tempArray.length, it sends the temperature to the clusterhead.
int16_t lightArraySize = sizeof(lightArray)/sizeof(lightArray[0]); //Holds array size for readability.

/* Soil Moisture sensor variables */
const int moisturePin = A1; //pin reading output of sensor
//duration in millis to wait between reads
const uint32_t MOISTURE_READ_DELAY_OVERALL =   6000;//900000;    //Overall moisture read delay
const uint32_t MOISTURE_READ_DELAY = MOISTURE_READ_DELAY_OVERALL/6;     //Actual moisture read delay - reads average of every 6 readings.
unsigned long lastMoistureUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* moistureSensorUuid("ea5248a4-43cc-4198-a4aa-79200a750835");
BleCharacteristic moistureSensorCharacteristic("moisture",
BleCharacteristicProperty::NOTIFY, moistureSensorUuid, sensorNode1ServiceUuid);
//Array of last recorded moistures for short term averages
int16_t moistureArray[6];
int16_t moistureAssigned = 0;                                 //Tracks the number of assigned moistures. When temp assigned == tempArray.length, it sends the temperature to the clusterhead.
int16_t moistureArraySize = sizeof(moistureArray)/sizeof(moistureArray[0]); //Holds array size for readability.
/* Initial setup */
void setup() {
    const uint8_t val = 0x01;
    dct_write_app_data(&val, DCT_SETUP_DONE_OFFSET, 1);
    
    (void)logHandler; // Does nothing, just to eliminate the unused variable warning
    
    /* Setup bluetooth characteristics and advertise sensorNode1Service to be connected to by the clusterhead */
    BLE.on();//activate BT

    //add characteristics
    BLE.addCharacteristic(temperatureSensorCharacteristic);
    BLE.addCharacteristic(humiditySensorCharacteristic);
    BLE.addCharacteristic(lightSensorCharacteristic);
    BLE.addCharacteristic(moistureSensorCharacteristic);

    //data to be advertised
    BleAdvertisingData advData;
    advData.appendServiceUUID(sensorNode1ServiceUuid);

    // Continuously advertise when not connected to clusterhead
    Log.info("Start advertising");
    BLE.advertise(&advData);

    //Initialises rangefinder
    //rangefinder.init();
    //setup fan pin as PWM output
    //pinMode(fanSpeedPin, OUTPUT);
}

void loop() {
    //only begin using sensors when this node has connected to a cluster head
    if(true){   //BLE.connected()){
        long currentTime = millis();//record current time
        /* Check if it's time to take another reading for each sensor 
           If it is, update "lastUpdate" time, then read and update the appropriate characteristic
           A change in the characteristic will notify the connected cluster head
        */

        //temperature and humidity
        if(currentTime - lastTemperatureUpdate >= TEMPERATURE_READ_DELAY){
            //reset read delay timer
            lastTemperatureUpdate = currentTime;
            //read temp
            int8_t temp = readTemperature();
            
            if(temperatureAssigned == temperatureArraySize)
            {
                //1: calculates average
                int temperatureAverage = 0;
                for(int i = 0; i < temperatureArraySize; i++)
                {
                    temperatureAverage += temperatureArray[i];
                }

                temperatureAverage = (uint16_t) (temperatureAverage / temperatureArraySize);
                temperatureSensorCharacteristic.setValue(temperatureAverage);
                //log reading
                Log.info("1 temp : %u", temperatureArray[0]);
                Log.info("2 temp : %u", temperatureArray[1]);
                Log.info("3 temp : %u", temperatureArray[2]);
                Log.info("4 temp : %u", temperatureArray[3]);
                Log.info("5 temp : %u", temperatureArray[4]);
                Log.info("Average temp : %u", temperatureAverage);
                temperatureAssigned = 0;
            }
            else        //if the  array is not full, adds the last value to the end.
            {
                temperatureArray[temperatureAssigned] = temp;
                temperatureAssigned++;
            }
            /*
            //package together with send time in a buffer
            uint8_t* transmission[9];
            //memcpy(transmission, &tempAverage, sizeof(tempAverage));
            memcpy(transmission, &temp, sizeof(temp));
            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            //memcpy(transmission + sizeof(tempAverage), &sendTime, sizeof(sendTime));
            memcpy(transmission + sizeof(temp), &sendTime, sizeof(sendTime));
            */
            //send bluetooth transmission
            //temperatureSensorCharacteristic.setValue(transmission);
        }
        
        //light
        if(currentTime - lastLightUpdate >= LIGHT_READ_DELAY){
            lastLightUpdate = currentTime;
            uint16_t getValue = readLight();
            double getProcessedValue = (double) getValue;
            getProcessedValue = getProcessedValue/4095*100;
            getValue = (uint16_t) getProcessedValue;
            Log.info("[postprocess] Read light : %u analog read", getValue);
            if(lightAssigned == lightArraySize)
            {
                //1: calculates average
                int lightAverage = 0;
                for(int i = 0; i < lightArraySize; i++)
                {
                    lightAverage += lightArray[i];
                }

                lightAverage = (uint16_t) (lightAverage / lightArraySize);
                lightSensorCharacteristic.setValue(lightAverage);
                //log reading
                Log.info("1 light : %u", lightArray[0]);
                Log.info("2 light : %u", lightArray[1]);
                Log.info("3 light : %u", lightArray[2]);
                Log.info("4 light : %u", lightArray[3]);
                Log.info("5 light : %u", lightArray[4]);
                Log.info("Average light : %u", lightAverage);
                lightAssigned = 0;
            }
            else        //if the  array is not full, adds the last value to the end.
            {
                lightArray[lightAssigned] = getValue;
                lightAssigned++;
            }
            /*
            //store data in buffer
            uint8_t* transmission[10];
            memcpy(transmission, &getValue, sizeof(getValue));
            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            memcpy(transmission + sizeof(getValue), &sendTime, sizeof(sendTime));

            lightSensorCharacteristic.setValue(transmission);
            */
        }
        
        //Moisture
        if(currentTime - lastMoistureUpdate >= MOISTURE_READ_DELAY){
            lastMoistureUpdate = currentTime;
            uint16_t getValue = readMoisture();
            double getProcessedValue = (double) getValue;
            getProcessedValue = getProcessedValue/4095*100;
            getValue = (uint16_t) getProcessedValue;
            Log.info("[postprocess] Read light : %u analog read", getValue);
            //moistureSensorCharacteristic.setValue(getValue);


            /*
            //store data in buffer
            uint8_t* transmission[10];
            memcpy(transmission, &getValue, sizeof(getValue));
            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            memcpy(transmission + sizeof(getValue), &sendTime, sizeof(sendTime));

            moistureSensorCharacteristic.setValue(transmission);
            Log.info("moisture: %u", getValue);
            */
            if(moistureAssigned == moistureArraySize)
            {
                //1: calculates average
                int8_t moistureAverage = 0;
                for(int i = 0; i < moistureArraySize; i++)
                {
                    moistureAverage += moistureArray[i];
                }

                moistureAverage = (uint16_t) moistureAverage / moistureArraySize;
                /*
                //2: returns the average to the clusterhead.
                //package together with send time in a buffer
                uint8_t* transmission[9];
                memcpy(transmission, &moistureAverage, sizeof(moistureAverage));

                //record and append the sending time
                uint64_t sendTime = getCurrentTime();
                memcpy(transmission + sizeof(moistureAverage), &sendTime, sizeof(sendTime));

                */
                //send bluetooth transmission
                moistureSensorCharacteristic.setValue(moistureAverage);
                //resets tempAssigned.
                moistureAssigned = 0;
            }
            else        //if the temperature array is not full, adds the last temperature to the end.
            {
                moistureArray[moistureAssigned] = getValue;
                moistureAssigned++;
            }
        }
        //humidity
        if(currentTime - lastHumidityUpdate >= HUMIDITY_READ_DELAY){
           lastHumidityUpdate = currentTime;
           uint8_t humidity = readHumidity();
            //send bluetooth transmission
            //humiditySensorCharacteristic.setValue(humidity);
            
            if(humidityAssigned == humidityArraySize)
            {
                //1: calculates average
                int humidityAverage = 0;
                for(int i = 0; i < humidityArraySize; i++)
                {
                    humidityAverage += humidityArray[i];
                }

                humidityAverage = (uint16_t) (humidityAverage / humidityArraySize);
                humiditySensorCharacteristic.setValue(humidityAverage);
                //log reading
                Log.info("1 temp : %u", humidityArray[0]);
                Log.info("2 temp : %u", humidityArray[1]);
                Log.info("3 temp : %u", humidityArray[2]);
                Log.info("4 temp : %u", humidityArray[3]);
                Log.info("5 temp : %u", humidityArray[4]);
                Log.info("Average temp : %u", humidityAverage);
                humidityAssigned = 0;
            }
            else        //if the  array is not full, adds the last value to the end.
            {
                humidityArray[humidityAssigned] = humidity;
                humidityAssigned++;
            }
        }
        delay(100);
    }
    else{
        Log.info("not connected yet... ");
    }
}

/** Function called whenver a value for fanSpeedCharacteristic is received via bluetooth.
 *  Updates the speed of the fan to the received value */
/*
void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the byte value to set the fan pin adc to
    uint8_t fanSpeed;
    memcpy(&fanSpeed, &data[0], sizeof(uint8_t));

    if( (fanGetTime - fanStartTime) > fanInitTime)
    {
        Log.info("Fan Started!");
        //todo: Don't run for the first 0.1 second.
        Log.info("The fan power has been set via BT to %u", fanSpeed);
        //set the PWM output to the fan
        analogWrite(fanSpeedPin, fanSpeed, fanSpeedHz);
    } 
}
*/

/** Returns the current temperature in microseconds */
uint64_t getCurrentTime(){
    return Time.now();
    // return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/* Read the temperature in degrees celcius
*/
int8_t readTemperature(){
    // Read temperature as Celsius
	int8_t t = (int8_t) dht.getTempCelcius();   //Normally returns float
    
    Log.info("Read temperature: %u", t);

    //cloud data - can delete when not testing
	char str[2];
    sprintf(str, "%u", t);
	//Particle.publish("temperature", str, PUBLIC);
	
	return t;
}

/* Read the humidity in % */
uint8_t readHumidity(){
    //Read Humidity
	uint8_t h = (uint8_t) dht.getHumidity(); //normally returns float, casted to uint16_t
	Log.info("Read humidity: %u", h);
	
    //cloud data - can delete when not testing
    char str[2];
    sprintf(str, "%u", h);
	//Particle.publish("humidity", str, PUBLIC);

    return  h;
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
	//uint16_t lux =  (uint16_t) (getL - 1382.758621)/3.793103448 + 30;
    uint16_t lux = (uint16_t) (getL);
    Log.info("Read light: %u analog read", lux);
    return lux;
}

/* Read the value on the moisture sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint16_t readMoisture(){
    //do any transformation logic we might want
    uint16_t getL = analogRead(moisturePin);

    //cloud debug stuff - can delete after testing
	char str[2];
	sprintf(str, "%u", getL);
	Particle.publish("moisture", str, PUBLIC);
    
    Log.info("Read moisture: %u", getL);
    //return lux;
    return getL;
}