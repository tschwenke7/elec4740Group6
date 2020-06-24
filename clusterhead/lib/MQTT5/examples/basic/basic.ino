#include "MQTT5.h"

MQTT5 client;

void mqttFailure(MQTT5_REASON_CODE reason) {
    // See codes here: https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901031
    Serial.printlnf("Failure due to reason %d", (int) reason);
}

void mqttPacketReceived(char* topic, uint8_t* payload, uint16_t payloadLength, bool dup, MQTT5_QOS qos, bool retain) {
    char content[payloadLength + 1];
    memcpy(content, payload, payloadLength);
    content[payloadLength] = 0;

    if (!strcmp(content, "red"))
        RGB.color(255, 0, 0);
    else if (!strcmp(content, "green"))
        RGB.color(0, 255, 0);
    else if (!strcmp(content, "blue"))
        RGB.color(0, 0, 255);
    else
        RGB.color(255, 255, 255);
}

void setup() {
    RGB.control(true);

    client.onConnectFailed(mqttFailure);
    client.onPublishFailed(mqttFailure);
    client.onSubscribeFailed(mqttFailure);
    client.onPacketReceived(mqttPacketReceived);

    /*
    * You can also use IP address:
    * uint8_t server[] = {5, 196, 95, 208};
    * instead of domain:
    * "test.mosquitto.org"
    */
    if (client.connect("test.mosquitto.org", 1883, "client123") && client.awaitPackets()) {
       client.publish("/out", "Hello world");
       client.subscribe("/in");
    }
}

void loop() {
    client.loop();
}