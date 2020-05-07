#include "Particle.h"
#include "dct.h"
#include <HC-SR04.h>
#include <Grove_Temperature_And_Humidity_Sensor.h>
/*
 * sensorNode1.ino
 * Description: code to flash to the "sensor node 1" argon for assignment 1
 * Author: Tom Schwenke, Edward Ingle
 * Date: 27/04/2020
 */

DHT dht(D0);        //DHT for temperature/humidity 

SYSTEM_MODE(AUTOMATIC); //Put into Automatic mode so the argon can connect to the cloud

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

/* Service UUID for sensor node 1. It is advertised as one service, 
   with 4 characteristics for each of its 4 sensors */
const char* sensorNode1ServiceUuid("754ebf5e-ce31-4300-9fd5-a8fb4ee4a811");

/*Temperature and humidity sensor variables */
// const int temperaturePin = A0; //pin reading output of temp sensor
//duration in millis to wait between reads
const uint16_t TEMP_AND_HUMIDITY_READ_DELAY = 5000;
unsigned long lastTempAndHumidityUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* tempAndHumiditySensorUuid("99a0d2f9-1cfa-42b3-b5ba-1b4d4341392f");
BleCharacteristic tempAndHumiditySensorCharacteristic("tempAndHumid",
BleCharacteristicProperty::NOTIFY, tempAndHumiditySensorUuid, sensorNode1ServiceUuid);

/* Light sensor variables */
const int lightPin = A1; //pin reading output of sensor
//duration in millis to wait between reads
const uint16_t LIGHT_READ_DELAY = 5000;
unsigned long lastLightUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* lightSensorUuid("ea5248a4-43cc-4198-a4aa-79200a750835");
BleCharacteristic lightSensorCharacteristic("temp",
BleCharacteristicProperty::NOTIFY, lightSensorUuid, sensorNode1ServiceUuid);

/* Distance sensor variables */
const int distanceTriggerPin = D3;  //pin reading input of sensor
const int distanceEchoPin = D2;     //pin reading output of sensor
HC_SR04 rangefinder = HC_SR04(distanceTriggerPin, distanceEchoPin);
//duration in millis to wait between reads
const uint16_t DISTANCE_READ_DELAY = 5000;
unsigned long lastDistanceUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* distanceSensorUuid("45be4a56-48f5-483c-8bb1-d3fee433c23c");
BleCharacteristic distanceSensorCharacteristic("temp",
BleCharacteristicProperty::NOTIFY, distanceSensorUuid, sensorNode1ServiceUuid);
uint8_t lastRecordedDistance = 255;


/*debug variables */
double temperatureAnaCloud = 0;
double temperatureCloud = 0;
double lightCloud = 0;
double humidityCloud = 0;
double distanceCloud = 0;

/* Initial setup */
void setup() {
    const uint8_t val = 0x01;
    dct_write_app_data(&val, DCT_SETUP_DONE_OFFSET, 1);
    
    (void)logHandler; // Does nothing, just to eliminate the unused variable warning
    
    /* setup debug variables */
    Particle.variable("temperatureAna", temperatureAnaCloud);
    //Particle.variable("temperature", temperatureCloud);
    Particle.variable("light", lightCloud);
    Particle.variable("humidity", humidityCloud);
    Particle.variable("distance", distanceCloud);

    
    /* Setup bluetooth characteristics and advertise sensorNode1Service to be connected to by the clusterhead */
    BLE.on();//activate BT

    //add characteristics
    BLE.addCharacteristic(tempAndHumiditySensorCharacteristic);
    BLE.addCharacteristic(lightSensorCharacteristic);
    BLE.addCharacteristic(distanceSensorCharacteristic);

    //data to be advertised
    BleAdvertisingData advData;
    advData.appendServiceUUID(sensorNode1ServiceUuid);

    // Continuously advertise when not connected to clusterhead
    Log.info("Start advertising");
    BLE.advertise(&advData);

    //Initialises rangefinder
    rangefinder.init();
}

void loop() {
    //only begin using sensors when this node has connected to a cluster head
    //if(BLE.connected()){
        long currentTime = millis();//record current time
        /* Check if it's time to take another reading for each sensor 
           If it is, update "lastUpdate" time, then read and update the appropriate characteristic
           A change in the characteristic will notify the connected cluster head
        */
        //temperature and humidity
        if(currentTime - lastTempAndHumidityUpdate >= TEMP_AND_HUMIDITY_READ_DELAY){
            lastTempAndHumidityUpdate = currentTime;

            uint8_t temp = readTemperature();
            uint8_t humidity = readHumidity();

            //update cloud variables if we're doing this
            temperatureCloud = temp;
            humidityCloud = humidity;

            //package the two together in a 2-byte buffer
            char* transmission[2];
            memcpy(transmission, &temp, sizeof(temp));
            memcpy(transmission + sizeof(temp), &humidity, sizeof(humidity));
 
            //send bluetooth transmission
            tempAndHumiditySensorCharacteristic.setValue(transmission);
        }
        //light
        if(currentTime - lastLightUpdate >= LIGHT_READ_DELAY){
            lastLightUpdate = currentTime;
            uint16_t getValue = readLight();
            lightSensorCharacteristic.setValue(getValue);
            lightCloud = getValue;
            Log.info("Light: " + getValue);
        }
        //distance
        if(currentTime - lastDistanceUpdate >= DISTANCE_READ_DELAY){
            lastDistanceUpdate = currentTime;
            uint8_t getValue = readDistance();

            //if distance remains 0 for multiple cycles, only send first 0 over bluetooth
            //this helps save power
            if(!(getValue == 0 && lastRecordedDistance == 0)){
                distanceSensorCharacteristic.setValue(getValue);//send distance over bluetooth
                lastRecordedDistance = getValue;//update last recorded distance
                Log.info("Distance transmitted.");
            }
            distanceCloud = getValue;
            Log.info("Distance: " + getValue);
        }
        
        delay(100);
    //}
    //else{
    ///    Log.info("not connected yet... ");
    ///}
}

/* Read the value on the temperature sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint8_t readTemperature(){
    // Read temperature as Celsius
	uint8_t t = (uint8_t) dht.getTempCelcius();   //Normally returns float
	//May be able to change this to 8bit int - check when able.
	char str[2];
	sprintf(str, "%u", t);
	Particle.publish("temperature", str, PUBLIC);
	
	return t;
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
    return getL;
}

/* Read the value on the humidity sensor pin 
Analogue pin generates 12 bits of data, so store as a 2-byte uint
*/
uint8_t readHumidity(){
    //Read Humidity
	uint8_t h = (uint8_t) dht.getHumidity(); //normally returns float, casted to uint16_t
	//May be able to change this to 8bit int - check when able.
	char str[2];
	sprintf(str, "%u", h);
	Particle.publish("humidity", str, PUBLIC);
    //do any transformation logic we might want
    return  h;
}

/* Read the distance */
uint8_t readDistance(){
    //do any transformation logic we might want
    uint16_t cms = (uint16_t) rangefinder.distCM();
	char str[2];
	sprintf(str, "%u", cms);
	Particle.publish("distance", str, PUBLIC);
    
    return cms;//placeholder output value to use if sensor isnt connected
}