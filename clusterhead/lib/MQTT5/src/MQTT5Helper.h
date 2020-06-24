#pragma once

enum class MQTT5_QOS: uint8_t {
    QOS0 = 0, // At most once delivery
    QOS1 = 1, // At least once delivery
    QOS2 = 2  // Exactly once delivery
};

enum class MQTT5_RETAIN_HANDLING: uint8_t {
    SEND_AT_SUBSCRIBE   = 0, // Send retained messages at the time of the subscribe
    SEND_NOT_EXISTS     = 1, // Send retained messages at subscribe only if the subscription does not currently exist
    DO_NOT_SEND         = 2  // Do not send retained messages at the time of the subscribe
};

enum class MQTT5_REASON_CODE: uint8_t {
    SUCCESS                                 = 0,
    NORMAL_DISCONNECTION                    = 0,
    GRANTED_QOS_0                           = 0,
    GRANTED_QOS_1                           = 1,
    GRANTED_QOS_2                           = 2,
    DISCONNECT_WITH_WILL_MESSAGE            = 4,
    NO_MATCHING_SUBSCRIBERS                 = 16,
    NO_SUBSCRIPTION_EXISTED                 = 17,
    CONTINUE_AUTHENTICATION                 = 24,
    REAUTHENTICATE                          = 25,
    UNSPECIFIED_ERROR                       = 128,
    MALFORMED_PACKET                        = 129,
    PROTOCOL_ERROR                          = 130,
    IMPLEMENTATION_SPECIFIC_ERROR           = 131,
    UNSUPPORTED_PROTOCOL_VERSION            = 132,
    CLIENT_IDENTIFIER_NOT_VALID             = 133,
    BAD_USER_NAME_OR_PASSWORD               = 134,
    NOT_AUTHORIZED                          = 135,
    SERVER_UNAVAILABLE                      = 136,
    SERVER_BUSY                             = 137,
    BANNED                                  = 138,
    SERVER_SHUTTING_DOWN                    = 139,
    BAD_AUTHENTICATION_METHOD               = 140,
    KEEP_ALIVE_TIMEOUT                      = 141,
    SESSION_TAKEN_OVER                      = 142,
    TOPIC_FILTER_INVALID                    = 143,
    TOPIC_NAME_INVALID                      = 144,
    PACKET_IDENTIFIER_IN_USE                = 145,
    PACKET_IDENTIFIER_NOT_FOUND             = 146,
    RECEIVE_MAXIMUM_EXCEEDED                = 147,
    TOPIC_ALIAS_INVALID                     = 148,
    PACKET_TOO_LARGE                        = 149,
    MESSAGE_RATE_TOO_HIGH                   = 150,
    QUOTA_EXCEEDED                          = 151,
    ADMINISTRATIVE_ACTION                   = 152,
    PAYLOAD_FORMAT_INVALID                  = 153,
    RETAIN_NOT_SUPPORTED                    = 154,
    QOS_NOT_SUPPORTED                       = 155,
    USE_ANOTHER_SERVER                      = 156,
    SERVER_MOVED                            = 157,
    SHARED_SUBSCRIPTIONS_NOT_SUPPORTED      = 158,
    CONNECTION_RATE_EXCEEDED                = 159,
    MAXIMUM_CONNECT_TIME                    = 160,
    SUBSCRIPTION_IDENTIFIERS_NOT_SUPPORTED  = 161,
    WILDCARD_SUBSCRIPTIONS_NOT_SUPPORTED    = 162
};

// 3.1.2.11 CONNECT Properties
typedef struct MQTT5ConnectProperties { 
    uint32_t sessionExpiryInterval;     // 3.1.2.11.2 Session Expiry Interval
    uint16_t receiveMaximum;            // 3.1.2.11.3 Receive Maximum
    uint32_t maximumPacketSize;         // 3.1.2.11.4 Maximum Packet Size
    uint16_t topicAliasMaximum;         // 3.1.2.11.5 Topic Alias Maximum
} MQTT5ConnectProperties;

// 3.1.2 CONNECT Variable Header + 3.1.3 CONNECT Payload
typedef struct MQTT5ConnectOptions { 
    bool cleanStart;                // 3.1.2.4 Clean Start
    MQTT5_QOS willQos;              // 3.1.2.6 Will QoS
    bool willRetain;                // 3.1.2.7 Will Retain
    uint16_t keepAlive;             // 3.1.2.10 Keep Alive
    const char *willTopic;          // 3.1.3.3 Will Topic
    const uint8_t *willPayload;      // 3.1.3.4 Will Payload
    unsigned int willPayloadLength;
    const char *username;           // 3.1.3.5 User Name
    const char *password;           // 3.1.3.6 Password
    const MQTT5ConnectProperties *properties;
} MQTT5ConnectOptions;

// 3.3.2.3 PUBLISH Properties
typedef struct MQTT5PublishProperties { 
    bool payloadFormatIndicator;    // 3.3.2.3.2 Payload Format Indicator
    uint32_t messsageExpiryInterval;// 3.3.2.3.3 Message Expiry Interval
    uint16_t topicAlias;            // 3.3.2.3.4 Topic Alias
    const char *responseTopic;      // 3.3.2.3.5 Response Topic
    const uint8_t *correlationData; // 3.3.2.3.6 Correlation Data
    uint16_t correlationDataLength;
} MQTT5PublishProperties;

typedef struct MQTT5Subscription { 
    const char *topic;
    MQTT5_QOS maxQos;
    bool noLocal;
    bool retain;
    MQTT5_RETAIN_HANDLING retainHandling;
} MQTT5Subscription;

// Default values
static const MQTT5ConnectProperties MQTT5_DEFAULT_CONNECT_PROPERTIES = {
    0, 65535, 0, 10
};
static const MQTT5ConnectOptions MQTT5_DEFAULT_CONNECT_OPTIONS = {
    true, MQTT5_QOS::QOS0, false, 60, NULL, NULL, 0, NULL, NULL, &MQTT5_DEFAULT_CONNECT_PROPERTIES
};
static const MQTT5PublishProperties MQTT5_DEFAULT_PUBLISH_PROPERTIES = {
    0, 0, 0, NULL, NULL, 0
};
