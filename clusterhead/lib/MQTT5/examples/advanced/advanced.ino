#include "MQTT5.h"

MQTT5 client;
SerialLogHandler logHandler(LOG_LEVEL_NONE, {
    { "app", LOG_LEVEL_TRACE }
});
uint16_t pId;

#define TOPIC       "/out"
#define TOPIC_ALIAS 1

void mqttConnectSuccess(bool sessionPresent) {
    Serial.println("Connected successfully");
}

void mqttConnectFailed(MQTT5_REASON_CODE reason) {
    Serial.printlnf("Could not connect due to %d", (int) reason);
}

void mqttPacketDelivered(unsigned int packetId, bool qos2received) {
    Serial.printlnf("Delivery notification for packet %d", (int) packetId);
    if (qos2received && packetId == pId) {
        client.publishRelease(pId);
    }
}

void mqttPublishFailed(MQTT5_REASON_CODE reason) {
    Serial.printlnf("Failed to publish packet because of %d", (int) reason);
}

void mqttSubscribeFailed(MQTT5_REASON_CODE reason) {
    Serial.printlnf("Failed to subscribe because of %d", (int) reason);
}

void mqttPacketReceived(char* topic, uint8_t* payload, uint16_t payloadLength, bool, MQTT5_QOS, bool) {
    char content[payloadLength + 1];
    memcpy(content, payload, payloadLength);
    content[payloadLength] = '\0';
    Serial.printlnf("Received packet for topic %s with payload %s", topic, content);
}

void setup() {
    Serial.begin();
    
    // set optional callbacks
    client.onConnectSuccess(mqttConnectSuccess);
    client.onConnectFailed(mqttConnectFailed);
    client.onDelivery(mqttPacketDelivered);
    client.onPublishFailed(mqttPublishFailed);
    client.onSubscribeFailed(mqttSubscribeFailed);
    client.onPacketReceived(mqttPacketReceived); 
}

void loop() {
    if (Serial.available()) {
        switch (Serial.read()) {
            case 'c':
                if (!client.connected() && !client.connecting()) {
                    MQTT5ConnectProperties props = MQTT5_DEFAULT_CONNECT_PROPERTIES;
                    props.topicAliasMaximum = 10;
                    MQTT5ConnectOptions options = MQTT5_DEFAULT_CONNECT_OPTIONS;
                    options.keepAlive = 30;
                    options.willTopic = "/will";
                    options.willQos = MQTT5_QOS::QOS1;
                    options.willRetain = false;
                    options.willPayload = (const uint8_t*) "Offline";
                    options.willPayloadLength = 7;
                    options.properties = &props;
                    client.connect("test.mosquitto.org", 1883, "client123", options);
                } else {
                    Serial.println("Already connected");
                }
                break;
            case 'p':
                if (client.connected()) {
                    MQTT5PublishProperties props = MQTT5_DEFAULT_PUBLISH_PROPERTIES;
                    props.topicAlias = TOPIC_ALIAS;
                    switch (Serial.read()) {
                        case '1':
                            client.publish(TOPIC, "Hello world QOS1", false, MQTT5_QOS::QOS1, false, NULL, props);
                            break;
                        case '2':
                            client.publish(TOPIC, "Hello world QOS2", false, MQTT5_QOS::QOS2, false, &pId, props);
                            break;
                        case '0':
                        default:
                            client.publish(TOPIC, "Hello world QOS0", false, MQTT5_QOS::QOS0, false, NULL, props);
                            break;
                    }
                } else {
                    Serial.println("Not connected");
                }
                break;
            case 's':
                if (client.connected()) {
                    switch (Serial.read()) {
                        case '1': {
                            MQTT5Subscription sub1 = { "/in1", MQTT5_QOS::QOS1, true, true, MQTT5_RETAIN_HANDLING::SEND_NOT_EXISTS };
                            MQTT5Subscription sub2 = { "/in2", MQTT5_QOS::QOS1, true, true, MQTT5_RETAIN_HANDLING::SEND_NOT_EXISTS };
                            MQTT5Subscription sub3 = { "/in3", MQTT5_QOS::QOS1, true, true, MQTT5_RETAIN_HANDLING::SEND_NOT_EXISTS };
                            MQTT5Subscription subs[] = { sub1, sub2, sub3 };
                            client.subscribe(subs, 3);
                            break;
                        }
                        case '0':
                        default:
                            client.subscribe("/in");
                            break;
                    }
                } else {
                    Serial.println("Not connected");
                }
                break;
            case 'u':
                if (client.connected()) {
                    switch (Serial.read()) {
                        case '1': {
                            const char *topics[3] = { "/in1", "/in2", "/in3" };
                            client.unsubscribe(topics, 3);
                            break;
                        }
                        case '0':
                        default:
                            client.unsubscribe("/in");
                            break;
                    }
                } else {
                    Serial.println("Not connected");
                }
                break;
            case 'd':
                if (client.connecting() || client.connected()) {
                    client.disconnect();
                } else {
                    Serial.println("Already disconnected");
                }
            default:
                break;
        }
    }
    
    client.loop();
}