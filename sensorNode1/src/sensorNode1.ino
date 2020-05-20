
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
const uint16_t TEMPERATURE_READ_DELAY = 30000;
unsigned long lastTemperatureUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* temperatureSensorUuid("bc7f18d9-2c43-408e-be25-62f40645987c");
BleCharacteristic temperatureSensorCharacteristic("temp",
BleCharacteristicProperty::NOTIFY, temperatureSensorUuid, sensorNode1ServiceUuid);

/*Humidity sensor variables */
// const int temperaturePin = A0; //pin reading output of temp sensor
//duration in millis to wait between reads
const uint16_t HUMIDITY_READ_DELAY = 30000;
unsigned long lastHumidityUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* humiditySensorUuid("99a0d2f9-1cfa-42b3-b5ba-1b4d4341392f");
BleCharacteristic humiditySensorCharacteristic("humid",
BleCharacteristicProperty::NOTIFY, humiditySensorUuid, sensorNode1ServiceUuid);

/* Distance sensor variables */
const int distanceTriggerPin = D2;  //pin reading input of sensor
const int distanceEchoPin = D3;     //pin reading output of sensor
HC_SR04 rangefinder = HC_SR04(distanceTriggerPin, distanceEchoPin);
//duration in millis to wait between reads
const uint16_t DISTANCE_READ_DELAY = 1000;
unsigned long lastDistanceUpdate = 0;//last absolute time a recording was taken
//advertised bluetooth characteristic
const char* distanceSensorUuid("45be4a56-48f5-483c-8bb1-d3fee433c23c");
BleCharacteristic distanceSensorCharacteristic("distance",
BleCharacteristicProperty::NOTIFY, distanceSensorUuid, sensorNode1ServiceUuid);
uint8_t lastRecordedDistance = 255;

/* Current sensor characteristic */
const int currentSensorPin = -1; //TODO: update this
const uint16_t CURRENT_READ_DELAY = 5000;//time between sampling current
unsigned long lastCurrentUpdate = 0;
//advertised bluetooth characteristic
const char* currentSensorUuid("2822a610-32d6-45e1-b9fb-247138fc8df7");
BleCharacteristic currentSensorCharacteristic("current",
BleCharacteristicProperty::NOTIFY, currentSensorUuid, sensorNode1ServiceUuid);

/* Fan actuator characteristic */
const int fanSpeedPin = -1; //TODO: update this
const uint fanSpeedHz = 50;
//advertised bluetooth characteristic
const char* fanSpeedUuid("29fba3f5-4ce8-46bc-8d75-77806db22c31");
BleCharacteristic fanSpeedCharacteristic("fanSpeed",
BleCharacteristicProperty::WRITE_WO_RSP, fanSpeedUuid, sensorNode1ServiceUuid, onDataReceived);

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
    BLE.addCharacteristic(distanceSensorCharacteristic);

    //data to be advertised
    BleAdvertisingData advData;
    advData.appendServiceUUID(sensorNode1ServiceUuid);

    // Continuously advertise when not connected to clusterhead
    Log.info("Start advertising");
    BLE.advertise(&advData);

    //Initialises rangefinder
    rangefinder.init();
    //setup fan pin as PWM output
    pinMode(fanSpeedPin, OUTPUT);
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

            //package together with send time in a buffer
            uint8_t* transmission[9];
            memcpy(transmission, &temp, sizeof(temp));

            //record and append the sending time
            uint64_t sendTime = getCurrentTime();
            memcpy(transmission + sizeof(temp), &sendTime, sizeof(sendTime));

            //send bluetooth transmission
            temperatureSensorCharacteristic.setValue(transmission);
        }
        //humidity
        if(currentTime - lastHumidityUpdate >= HUMIDITY_READ_DELAY){
           lastHumidityUpdate = currentTime;
           uint8_t humidity = readHumidity();
           
           //send bluetooth transmission
           humiditySensorCharacteristic.setValue(humidity);
        }
        //distance
        if(currentTime - lastDistanceUpdate >= DISTANCE_READ_DELAY){
            lastDistanceUpdate = currentTime;
            uint8_t getValue = readDistance();

            //if distance remains 0 for multiple cycles, only send first 0 over bluetooth
            //this helps save power
            if(!(getValue == 0 && lastRecordedDistance == 0)){
                //store data in buffer
                uint8_t* transmission[9];
                memcpy(transmission, &getValue, sizeof(getValue));
                //record and append the sending time
                uint64_t sendTime = getCurrentTime();
                memcpy(transmission + sizeof(getValue), &sendTime, sizeof(sendTime));

                //send bluetooth transmission
                distanceSensorCharacteristic.setValue(transmission);
                lastRecordedDistance = getValue;//update last recorded distance
                Log.info("Distance transmitted.");
            }
            Log.info("Distance: " + getValue);
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
    }
}

/** Function called whenver a value for fanSpeedCharacteristic is received via bluetooth.
 *  Updates the speed of the fan to the received value */
void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the 2-byte value to set the fan pin adc to
    uint16_t fanSpeed;
    memcpy(&fanSpeed, &data[0], sizeof(uint16_t));

    Log.info("The fan power has been set via BT to %u", fanSpeed);
    //set the PWM output to the fan
    analogWrite(fanSpeedPin, fanSpeed, fanSpeedHz);
}

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
	Particle.publish("temperature", str, PUBLIC);
	
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
	Particle.publish("humidity", str, PUBLIC);

    return  h;
}

/* Read the value on the current sensor pin */
uint16_t readCurrent(){
    //TODO: work out how to actually read this

    //cloud data - can delete when not testing
    char str[2];
    sprintf(str, "%u", 0);
	Particle.publish("distance (not currently implemented)", str, PUBLIC);

    Log.info("Read current (not currently implemented): %u", 0);
    return 0;
}

/* Read the distance in centimetres*/
uint8_t readDistance(){
    uint8_t cms = (uint8_t) rangefinder.distCM();
	Log.info("Read distance: %u", cms);
    
    //cloud data - can delete when not testing
    char str[2];
    sprintf(str, "%u", cms);
	Particle.publish("distance", str, PUBLIC);
    
    return cms;
}