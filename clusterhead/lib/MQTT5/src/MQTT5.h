#pragma once

/* 
 * MQTT5 library by Thomas Peroutka <thomas.peroutka@gmail.com>
 * Github: https://github.com/perotom/MQTT5
 * The implementation is based on https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html
 */

#include "Particle.h"
#include "MQTT5Helper.h"

// Enables the logging of the buffer content when reading and writing
// #define MQTT5_DEBUG

class MQTT5
{
public:
    MQTT5(uint16_t maxPacketSize = 256);

    /**
     * Must be called periodically to keep library working correctly.
     * @return If a packet was read it will return the packet control type,
     *         otherwise 0.
     */
    uint8_t loop();

    /**
     * Disconnects from the server gracefully.
     */
    void disconnect();

    /**
     * Connects to the given server. TCP connection will be created synchronous
     * but the MQTT connect will happen asynchronous. This means the returned value
     * only indicates if the TCP socket was opend successfully and the connect request
     * was sent. It doesn't give feedback whether or not the connect was successfull.
     * Therefore @see onConnectSuccess() and @see onConnectFailed().
     * @return True if packet was sent successfully, otherwise false (only due to exceeded buffer).
     **/
    bool connect(uint8_t *ip, uint16_t port, const char *clientId, MQTT5ConnectOptions options = MQTT5_DEFAULT_CONNECT_OPTIONS);
    bool connect(const char *domain, uint16_t port, const char *clientId, MQTT5ConnectOptions options = MQTT5_DEFAULT_CONNECT_OPTIONS);
    bool connecting();
    bool connected();

    /**
     * Publishes a packet. If using QoS 2 you need to make sure to publish the release for
     * the packet with @see publishRelease(uint16_t, bool) in @see onDelivery(unsigned int, bool).
     * In case of QoS 1 & 2: This function doesn't wait for an answer. Please use either 
     * @see awaitPackets(unsigned long) for synchronous or @see onDelivery(unsigned int, bool)
     * for asynchronous waiting. To handle errors @see onPublishFailed(MQTT5_REASON_CODE).
     * @return True if packet was sent successfully, otherwise false (only due to exceeded buffer).
     **/
    bool publish(const char *topic, const char *payload, MQTT5_QOS qos = MQTT5_QOS::QOS0, uint16_t *packetId = NULL);
    bool publish(const char *topic, const char *payload, bool retain, MQTT5_QOS qos, bool dup, uint16_t *packetId, MQTT5PublishProperties properties = MQTT5_DEFAULT_PUBLISH_PROPERTIES);
    bool publish(const char *topic, const uint8_t *payload, uint16_t payloadLength, bool retain, MQTT5_QOS qos, bool dup, uint16_t *packetId, MQTT5PublishProperties properties = MQTT5_DEFAULT_PUBLISH_PROPERTIES);
    
    /**
     * Only used for QoS 2 packets. Sends the release command for the given packet.
     * @return True if packet was sent successfully, otherwise false (only due to exceeded buffer).
     */
    bool publishRelease(uint16_t packetId, bool notFound = false);

    /**
     * Subscribes the client to given topics. This function doesn't wait for an answer. Please use 
     * @see awaitPackets(unsigned long) for synchronous waiting. To handle errors 
     * @see onSubscribeFailed(MQTT5_REASON_CODE). In case of multiple subscribtions failed, the
     * error callback will be called for every failed subscription with the appropriate reason.
     * @return True if packet was sent successfully, otherwise false (only due to exceeded buffer).
     */
    bool subscribe(const char *topic, MQTT5_QOS maxQos = MQTT5_QOS::QOS2, bool noLocal = true, bool retain = true, MQTT5_RETAIN_HANDLING retainHandling = MQTT5_RETAIN_HANDLING::SEND_NOT_EXISTS);
    bool subscribe(MQTT5Subscription subscriptions[], uint8_t subscriptionsLength);

    /**
     * Unsubscribes the client from the given topics. This function doesn't wait for an answer. 
     * Please use @see awaitPackets(unsigned long) for synchronous waiting. To handle errors 
     * @see onUnsubscribeFailed(MQTT5_REASON_CODE).
     * @return True if packet was sent successfully, otherwise false (only due to exceeded buffer).
     */
    bool unsubscribe(const char *topic);
    bool unsubscribe(const char *topic[], uint8_t topicLength);

    /**
     * @return The amount of packets that are sent but no 
     *         acknowledge was yet received. Doesn't track
     *         ping req/res.
     */
    uint8_t packetsAwaitingAck();

    /**
     * Waits for packetsAwaitingAck() to be zero.
     * @param timeout The timeout in milliseconds.
     * @return True if all packets are acknowledged within
     *         the timeout, otherwise false.
     */
    bool awaitPackets(unsigned long timeout = 5000UL);

    // Set callbacks
    /**
     * Gets called when the client was connected successfully
     * to the server. 
     * @param sessionPresent 3.2.2.1.1 Session Present.
     */
    void onConnectSuccess(void (*callback)(bool sessionPresent));

    /**
     * Gets called when the connect failed. TCP connection
     * is already closed.
     */
    void onConnectFailed(void (*callback)(MQTT5_REASON_CODE));

    /**
     * Gets called when a message was delivered at the server for 
     * QoS 1 & 2. For QoS 2, which is indicated by the @param qos2received,
     * you need to send @see publishRelease(uint16_t, bool).
     */
    void onDelivery(void (*callback)(unsigned int packetId, bool qos2received));

    /**
     * Gets called when the server responds with a failure when
     * receiving the packet.
     */
    void onPublishFailed(void (*callback)(MQTT5_REASON_CODE));

    /**
     * Gets called when the server responds with a failure when
     * subscribing to topic.
     */
    void onSubscribeFailed(void (*callback)(MQTT5_REASON_CODE));

    /**
     * Gets called when the server responds with a failure when
     * unsubscribing from a topic.
     */
    void onUnsubscribeFailed(void (*callback)(MQTT5_REASON_CODE));

    /**
     * Gets called when a packet is received from the server.
     */
    void onPacketReceived(void (*callback)(char* topic, uint8_t* payload, uint16_t payloadLength, bool dup, MQTT5_QOS qos, bool retain));

    ~MQTT5();

private:
    // Variables
    Logger logger;
    TCPClient *socket;
    uint8_t *buffer;
    const char *hostDomain;
    uint8_t *hostIp;
    uint16_t hostPort;
    unsigned long lastOutbound;
    unsigned long lastInbound;
    unsigned long lastPingSent;
    uint8_t pingRetries;
    uint16_t keepAlive;
    bool isConnecting = false;
    uint16_t nextPacketId;
    uint16_t maxPacketSize;
    uint8_t maxTopicAlias = DEFAULT_MAX_TOPIC_ALIAS;
    uint16_t *registeredTopicAlias = NULL;
    uint8_t registeredTopicAliasLen;
    uint8_t packetsInFlight;

    // Callbacks
    void (*callbackConnectSuccess)(bool sessionPresent);
    void (*callbackConnectFailed)(MQTT5_REASON_CODE reason);
    void (*callbackQOS)(unsigned int packetId, bool qos2received);
    void (*callbackPublishFailed)(MQTT5_REASON_CODE reason);
    void (*callbackSubscribeFailed)(MQTT5_REASON_CODE reason);
    void (*callbackUnsubscribeFailed)(MQTT5_REASON_CODE reason);
    void (*callbackPacketReceived)(char*, uint8_t*, uint16_t, bool, MQTT5_QOS, bool);

    // Functions
    bool connect(const char *clientId, MQTT5ConnectOptions options);
    bool pubAck(uint16_t packetId);
    bool pubRec(uint16_t packetId);
    bool pubComp(uint16_t packetId);
    bool ping();
    bool disconnectWithReason(MQTT5_REASON_CODE reason);

    void close();

    bool topicAliasRegister(uint16_t alias);
    bool topicAliasRegistered(uint16_t alias);

    /**
     * Processes a packet from the buffer. Starts at @param startIndex.
     */
    void processPacket(uint8_t packetType, uint8_t flags, uint16_t startIndex, uint16_t contentLength);
    void processPacketConnAck(uint8_t flags, uint16_t startIndex, uint16_t contentLength);
    void processPacketConnAckProperties(uint8_t identifier, uint16_t *index);
    void processPacketPub(uint8_t flags, uint16_t startIndex, uint16_t contentLength);
    void processPacketPubProperties(uint8_t identifier, uint16_t *index);
    void processPacketPubAckRec(uint8_t flags, uint16_t startIndex, uint16_t contentLength, bool isAck);
    void processPacketPubRel(uint8_t flags, uint16_t startIndex, uint16_t contentLength);
    void processPacketPubComp(uint8_t flags, uint16_t startIndex, uint16_t contentLength);
    void processPacketSubAck(uint8_t flags, uint16_t startIndex, uint16_t contentLength);
    void processPacketUnsub(uint8_t flags, uint16_t startIndex, uint16_t contentLength);
    void processPacketPingResp(uint8_t flags, uint16_t startIndex, uint16_t contentLength);
    void processPacketDisconnect(uint8_t flags, uint16_t startIndex, uint16_t contentLength);
    
    /**
     * This functions reads a variable byte integer (according to 1.5.5) into value
     * from the buffer and advances the @param position.
     * @param position The start position of the variable integer to read.
     * @param value The value in which to store the integer.
     * @return True if integer was read, otherwise false.
     */
    bool readVariableByteInteger(uint16_t *position, uint16_t *value);

    uint16_t readUTF8StringLength(uint16_t *position);
    void readUTF8String(uint16_t *position, char *arr, uint16_t len);

    /**
     * This function reads a 2 byte integer from the buffer and advances the @param position.
     **/
    uint16_t readInt(uint16_t *position);

    /**
     * This function reads a 4 byte integer from the buffer and advances the @param position.
     **/
    uint32_t readLong(uint16_t *position);

    /**
     * Writes the buffer to the socket.
     * @return True if written bytes to socket equals the content length,
     *         otherwise false.
     */
    bool writeToSocket(uint16_t length, bool increasePacketsInFlight = true);

    /**
     * This function writes a variable byte integer (according to 1.5.5) into the buffer 
     * at the given position. Shifts the content after the inserted integer. Only content
     * within the range of @param length will be shifted so make sure no content is after
     * that.
     * @param position The position where the integer will be put and
     *                 the content will be shifted. Will be advanced to
     *                 the next position after the content
     * @param length The length of the content to shift.
     * @return False if packet size is exceeded, otherwise true.
     */
    bool writeVariableByteInteger(uint16_t *position, uint16_t length);

    /**
     * Writes a UTF-8 string into the buffer.
     * @param position The position where the string will be put and
     *                 the content will be shifted. Will be advanced to
     *                 the next position after the content
     * @return False if packet size is exceeded, otherwise true.
     */
    bool writeUTF8String(uint16_t *position, const char* str, uint16_t length);

    /**
     * Writes single byte.
     * @param position The position where the byte will be put and
     *                 the content will be shifted. Will be advanced to
     *                 the next position after the content
     * @return False if packet size is exceeded, otherwise true.
     */
    bool writeByte(uint16_t *position, uint8_t content);
        
    /**
     * Writes multiple bytes.
     * @param position The position where the bytes will be put and
     *                 the content will be shifted. Will be advanced to
     *                 the next position after the content
     * @return False if packet size is exceeded, otherwise true.
     */
    bool writeBytes(uint16_t *position, const uint8_t *payload, uint16_t payloadLength);

    /**
     * Writes an integer.
     * @param position The position where the integer will be put and
     *                 the content will be shifted. Will be advanced to
     *                 the next position after the content
     * @return False if packet size is exceeded, otherwise true.
     */
    bool writeInt(uint16_t *position, uint16_t content);

    /**
     * Writes an integer.
     * @param position The position where the integer will be put and
     *                 the content will be shifted. Will be advanced to
     *                 the next position after the content
     * @return False if packet size is exceeded, otherwise true.
     */
    bool writeInt(uint16_t *position, uint32_t content);

    // Constants
    static const unsigned long PING_TIMEOUT = 3000UL; // If no ping response happens within this millisecond period another ping request is sent
    static const unsigned int DEFAULT_MAX_TOPIC_ALIAS = 10;

    // 2.1.2 MQTT Control Packet type
    static const uint8_t CTRL_CONNECT      = 1; // Connection request
    static const uint8_t CTRL_CONNACK      = 2; // Connect acknowledgment
    static const uint8_t CTRL_PUBLISH      = 3; // Publish message
    static const uint8_t CTRL_PUBACK       = 4; // Publish acknowledgment (QoS 1)
    static const uint8_t CTRL_PUBREC       = 5; // Publish received (QoS 2 delivery part 1)
    static const uint8_t CTRL_PUBREL       = 6; // Publish release (QoS 2 delivery part 2)
    static const uint8_t CTRL_PUBCOMP      = 7; // Publish complete (QoS 2 delivery part 3)
    static const uint8_t CTRL_SUBSCRIBE    = 8; // Subscribe request
    static const uint8_t CTRL_SUBACK       = 9; // Subscribe acknowledgment
    static const uint8_t CTRL_UNSUBSCRIBE  = 10; // Unsubscribe request
    static const uint8_t CTRL_UNSUBACK     = 11; // Unsubscribe acknowledgment
    static const uint8_t CTRL_PINGREQ      = 12; // PING request
    static const uint8_t CTRL_PINGRESP     = 13; // PING response
    static const uint8_t CTRL_DISCONNECT   = 14; // Disconnect notification
    static const uint8_t CTRL_AUTH         = 15; // Authentication exchange

    // Variable header properties
    static const uint8_t PROP_PAYLOAD_FORMAT_INDICATOR	        = 0x01;
    static const uint8_t PROP_MESSAGE_EXPIRY_INTERVAL	        = 0x02;
    static const uint8_t PROP_CONTENT_TYPE	                    = 0x03;
    static const uint8_t PROP_RESPONSE_TOPIC	                = 0x08;
    static const uint8_t PROP_CORRELATION_DATA	                = 0x09;
    static const uint8_t PROP_SUBSCRIPTION_IDENTIFIER	        = 0x0B;
    static const uint8_t PROP_SESSION_EXPIRY_INTERVAL	        = 0x11;
    static const uint8_t PROP_ASSIGNED_CLIENT_IDENTIFIER	    = 0x12;
    static const uint8_t PROP_SERVER_KEEP_ALIVE	                = 0x13;
    static const uint8_t PROP_AUTHENTICATION_METHOD	            = 0x15;
    static const uint8_t PROP_AUTHENTICATION_DATA	            = 0x16;
    static const uint8_t PROP_REQUEST_PROBLEM_INFORMATION	    = 0x17;
    static const uint8_t PROP_WILL_DELAY_INTERVAL	            = 0x18;
    static const uint8_t PROP_REQUEST_RESPONSE_INFORMATION	    = 0x19;
    static const uint8_t PROP_RESPONSE_INFORMATION	            = 0x1A;
    static const uint8_t PROP_SERVER_REFERENCE	                = 0x1C;
    static const uint8_t PROP_REASON_STRING	                    = 0x1F;
    static const uint8_t PROP_RECEIVE_MAXIMUM	                = 0x21;
    static const uint8_t PROP_TOPIC_ALIAS_MAXIMUM	            = 0x22;
    static const uint8_t PROP_TOPIC_ALIAS	                    = 0x23;
    static const uint8_t PROP_MAXIMUM_QOS	                    = 0x24;
    static const uint8_t PROP_RETAIN_AVAILABLE          	    = 0x25;
    static const uint8_t PROP_USER_PROPERTY	                    = 0x26;
    static const uint8_t PROP_MAXIMUM_PACKET_SIZE	            = 0x27;
    static const uint8_t PROP_WILDCARD_SUBSCRIPTION_AVAILABLE	= 0x28;
    static const uint8_t PROP_SUBSCRIPTION_IDENTIFIER_AVAILABLE	= 0x29;
    static const uint8_t PROP_SHARED_SUBSCRIPTION_AVAILABLE	    = 0x2A;

};
