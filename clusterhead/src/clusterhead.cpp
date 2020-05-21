/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "c:/Users/tschw/repos/elec4740Group6/clusterhead/src/clusterhead.ino"
#include "Particle.h"
#include "dct.h"
#include <chrono>
/*
 * clusterhead.ino
 * Description: code to flash to the "clusterhead" argon for assignment 1
 * Author: Tom Schwenke
 * Date: 07/05/2020
 */

// This example does not require the cloud so you can run it in manual mode or
// normal cloud-connected mode
void setup();
void loop();
void monitorAlarms(uint8_t secondsPassed);
void updateSoundThresholdCounters(uint8_t secondsPassed);
void updateStatusLed();
bool alarmCondtitionsMet(int alarmNumber);
void startAlarm(int alarmNumber);
void resetAlarm(int alarmNumber);
void onTemperatureReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onHumidityReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onCurrentReceived1(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onCurrentReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onDistanceReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onTemperatureReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onLightReceived2(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onSoundReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
void onHumanDetectorReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
#line 13 "c:/Users/tschw/repos/elec4740Group6/clusterhead/src/clusterhead.ino"
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

/* Alarm variables */
//The active status of each of the 4 possible alarm conditions. True if active, false if inactive
bool alarmActive [4] = {false, false, false, false};
/* The absolute time in seconds each of the 4 alarms was activated.*/
long long alarmActivatedTimes[4] = {0,0,0,0};
//after this number of seconds without another alarm-worthy event, the alarm will automatically reset.
const uint16_t ALARM_COOLOFF_DELAY = 60;
//number of seconds since each alarm stopped being triggered. If one reaches ALARM_COOLOFF_DELAY, reset that alarm
uint16_t alarmCooloffCounters [4] = {0, 0, 0, 0};

//alarm 1 will be triggered if an object is detected within this many centimetres
const uint16_t DISTANCE_THRESHOLD = 25;
//light must go below this value of Lux to trigger alarms 2 or 3
const uint16_t LIGHT_THRESHOLD = 100;
//no alarm below t0, alarm 2 may trigger between t0-t1, alarm 3 between t1-t2, alarm 4 if > t2
const int16_t SOUND_VOLUME_THRESHOLDS [3] = {55, 70, 80};
//sound must continue for t0 seconds to trigger alarm 2, or t1 seconds for alarm 3
const uint16_t SOUND_DURATION_THRESHOLDS [2] = {30, 10};

//how many seconds has the sound been at a level which can trigger t0=alarm2 or t1=alarm3?
unsigned long durationAtSoundThresholds[2] = {0, 0};



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

// void onDataReceived(const uint8_t* data, size_t len, const BlePeerDevice& peer, void* context);
const size_t SCAN_RESULT_MAX = 30;
BleScanResult scanResults[SCAN_RESULT_MAX];

void setup() {
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
}

void loop() { 
    //do stuff if both sensors have been connected
    if (sensorNode1.connected() && sensorNode2.connected()) {
        //record start time of this loop
        loopStart = millis();

        //update alarm cooloff timers and sound-threshold durations only every 2 seconds
        if(quarterSeconds % 8 == 0){
            //monitor alarms to see if they need to timeout and be reset
            monitorAlarms(2);
            //update sound threshold counters "2" seconds after last update
            updateSoundThresholdCounters(2);
        }

        //flash appropriate colour at appropriate interval for active alarm
        updateStatusLed();
        
        //loop every 250ms, to allow 2Hz status LED flashing if necessary
        //subtract processing time from the delay to make intervals consistent
        delay(250 - (millis() - loopStart));
        quarterSeconds+=1;  
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

/* Update the sound threshold counters after a given amount of seconds has passed
 * @param secondsPassed: number of seconds since this was last called. */
void monitorAlarms(uint8_t secondsPassed){
    //check each of the 4 alarms
    for (uint8_t i = 0; i < 4; i++){
        //if this alarm is still active...
        if(alarmActive[i]){
            //if its conditions are no longer met, increment the cooloff counter
            if(!alarmCondtitionsMet(i)){    
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
based on the values of "quarterSeconds" and "alarmActive[]" 
TODO - add code to actually turn on/off the LED */
void updateStatusLed(){
    if(alarmActive[0]){
        if(quarterSeconds % 8 == 0){
            //turn status light on blue
        }
        else if(quarterSeconds % 8 == 4){
            //turn status light off
        }
    }
    else if(alarmActive[1]){
        if (quarterSeconds % 2 == 0){
            //turn blue light on
        }
        else{
            //turn blue light off
        }
    }
    else if(alarmActive[2]){
        if(quarterSeconds % 4 == 0){
            //turn red light on
        }
        else if(quarterSeconds % 4 == 2){
            // turn red light off
        }
    }
    else if(alarmActive[3]){
        if (quarterSeconds % 2 == 0){
            //turn red light on
        }
        else{
            //turn red light off
        }
    }
}

/* Functions to control the functionality of clusterhead actuators */

/* Alarms */
/* checks if the current conditions meet those required for the specified alarm.
    alarmNumber can be 0 - 3, corresponding to the 4 different alarms.  */
bool alarmCondtitionsMet(int alarmNumber){
    //TODO - add variables so that we can check if these conditions are currently met.
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
    //which node triggered this alarm?
    uint8_t alarmSourceSensorNodeId = 2;
    //check that alarmNumber is valid index
    if(alarmNumber >=0 && alarmNumber <= 3){
        //record the time this alarm was activated
        alarmActivatedTimes[alarmNumber] = Time.now();

        //TODO - activate the appropriate light
        switch(alarmNumber){
            case 0:
                //Blue LED flashing, 0.5 Hz frequency
                alarmSourceSensorNodeId = 1;
                break;
            case 1:
                //Blue LED flashing, 2 Hz 
                break;
            case 2:
                //Red LED flashing, 1 Hz
                break;
            case 3:
                //Red LED flashing, 2 Hz
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
        uint8_t alarmSensorNodeId = 2; //the sensor node which the alarm originated from
        //record the time elapsed
        int eventDuration = Time.now() - alarmActivatedTimes[alarmNumber];
        //TODO - convert alarmActivatedTimes[alarmNumber] to printable local date/time

        //TODO - deactivate the appropriate light
        switch(alarmNumber){
            case 0:
                //Blue LED flashing, 0.5 Hz frequency
                alarmSensorNodeId = 1;
                break;
            case 1:
                //Blue LED flashing, 2 Hz 
                break;
            case 2:
                //Red LED flashing, 1 Hz
                break;
            case 3:
                //Red LED flashing, 2 Hz
                break;
        }

        //log event information
        Log.info("Alarm %d triggered by Sensor Node %u at [converted date/time here]. Duration: %d seconds",
            alarmNumber, alarmSensorNodeId, eventDuration);

        //set alarm to inactive
        alarmActive[alarmNumber] = false;
        //TODO - turn off alarm light
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