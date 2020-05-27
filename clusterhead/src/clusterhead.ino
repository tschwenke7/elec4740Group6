#include "Particle.h"
#include "dct.h"
#include <chrono>
#include <string>
#include <LiquidCrystal.h>
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

/* Control variables */
/* Number of quarter-seconds which have passed during the current 2 second cycle
 * of the main loop. Used to activate flasing lights at different intervals of quarter seconds.
 * May eventually overflow, but that's ok cause loop only uses modulo math*/
uint32_t quarterSeconds = 0;
/* holds the starting millis of this execution of the main loop, 
so we can work out how long to wait before beginning the next loop */
unsigned long loopStart = 0;

/* Data tracking variables */
int currentSound = 0;
uint16_t currentLight = 0;
uint8_t currentDistance = 0;

//Gets temp and humidity from sn1, and light from sn2 for logic
int16_t getTempsn1 = -999;
int16_t getHumidsn1 = -1;

int16_t getLightsn2 = -1;
uint8_t getHumanDetectsn2 = 0x00;

/* Alarm variables */
//The active status of each of the 4 possible alarm conditions. True if active, false if inactive
bool alarmActive [4] = {false, false, false, false};
/* The absolute time in seconds each of the 4 alarms was activated.*/
long long alarmActivatedTimes[4] = {0,0,0,0};
/* The absolute time in seconds each of the 4 alarms stopped being triggered.*/
long long alarmEventEndedTimes[4] = {0,0,0,0};
//number of seconds since each alarm stopped being triggered. If one reaches ALARM_COOLOFF_DELAY, reset that alarm
uint16_t alarmCooloffCounters [4] = {0, 0, 0, 0};
//how many seconds has the sound been at a level which can trigger t0=alarm1 or t1=alarm2?
unsigned long durationAtSoundThresholds[2] = {0, 0};

/* USER CONFIGURABLE VARIABLES */
//after this number of seconds without another alarm-worthy event, the alarm will automatically reset.
uint16_t ALARM_COOLOFF_DELAY = 60;
//alarm 0 will be triggered if an object is detected within this many centimetres
uint16_t DISTANCE_THRESHOLD = 25;
//light must go below this value of Lux to trigger alarms 1 or 2
uint16_t LIGHT_THRESHOLD = 100;
//no alarm below t0, alarm 1 may trigger between t0-t1, alarm 2 between t1-t2, alarm 3 if > t2
int16_t SOUND_VOLUME_THRESHOLDS [3] = {55, 70, 80};
//sound must continue for t0 seconds to trigger alarm 1, or t1 seconds for alarm 2
uint16_t SOUND_DURATION_THRESHOLDS [2] = {30, 10};

//values for fan speed and LED Voltage
uint8_t fanspeed2 = 128;
uint8_t fanspeed1 = 64;
uint8_t fanspeed0 = 0;

uint8_t ledVolt100 = 254;
uint8_t ledVolt75 = 191;
uint8_t ledVolt50 = 128;
uint8_t ledVolt30 = 77;

/* Bluetooth variables */
//bluetooth devices we want to connect to and their service ids
BlePeerDevice sensorNode1;
BlePeerDevice sensorNode2;
BleUuid sensorNode1ServiceUuid("754ebf5e-ce31-4300-9fd5-a8fb4ee4a811");
BleUuid sensorNode2ServiceUuid("97728ad9-a998-4629-b855-ee2658ca01f7");

//characteristics we want to track
//for sensor node 1
BleCharacteristic temperatureSensorCharacteristic1;
BleCharacteristic humiditySensorCharacteristic;
BleCharacteristic distanceSensorCharacteristic;
BleCharacteristic currentSensorCharacteristic1;
BleCharacteristic fanSpeedCharacteristic;

//for sensor node 2
BleCharacteristic temperatureSensorCharacteristic2;
BleCharacteristic lightSensorCharacteristic2;
BleCharacteristic soundSensorCharacteristic;
BleCharacteristic humanDetectorCharacteristic;
BleCharacteristic currentSensorCharacteristic2;
BleCharacteristic ledVoltageCharacteristic;


//Variables for LCD display
LiquidCrystal lcd(D0, D1, D2, D3, D4, D5);  


// void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
const size_t SCAN_RESULT_MAX = 30;
BleScanResult scanResults[SCAN_RESULT_MAX];

/* Particle Cloud functions */
/* Reset one or all alarms */
int resetAlarmCloud(String alarmNumber){
    //reset all alarms if input is "all"
    if(alarmNumber.equalsIgnoreCase("all") == 0){
        for(int i = 0; i < 4; i++){
            if(alarmActive[i]){
                resetAlarm(i);
            }
        }
        return 1;
    }
    //otherwise only reset the alarm number provided
    else {
        int i = alarmNumber.toInt();
        if(alarmActive[i]){
            resetAlarm(i);
        }
        return 1;
    }
}

/* set ALARM_COOLOFF_DELAY */
int setAlarmCooloffDelay(String delay){
    ALARM_COOLOFF_DELAY = (uint16_t) delay.toInt();
    return 1;
}

/* set DISTANCE_THRESHOLD */
int setDistanceThreshold(String threshold){
    DISTANCE_THRESHOLD = (uint16_t) threshold.toInt();
    return 1;
}

/* set LIGHT_THRESHOLD */
int setLightThreshold(String threshold){
    LIGHT_THRESHOLD = (uint16_t) threshold.toInt();
    return 1;
}

/* set SOUND_VOLUME_THRESHOLDS based on comma-separated string of 3 numbers */
int setVolumeThresholds(String thresholds){
    String delimiter = ",";
    uint8_t pos = 0;
    int i = 0;
    while ((pos = thresholds.indexOf(delimiter)) != std::string::npos) {
        SOUND_VOLUME_THRESHOLDS[i] = (uint16_t) thresholds.substring(0, pos).toInt();
        thresholds.remove(0, pos + delimiter.length());
        i++;
    }
    return 1;
}

/* set SOUND_DURATION_THRESHOLDS based on comma-separated string of 3 numbers */
int setSoundDurationThresholds(String thresholds){
    String delimiter = ",";
    uint8_t pos = 0;
    int i = 0;
    while ((pos = thresholds.indexOf(delimiter)) != std::string::npos) {
        SOUND_DURATION_THRESHOLDS[i] = (uint16_t) thresholds.substring(0, pos).toInt();
        thresholds.remove(0, pos + delimiter.length());
        i++;
    }
    return 1;  
}


void setup() {
    //initialise Particle Cloud functions
    Particle.function("resetAlarm",resetAlarmCloud);
    Particle.function("setAlarmCooloffDelay",setAlarmCooloffDelay);
    Particle.function("setDistanceThreshold",setDistanceThreshold);
    Particle.function("setLightThreshold",setLightThreshold);
    Particle.function("setVolumeThresholds",setVolumeThresholds);
    Particle.function("setSoundDurationThresholds",setSoundDurationThresholds);

    //take control of the onboard RGB LED
    RGB.control(true);

    //change timezone to aedt (UTC + 11)
    Time.zone(11);

    const uint8_t val = 0x01;
    dct_write_app_data(&val, DCT_SETUP_DONE_OFFSET, 1);
    (void)logHandler; // Does nothing, just to eliminate the unused variable warning

    BLE.on();
    
    
    //map functions to be called whenever new data is received for a characteristic
    temperatureSensorCharacteristic1.onDataReceived(onTemperatureReceived1, NULL);
    humiditySensorCharacteristic.onDataReceived(onHumidityReceived, NULL);
    distanceSensorCharacteristic.onDataReceived(onDistanceReceived, NULL);
    currentSensorCharacteristic1.onDataReceived(onCurrentReceived1, NULL);

    temperatureSensorCharacteristic2.onDataReceived(onTemperatureReceived2, NULL);
    lightSensorCharacteristic2.onDataReceived(onLightReceived2, NULL);
    soundSensorCharacteristic.onDataReceived(onSoundReceived, NULL);
    humanDetectorCharacteristic.onDataReceived(onHumanDetectorReceived, NULL);
    currentSensorCharacteristic2.onDataReceived(onCurrentReceived2, NULL);

    // set up the LCD's number of columns and rows: 
    lcd.begin(16,2);
    // Print a message to the LCD.
    lcd.print("Test");
}

void loop() { 
    //do stuff if both sensors have been connected
    if (sensorNode1.connected()){// && sensorNode2.connected()) {   //Add this back in when required!
        //record start time of this loop
        loopStart = millis();

        //update alarm cooloff timers and sound-threshold durations only every 2 seconds
        if(quarterSeconds % 4 == 0){
            /*check if we need to activate time-based alarms, 
            monitor current alarms to see if they need to timeout and be reset */
            monitorAlarms(2);
            //update sound threshold counters "2" seconds after last update
            updateSoundThresholdCounters(2);
        }

        //flash appropriate colour at appropriate interval for active alarm
        updateStatusLed();

        //test bluetooth
        /*
        if (quarterSeconds % 8 == 0){
            uint16_t test = (uint16_t) quarterSeconds;
            fanSpeedCharacteristic.setValue(test);
            Log.info("%u", test);
        }
        */
        //loop every 250ms, to allow 2Hz status LED flashing if necessary
        //subtract processing time from the delay to make intervals consistently sized
        delay(250 - (millis() - loopStart));
        quarterSeconds+=1;  

        //Displays LCD 
        // set the cursor to column 0, line 1
        // (note: line 1 is the second row, since counting begins with 0):
        lcd.setCursor(0, 1);
        // print the number of seconds since reset:
        lcd.print(millis()/1000);
        
        //Sensor logic for fan
        if((getTempsn1 >= 20)
		&& (getTempsn1 <= 24))
		{
			if(getHumidsn1 >= 60)
			{
                fanSpeedCharacteristic.setValue(fanspeed2);
				//Measure power consumption
			}
			else
			{
                fanSpeedCharacteristic.setValue(fanspeed1);
				//Measure power
			}
		}
		else if(getTempsn1 < 24)
		{
			//Turn off fan
            fanSpeedCharacteristic.setValue(fanspeed0);
		}
		
		if(getHumanDetectsn2 == 0x01)
		{
			if(getLightsn2 > 400)
			{
				//Turn ON the LED light at 50% intensity level and measure the light’s power consumption and display
                ledVoltageCharacteristic.setValue(ledVolt50);
			}
			else if(getLightsn2 < 200)
			{
				//Turn ON the LED light at 100% intensity level and measure the light energy and display above.
                ledVoltageCharacteristic.setValue(ledVolt100);
			}
			else if(
			(getLightsn2 >= 200)
			&& (getLightsn2 <= 400)
			)
			{
				//Turn ON the LED light at 75% intensity level and measure the light energy and display above.
                ledVoltageCharacteristic.setValue(ledVolt75);
			}
		}
		if(getHumanDetectsn2 == 0x00)
		{
			
			if(getLightsn2 < 150)
			{
				//Turn ON the LED light at 30% intensity level
                ledVoltageCharacteristic.setValue(ledVolt30);
			}
			
		}
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
                        sensorNode1.getCharacteristicByUUID(temperatureSensorCharacteristic1, "bc7f18d9-2c43-408e-be25-62f40645987c");
                        sensorNode1.getCharacteristicByUUID(humiditySensorCharacteristic, "99a0d2f9-1cfa-42b3-b5ba-1b4d4341392f");
                        sensorNode1.getCharacteristicByUUID(distanceSensorCharacteristic, "45be4a56-48f5-483c-8bb1-d3fee433c23c");
                        sensorNode1.getCharacteristicByUUID(currentSensorCharacteristic1, "2822a610-32d6-45e1-b9fb-247138fc8df7");
                        sensorNode1.getCharacteristicByUUID(fanSpeedCharacteristic, "29fba3f5-4ce8-46bc-8d75-77806db22c31");
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
                        sensorNode2.getCharacteristicByUUID(currentSensorCharacteristic2, "2822a610-32d6-45e1-b9fb-247138fc8df7");
                        sensorNode2.getCharacteristicByUUID(ledVoltageCharacteristic, "97017674-9615-4fba-9712-6829f2045836");

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

/* check if we need to activate time-based alarms, 
 * monitor current alarms to see if they need to timeout and be reset.
 * @param secondsPassed: number of seconds since this was last called. */
void monitorAlarms(uint8_t secondsPassed){
    //check each of the 4 alarms
    for (uint8_t i = 0; i < 4; i++){
        //if this alarm is still active...
        if(alarmActive[i]){
            //if its conditions are no longer met, increment the cooloff counter
            if(!alarmCondtitionsMet(i)){
                //set the end of alarm time to now, if this is the first iteration of cooloff
                if(alarmCooloffCounters[i] == 0){
                    alarmEventEndedTimes[i] = Time.local();
                }

                alarmCooloffCounters[i] += secondsPassed;

                //if cooloff counter has reached the cooloff delay, then automatically reset this alarm
                if(alarmCooloffCounters[i] >= ALARM_COOLOFF_DELAY){
                    resetAlarm(i);
                }
            }
            
            //if conditions are still met, reset cooloff counter
            else{
                alarmCooloffCounters[i] = 0;
            }
        }
    }

    //check if the time-based alarms (alarms 1 and 2) need to be activated
    if(!alarmActive[2] && alarmCondtitionsMet(2)){
        startAlarm(2);
    }
    if(!alarmActive[1] && alarmCondtitionsMet(1)){
        startAlarm(1);
    }
}

/* Update the sound threshold counters after a given amount of seconds has passed
 * @param secondsPassed: number of seconds since this was last called. */
void updateSoundThresholdCounters(uint8_t secondsPassed){
    //only count up if light level is below threshold
    if(currentLight < LIGHT_THRESHOLD){
        if (currentSound > SOUND_VOLUME_THRESHOLDS[1]){
            durationAtSoundThresholds[1] += secondsPassed;//increment high volume counter
            durationAtSoundThresholds[0] += secondsPassed;//increment med volume counter
        }
        else if(currentSound > SOUND_VOLUME_THRESHOLDS[0]){
            durationAtSoundThresholds[1] = 0;//reset high volume counter
            durationAtSoundThresholds[0] += secondsPassed;//increment med volume counter
        }
        else{
            //reset both counters
            durationAtSoundThresholds[1] = 0;
            durationAtSoundThresholds[0] = 0;
        }
    }
    
    //if it's too bright, don't count, and reset counters
    else{
        //reset both counters
        durationAtSoundThresholds[1] = 0;
        durationAtSoundThresholds[0] = 0;
    }
}

/* Turns the status light on and off at the appropriate intervals, 
based on the values of "quarterSeconds" and "alarmActive[]".
Priority: First active alarm in this list will control the status LED: alarm 0, 3, 2, 1 */
void updateStatusLed(){
    //alarm 0 - Blue LED flashing, 0.5 Hz frequency
    if(alarmActive[0]){
        if(quarterSeconds % 8 == 0){
            //turn status light on blue
            RGB.color(0,0,255);
        }
        else if(quarterSeconds % 8 == 4){
            //turn status light off
            RGB.color(0,0,0);
        }
    }
    //alarm 3 - Red LED flashing, 2 Hz
    else if(alarmActive[3]){
        if (quarterSeconds % 2 == 0){
            //turn red light on
            RGB.color(255,0,0);
        }
        else{
            //turn red light off
            RGB.color(0,0,0);
        }
    }
    //alarm 2 - Red LED flashing, 1 Hz
    else if(alarmActive[2]){
        if(quarterSeconds % 4 == 0){
            //turn red light on
            RGB.color(255,0,0);
        }
        else if(quarterSeconds % 4 == 2){
            // turn red light off
            RGB.color(0,0,0);
        }
    }
    //alarm 1 - Blue LED flashing, 2 Hz 
    else if(alarmActive[1]){
        if (quarterSeconds % 2 == 0){
            //turn blue light on
            RGB.color(0,0,255);
        }
        else{
            //turn blue light off
            RGB.color(0,0,0);
        }
    }
}

/* Functions to control the functionality of clusterhead actuators */

/* Alarms */
/* checks if the current conditions meet those required for the specified alarm.
    alarmNumber can be 0 - 3, corresponding to the 4 different alarms.  */
bool alarmCondtitionsMet(int alarmNumber){
    switch(alarmNumber){
        case 0:
            //Object movement detected within 25cms
            return (
                currentDistance != 0 
                && currentDistance <= DISTANCE_THRESHOLD
            );
        case 1:
            //Sound Level 55-70 dBA for 30 seconds, light level <100 lux and noise last for more than 30 sec
            return (
                durationAtSoundThresholds[0] >= SOUND_DURATION_THRESHOLDS[0]
                && currentLight < LIGHT_THRESHOLD
            );
        case 2:
            //Sound level > 70 dBA for 10 seconds, light level < 100 lux and noise last for more than 10 sec
            return (
                durationAtSoundThresholds[1] >= SOUND_DURATION_THRESHOLDS[1]
                && currentLight < LIGHT_THRESHOLD
            );
        case 3:
            //Sound level > 80 dBA
            return (currentSound > SOUND_VOLUME_THRESHOLDS[2]);
        default:
            Log.info("@@@@@@ ERROR - invalid alarm number supplied to 'alarmConditionsMet' function. Expected value from 0 - 3, got %d", alarmNumber);
            return false;
    }
}

/* Activates the specified alarm.
    alarmNumber can be 0 - 3, corresponding to the 4 different alarms.  */
void startAlarm(int alarmNumber){
    //check that alarmNumber is valid index
    if(alarmNumber >=0 && alarmNumber <= 3){
        //activate this alarm and record the time it was activated
        alarmActive[alarmNumber] = true;
        alarmActivatedTimes[alarmNumber] = Time.local();

        //do alarm-specific logic
        //which node triggered this alarm?
        uint8_t alarmSourceSensorNodeId = 2;
        switch(alarmNumber){
            case 0:
                alarmSourceSensorNodeId = 1;
                break;
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
        }
        //TODO - update LCD - "alarm at sensor node 1|2"
        Log.info("Activating alarm %d from sensor node %u", alarmNumber, alarmSourceSensorNodeId);
    }
    else{
        Log.info("@@@@@@ ERROR - invalid alarm number supplied to 'startAlarm' function. Expected value from 0 - 3, got %d", alarmNumber);
    }

    
}

/* Resets/deactivates the specified alarm.
    alarmNumber can be 0 - 3, corresponding to the 4 different alarms.  */
void resetAlarm(int alarmNumber){
    //check that alarmNumber is valid index
    if(alarmNumber >=0 && alarmNumber <= 3){
        //record the time elapsed
        long endTime = Time.local();
        int eventDuration = endTime - alarmActivatedTimes[alarmNumber];

        //alarm-specific logic
        uint8_t alarmSensorNodeId = 2; //the sensor node which the alarm originated from
        switch(alarmNumber){
            case 0:
                alarmSensorNodeId = 1;
                break;
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
        }

        //log event information
        Log.info("Alarm %d event triggered by Sensor Node %u at %s. Duration: %d seconds",
            alarmNumber, alarmSensorNodeId, Time.timeStr(alarmActivatedTimes[alarmNumber]), eventDuration);

        //set alarm to inactive
        alarmActive[alarmNumber] = false;
        //turn off alarm light
        RGB.color(0,0,0);
    }
    else{
        Log.info("@@@@@@ ERROR - invalid alarm number supplied to 'resetAlarm' function. Expected value from 0 - 3, got %d", alarmNumber);
    }
}

/* These functions are where we do something with the data (in bytes) we've received via bluetooth */

void onTemperatureReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    int8_t receivedTemp;
    uint64_t sentTime;

    
    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(receivedTemp), sizeof(sentTime));
    //read the temp
    memcpy(&receivedTemp, &data[0], sizeof(receivedTemp));
    

    Log.info("Sensor 1 - Temperature: %u degrees Celsius", receivedTemp);
    getTempsn1 = temperature;
    // Log.info("Temp/humidity transmission delay: %llu seconds", calculateTransmissionDelay(sentTime));
}

void onHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint8_t receivedHumidity;
    memcpy(&receivedHumidity, &data[0], sizeof(receivedHumidity));
    Log.info("Sensor 1 - Humidity: %u%%", receivedHumidity);
}

void onCurrentReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the current sensor reading
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    
    Log.info("Sensor 1 - Current: %u Amps", twoByteValue);
}

void onCurrentReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    //read the current sensor reading
    uint16_t twoByteValue;
    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    
    Log.info("Sensor 2 - Current: %u Amps", twoByteValue);
}

void onDistanceReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint8_t byteValue;
    uint64_t sentTime;

    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(byteValue), sizeof(sentTime));

    memcpy(&byteValue, &data[0], sizeof(uint8_t));
    Log.info("Sensor 1 - Distance: %u cm", byteValue);

    currentDistance = byteValue;
    //activate alarm 0 if it's within 25cm threshold
    if(alarmCondtitionsMet(0)){
        startAlarm(0);
    }
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

    currentLight = twoByteValue;
    // Log.info("Transmission delay: %llu seconds", calculateTransmissionDelay(sentTime));
}

void onSoundReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context){
    uint16_t twoByteValue;
    uint64_t sentTime;

    //read the time of sending, to calculate transmission delay
    memcpy(&sentTime, &data[0] + sizeof(twoByteValue), sizeof(sentTime));

    memcpy(&twoByteValue, &data[0], sizeof(uint16_t));
    Log.info("Sensor 2 - Sound: %u dB", twoByteValue);

    currentSound = twoByteValue;

    //activate alarm 3 immediately if above max volume threshold
    if(alarmCondtitionsMet(3)){
        startAlarm(3);
    }

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