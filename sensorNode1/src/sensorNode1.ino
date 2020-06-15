#include "Particle.h"
#include "dct.h"
#include <HC-SR04.h>
#include <Grove_Temperature_And_Humidity_Sensor.h>
#include <chrono>
/*
 * sensorNode1.ino
 * Description: code to flash to the "sensor node 1" argon for assignment 2
 * Author: Tom Schwenke, Edward Ingle
 * Date: 20/05/2020
 */

DHT dht(D0);        //DHT for temperature/humidity 

SYSTEM_MODE(AUTOMATIC); //Put into Automatic mode so the argon can connect to the cloud

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

/* Service UUID for sensor node 1. It is advertised as one service, 
   with 5 characteristics for each of its 4 sensors and its 1 actuator*/
const char* sensorNode1ServiceUuid("754ebf5e-ce31-4300-9fd5-a8fb4ee4a811");


/*Temperature sensor variables */
//duration in millis to wait between reads
const uint16_t TEMPERATURE_READ_DELAY = 1000;
unsigned long lastTemperatureUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* temperatureSensorUuid("bc7f18d9-2c43-408e-be25-62f40645987c");
BleCharacteristic temperatureSensorCharacteristic("temp",
BleCharacteristicProperty::NOTIFY, temperatureSensorUuid, sensorNode1ServiceUuid);
//Array of last recorded temperatures for short term averages
int8_t tempArray[5];
int8_t tempAssigned = 0;                                //Tracks the number of assigned temperatures. When temp assigned == tempArray.length, it sends the temperature to the clusterhead.
int8_t tempArraySize = sizeof(tempArray)/sizeof(tempArray[0]); //Holds array size for readability.

/*Humidity sensor variables */
// const int temperaturePin = A0; //pin reading output of temp sensor
//duration in millis to wait between reads
const uint16_t HUMIDITY_READ_DELAY = 1000;
unsigned long lastHumidityUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* humiditySensorUuid("99a0d2f9-1cfa-42b3-b5ba-1b4d4341392f");
BleCharacteristic humiditySensorCharacteristic("humid",
BleCharacteristicProperty::NOTIFY, humiditySensorUuid, sensorNode1ServiceUuid);
//Array of last recorded humidity for short term averages
int8_t humidityArray[5];
int8_t humidityAssigned = 0;                                //Tracks the number of assigned humidity. When humidity assigned == humidityArray.length, it sends the humidity to the clusterhead.
int8_t humidityArraySize = sizeof(humidityArray)/sizeof(humidityArray[0]); //Holds array size for readability.


/* Light sensor variables */
const int lightPin = A2; //pin reading output of sensor
//duration in millis to wait between reads
const uint16_t LIGHT_READ_DELAY = 1000;
unsigned long lastLightUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* lightSensorUuid("ea5248a4-43cc-4198-a4aa-79200a750835");
BleCharacteristic lightSensorCharacteristic("light",
BleCharacteristicProperty::NOTIFY, lightSensorUuid, sensorNode1ServiceUuid);

/* Moisture sensor variables */
const int moisturePin = A1; //pin reading output of sensor
//duration in millis to wait between reads
const uint16_t MOISTURE_READ_DELAY = 1000;
unsigned long lastMoistureUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* moistureSensorUuid("ea5248a4-43cc-4198-a4aa-79200a750835");
BleCharacteristic moistureSensorCharacteristic("moisture",
BleCharacteristicProperty::NOTIFY, moistureSensorUuid, sensorNode1ServiceUuid);

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
    if(BLE.connected()){
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
            /*
            //1: calculates average
            int8_t tempAverage = 0;
            for(int i = 0; i < tempArraySize; i++)
            {
                tempAverage += tempArray[i];
            }
            tempAverage = (int8_t) tempAverage / tempArraySize;
            */
            //2: returns the average to the clusterhead.
            //package together with send time in a buffer
            uint8_t* transmission[9];
            //memcpy(transmission, &tempAverage, sizeof(tempAverage));
            memcpy(transmission, &temp, sizeof(temp));
            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            //memcpy(transmission + sizeof(tempAverage), &sendTime, sizeof(sendTime));
            memcpy(transmission + sizeof(temp), &sendTime, sizeof(sendTime));

            //send bluetooth transmission
            temperatureSensorCharacteristic.setValue(transmission);

            /*
            if(tempAssigned == tempArraySize)
            {
                //1: calculates average
                int8_t tempAverage = 0;
                for(int i = 0; i < tempArraySize; i++)
                {
                    tempAverage += tempArray[i];
                }
                tempAverage = (int8_t) tempAverage / tempArraySize;
                //2: returns the average to the clusterhead.
                //package together with send time in a buffer
                uint8_t* transmission[9];
                memcpy(transmission, &tempAverage, sizeof(tempAverage));

                //record and append the sending time
                uint64_t sendTime = getCurrentTime();
                memcpy(transmission + sizeof(tempAverage), &sendTime, sizeof(sendTime));

                //send bluetooth transmission
                temperatureSensorCharacteristic.setValue(transmission);
                //resets tempAssigned.
                tempAssigned = 0;
            }
            else        //if the temperature array is not full, adds the last temperature to the end.
            {
                tempArray[tempAssigned] = temp;
                tempAssigned++;
            }
            */
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
            Log.info("Light: %u", getValue);
        }
        
        //Moisture
        if(currentTime - lastMoistureUpdate >= MOISTURE_READ_DELAY){
            lastMoistureUpdate = currentTime;
            uint16_t getValue = readMoisture();

            //store data in buffer
            uint8_t* transmission[10];
            memcpy(transmission, &getValue, sizeof(getValue));
            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            memcpy(transmission + sizeof(getValue), &sendTime, sizeof(sendTime));

            moistureSensorCharacteristic.setValue(transmission);
            Log.info("moisture: %u", getValue);
        }
        //humidity
        if(currentTime - lastHumidityUpdate >= HUMIDITY_READ_DELAY){
           lastHumidityUpdate = currentTime;
           uint8_t humidity = readHumidity();
           
           //send bluetooth transmission
           humiditySensorCharacteristic.setValue(humidity);

            /*
            if(humidityAssigned == humidityArraySize)
            {
                //1: calculates average
                int8_t humidityAverage = 0;
                for(int i = 0; i < humidityArraySize; i++)
                {
                    humidityAverage += humidityArray[i];
                }
                humidityAverage = (int8_t) humidityAverage / humidityArraySize;
                */
                //2: returns the average to the clusterhead.
                //package together with send time in a buffer
            uint8_t* transmission[9];
            //memcpy(transmission, &humidityAverage, sizeof(humidityAverage));.
            memcpy(transmission, &humidity, sizeof(humidity));

            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            //memcpy(transmission + sizeof(humidityAverage), &sendTime, sizeof(sendTime));
            memcpy(transmission + sizeof(humidity), &sendTime, sizeof(sendTime));

            //send bluetooth transmission
            humiditySensorCharacteristic.setValue(transmission);
            //resets humidityAssigned.
            humidityAssigned = 0;
                /*
            }
            else        //if the humidity array is not full, adds the last humidity to the end.
            {
                humidityArray[humidityAssigned] = humidity;
                humidityAssigned++;
            }
                */
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

/* Read the value on the current sensor pin */
uint16_t readCurrent(){
    //TODO: work out how to actually read this

    //cloud data - can delete when not testing
    char str[2];
    sprintf(str, "%u", 0);
	//Particle.publish("Current (not currently implemented)", str, PUBLIC);

    Log.info("Read current (not currently implemented): %u", 0);
    return 0;
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
    
    //convert to lux
	uint16_t lux =  (uint16_t) (getL - 1382.758621)/3.793103448 + 30;
    Log.info("Read moisture: %u lux", lux);
    return lux;
}