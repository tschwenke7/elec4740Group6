/* MQTT5 library by Thomas Peroutka <thomas.peroutka@gmail.com>
 */

#include "MQTT5.h"

MQTT5::MQTT5(uint16_t maxp): logger("app.MQTT") {
    maxPacketSize = maxp;
    socket = new TCPClient();
    buffer = new uint8_t[maxPacketSize];
}

bool MQTT5::connected() {
    return !connecting() && socket->connected();
}

bool MQTT5::connecting() {
    return isConnecting && socket->connected();
}

uint8_t MQTT5::packetsAwaitingAck() {
    return packetsInFlight;
}

bool MQTT5::awaitPackets(unsigned long timeout) {
    unsigned long t = millis();
    logger.trace("Await packet acks");
    while (millis() - t < timeout) {
        loop();
        if (packetsAwaitingAck() == 0) {
            logger.trace("All packets acknowledged");
            return true;
        }
    }
    logger.trace("Await timed out");
    return false;
}

uint8_t MQTT5::loop() {
    // Check if connecting takes too long
    if (connecting()) {
        if (!socket->connected()) {
            close();
            logger.info("Socked closed");
            return 0;
        } else if (millis() - lastOutbound > keepAlive*1000UL) {
            close();
            logger.info("Connect timed out");
            if (callbackConnectFailed)
                callbackConnectFailed(MQTT5_REASON_CODE::UNSPECIFIED_ERROR);
            return 0;
        }
    }

    // Read data from socket
    if (socket->available()) {
        lastInbound = millis();
        uint16_t readLength = socket->read(buffer, maxPacketSize);

#ifdef MQTT5_DEBUG
        logger.trace("Read %d bytes:", readLength);
        logger.dump(buffer, readLength);
        logger.print("\n");
#endif

        uint16_t index = 1;
        uint16_t contentLength = 0;
        
        if (!readVariableByteInteger(&index, &contentLength)) {
            return 0;
        } else if (readLength != contentLength + index) {
            logger.warn("Packet not complete");
            return 0;
        }

        uint8_t packetType = (buffer[0] >> 4);
        if (packetType != CTRL_PINGRESP && 
            packetType != CTRL_PUBLISH && 
            packetType != CTRL_DISCONNECT)
            packetsInFlight--;
            
        logger.trace("Received packet type %d with length %d", packetType, contentLength);
        processPacket(packetType, buffer[0] & 0xF, index, contentLength);
        return packetType;
    }
    
    if (!connected())
        return 0;

    // Check keep alive interval
    if ((millis() - lastInbound >= keepAlive*1000UL) || (millis() - lastOutbound >= keepAlive*1000UL)) {
        if (lastPingSent == 0 && !ping()) {
            logger.warn("Ping could not be sent");
        }
    }

    // Check if ping needs retry
    if (lastPingSent != 0 && millis() - lastPingSent >= PING_TIMEOUT && pingRetries < 2) {
        pingRetries++;
        logger.info("Ping response not received yet. Resending ping request");
        if (!ping()) {
            logger.warn("Ping could not be sent");
        }
    }
    return 0;
}

void MQTT5::processPacket(uint8_t packetType, uint8_t flags, uint16_t index, uint16_t contentLength) {
    switch (packetType) {
        case CTRL_CONNACK:
            processPacketConnAck(flags, index, contentLength);
            break;
        case CTRL_PUBLISH:
            processPacketPub(flags, index, contentLength);
            break;
        case CTRL_PUBACK:
        case CTRL_PUBREC:
            processPacketPubAckRec(flags, index, contentLength, packetType == CTRL_PUBACK);
            break;
        case CTRL_PUBREL:
            processPacketPubRel(flags, index, contentLength);
            break;
        case CTRL_PUBCOMP:
            processPacketPubComp(flags, index, contentLength);
            break;
        case CTRL_SUBACK:
            processPacketSubAck(flags, index, contentLength);
            break;
        case CTRL_UNSUBACK:
            processPacketUnsub(flags, index, contentLength);
            break;
        case CTRL_PINGRESP:
            processPacketPingResp(flags, index, contentLength);
            break;
        case CTRL_DISCONNECT:
            processPacketDisconnect(flags, index, contentLength);
            break;
        default: 
            logger.info("Unkown control packet type %d", packetType);   
            break;
    }
}

void MQTT5::processPacketConnAck(uint8_t flags, uint16_t index, uint16_t contentLength) {
    isConnecting = false;
    bool sessionPresent = buffer[index++] & 0x1;
    MQTT5_REASON_CODE reasonCode = (MQTT5_REASON_CODE) buffer[index++];
    if (reasonCode == MQTT5_REASON_CODE::SUCCESS) {
        uint16_t propertiesLength;
        if (readVariableByteInteger(&index, &propertiesLength) && propertiesLength > 0) {
            uint16_t propertiesStartIndex = index;
            while (index - propertiesStartIndex < propertiesLength) {
                processPacketConnAckProperties(buffer[index++], &index);
            }
        }
        lastPingSent = 0;
        pingRetries = 0;
        nextPacketId = 1;
        // init topic alias array
        registeredTopicAlias = (uint16_t*) malloc(maxTopicAlias * sizeof(uint16_t));
        registeredTopicAliasLen = 0;
        logger.info("Successfully connected");
        if (callbackConnectSuccess)
            callbackConnectSuccess(sessionPresent);
    } else {
        close();
        logger.info("Failed to connect. Reason: %d", (uint8_t) reasonCode);   
        if (callbackConnectFailed)
            callbackConnectFailed(reasonCode);
    }
}

void MQTT5::processPacketConnAckProperties(uint8_t identifier, uint16_t *index) {
    switch (identifier) {
        case PROP_SESSION_EXPIRY_INTERVAL:
            logger.trace("Session Expiry Interval: %ld", readLong(index));
            break;
        case PROP_RECEIVE_MAXIMUM:
            logger.trace("Receive Maximum: %d", readInt(index));
            break;
        case PROP_MAXIMUM_QOS:
            logger.trace("Maximum QoS: %d", buffer[(*index)++]);
            break;
        case PROP_RETAIN_AVAILABLE:
            logger.trace("Retain Available: %d", buffer[(*index)++]);
            break;
        case PROP_MAXIMUM_PACKET_SIZE:
            logger.trace("Maximum Packet Size: %ld", readLong(index));
            break;
        case PROP_ASSIGNED_CLIENT_IDENTIFIER: {
            uint16_t len = readUTF8StringLength(index);
            char str[len + 1];
            readUTF8String(index, str, len);
            logger.trace("Assigned Client Identifier: %s", str);
            break;
        }
        case PROP_TOPIC_ALIAS_MAXIMUM:
            maxTopicAlias = min(maxTopicAlias , readInt(index));
            logger.trace("Topic Alias Maximum: %d", maxTopicAlias);
            break;
        case PROP_REASON_STRING: {
            uint16_t len = readUTF8StringLength(index);
            char str[len + 1];
            readUTF8String(index, str, len);
            logger.trace("Reason String: %s", str);
            break;
        }
        case PROP_USER_PROPERTY:
            logger.trace("User Property");
            break;
        case PROP_WILDCARD_SUBSCRIPTION_AVAILABLE:
            logger.trace("Wildcard Subscription Available: %d", buffer[(*index)++]);
            break;
        case PROP_SUBSCRIPTION_IDENTIFIER_AVAILABLE:
            logger.trace("Subscription Identifiers Available: %d", buffer[(*index)++]);
            break;
        case PROP_SHARED_SUBSCRIPTION_AVAILABLE:
            logger.trace("Shared Subscription Available: %d", buffer[(*index)++]);
            break;
        case PROP_SERVER_KEEP_ALIVE:
            keepAlive = readInt(index);
            logger.trace("Server Keep Alive: %d", keepAlive);
            break;
        case PROP_RESPONSE_INFORMATION: {
            uint16_t len = readUTF8StringLength(index);
            char str[len + 1];
            readUTF8String(index, str, len);
            logger.trace("Response Information: %s", str);
            break;
        }
        case PROP_SERVER_REFERENCE: {
            uint16_t len = readUTF8StringLength(index);
            char str[len + 1];
            readUTF8String(index, str, len);
            logger.trace("Server Reference: %s", str);
            break;
        }
        case PROP_AUTHENTICATION_METHOD: {
            uint16_t len = readUTF8StringLength(index);
            char str[len + 1];
            readUTF8String(index, str, len);
            logger.trace("Authentication Method: %s", str);
            break;
        }
        case PROP_AUTHENTICATION_DATA:
            logger.trace("Authentication Data");
            break;
        default:
            logger.warn("Unknown connect property");
            break;
    }
}

void MQTT5::processPacketPub(uint8_t flags, uint16_t startIndex, uint16_t contentLength) {
    // 3.3.1 PUBLISH Fixed Header
    bool dup = (flags & 0x8) > 0;
    MQTT5_QOS qos = (MQTT5_QOS) ((flags & 0x6) >> 1);
    bool retain = (flags & 0x1) > 0;
    uint16_t indexContentStart = startIndex;

    // 3.3.2.1 Topic Name
    uint16_t len = readUTF8StringLength(&startIndex);
    char topic[len + 1];
    readUTF8String(&startIndex, topic, len);
    
    // 3.3.2.2 Packet Identifier
    uint16_t packetId = 0;
    if (qos != MQTT5_QOS::QOS0)
        packetId = readInt(&startIndex);

    // 3.3.2.3.1 Property Length
    uint16_t propertiesLength;
    if (readVariableByteInteger(&startIndex, &propertiesLength) && propertiesLength > 0) {
        uint16_t propertiesStartIndex = startIndex;
        while (startIndex - propertiesStartIndex < propertiesLength) {
            processPacketPubProperties(buffer[startIndex++], &startIndex);
        }
    }

    if (qos != MQTT5_QOS::QOS0)
        logger.info("Received packet for topic %s with id %d and length %d", topic, packetId, (contentLength + indexContentStart) - startIndex);
    else
        logger.info("Received packet for topic %s with length %d", topic, (contentLength + indexContentStart) - startIndex);
    
    if (callbackPacketReceived)
        callbackPacketReceived(topic, buffer + startIndex, (contentLength + indexContentStart) - startIndex, dup, qos, retain);

    // Send acknowledge
    if (qos == MQTT5_QOS::QOS1)
        pubAck(packetId);
    else if (qos == MQTT5_QOS::QOS2)
        pubRec(packetId);
}

void MQTT5::processPacketPubProperties(uint8_t identifier, uint16_t *index) {
    switch (identifier) {
        case PROP_PAYLOAD_FORMAT_INDICATOR:
            logger.trace("Payload Format Indicator: %d", buffer[(*index)++]);
            break;
        case PROP_MESSAGE_EXPIRY_INTERVAL:
            logger.trace("Message Expiry Interval: %ld", readLong(index));
            break;
        case PROP_TOPIC_ALIAS:
            logger.trace("Topic Alias: %d", readInt(index));
            break;
        case PROP_RESPONSE_TOPIC: {
            uint16_t len = readUTF8StringLength(index);
            char str[len + 1];
            readUTF8String(index, str, len);
            logger.trace("Response Topic: %s", str);
            break;
        }
        case PROP_CORRELATION_DATA:
            logger.trace("Correlation Data");
            break;
        case PROP_USER_PROPERTY:
            logger.trace("User Property");
            break;
        case PROP_SUBSCRIPTION_IDENTIFIER:
            logger.trace("Subscription Identifier");
            break;
        case PROP_CONTENT_TYPE: {
            uint16_t len = readUTF8StringLength(index);
            char str[len + 1];
            readUTF8String(index, str, len);
            logger.trace("Content Type: %s", str);
            break;
        }
        default:
            logger.warn("Unknown publish property");
            break;
    }
}

void MQTT5::processPacketPubAckRec(uint8_t flags, uint16_t startIndex, uint16_t contentLength, bool isAck) {
    uint16_t packetId = readInt(&startIndex);
    MQTT5_REASON_CODE reason = contentLength == 2 ? MQTT5_REASON_CODE::SUCCESS : (MQTT5_REASON_CODE) buffer[startIndex++];
    logger.info("Received publish %s for packet %d and response %d", isAck ? "ack" : "rec", packetId, (uint8_t) reason);

    if (reason != MQTT5_REASON_CODE::SUCCESS) {
        if (callbackPublishFailed)
            callbackPublishFailed(reason);
    } else if (callbackQOS) {
        callbackQOS(packetId, !isAck);
    }
}

void MQTT5::processPacketPubRel(uint8_t flags, uint16_t startIndex, uint16_t contentLength) {
    uint16_t packetId = readInt(&startIndex);
    MQTT5_REASON_CODE reason = contentLength == 2 ? MQTT5_REASON_CODE::SUCCESS : (MQTT5_REASON_CODE) buffer[startIndex++];
    logger.info("Received publish rel for packet %d and response %d", packetId, (uint8_t) reason);
    if (reason == MQTT5_REASON_CODE::SUCCESS) {
        pubComp(packetId);
    }
}

void MQTT5::processPacketPubComp(uint8_t flags, uint16_t startIndex, uint16_t contentLength) {
    uint16_t packetId = readInt(&startIndex);
    MQTT5_REASON_CODE reason = contentLength == 2 ? MQTT5_REASON_CODE::SUCCESS : (MQTT5_REASON_CODE) buffer[startIndex++];
    logger.info("Received publish complete for packet %d and response %d", packetId, (uint8_t) reason);
}

void MQTT5::processPacketSubAck(uint8_t flags, uint16_t startIndex, uint16_t contentLength) {
    uint16_t packetId = readInt(&startIndex);
    MQTT5_REASON_CODE reason = (MQTT5_REASON_CODE) buffer[startIndex++];
    if (reason != MQTT5_REASON_CODE::SUCCESS) {
        if (callbackSubscribeFailed)
            callbackSubscribeFailed(reason);
    } else {
        for (int i = 1; i < contentLength - 2; i++) { // skip first packet 
            MQTT5_REASON_CODE reason = (MQTT5_REASON_CODE) buffer[startIndex++];
            logger.info("Received subscription ack for packet %d and subscription index %d and response %d", packetId, i, (uint8_t) reason);
            if (reason != MQTT5_REASON_CODE::GRANTED_QOS_0 &&
                reason != MQTT5_REASON_CODE::GRANTED_QOS_1 &&
                reason != MQTT5_REASON_CODE::GRANTED_QOS_2)
                if (callbackSubscribeFailed)
                    callbackSubscribeFailed(reason);
        }
    }
}

void MQTT5::processPacketUnsub(uint8_t flags, uint16_t startIndex, uint16_t contentLength) {
    uint16_t packetId = readInt(&startIndex);
    MQTT5_REASON_CODE reason = (MQTT5_REASON_CODE) buffer[startIndex++];
    if (reason != MQTT5_REASON_CODE::SUCCESS) {
        if (callbackUnsubscribeFailed)
            callbackUnsubscribeFailed(reason);
    } else {
        for (int i = 1; i < contentLength - 2; i++) { // skip first packet 
            MQTT5_REASON_CODE reason = (MQTT5_REASON_CODE) buffer[startIndex++];
            logger.info("Received unsubscription ack for packet %d and subscription index %d and response %d", packetId, i, (uint8_t) reason);
            if (reason != MQTT5_REASON_CODE::GRANTED_QOS_0 &&
                reason != MQTT5_REASON_CODE::GRANTED_QOS_1 &&
                reason != MQTT5_REASON_CODE::GRANTED_QOS_2)
                if (callbackUnsubscribeFailed)
                    callbackUnsubscribeFailed(reason);
        }
    }
}

void MQTT5::processPacketPingResp(uint8_t flags, uint16_t startIndex, uint16_t contentLength) {
    lastPingSent = 0;
    pingRetries = 0;
}

void MQTT5::processPacketDisconnect(uint8_t flags, uint16_t startIndex, uint16_t contentLength) {
    MQTT5_REASON_CODE reason = contentLength == 0 ? MQTT5_REASON_CODE::SUCCESS : (MQTT5_REASON_CODE) buffer[startIndex++];
    logger.info("Received disconnect from server due to reason %d", (uint8_t) reason);
    close();
}

bool MQTT5::connect(uint8_t *ip, uint16_t port, const char *clientId, MQTT5ConnectOptions options) {
    hostIp = ip;
    hostDomain = NULL;
    hostPort = port;
    return connect(clientId, options);
}
bool MQTT5::connect(const char *domain, uint16_t port, const char *clientId, MQTT5ConnectOptions options) {
    hostIp = NULL;
    hostDomain = domain;
    hostPort = port;
    return connect(clientId, options);
}

bool MQTT5::connect(const char *clientId, MQTT5ConnectOptions options) {
    if (connecting())
        return true;
    
    keepAlive = options.keepAlive;
    if (!connected()) {
        isConnecting = true;
        bool success = true;
        if (hostIp == NULL) { // Use domain to connect
            logger.info("Connecting to server %s on port %d", hostDomain, hostPort);
            success = socket->connect(hostDomain, hostPort);
        } else { // Use ip address to connect
            logger.info("Connecting to server %d.%d.%d.%d on port %d", hostIp[0], hostIp[1], hostIp[2], hostIp[3], hostPort);
            success = socket->connect(hostIp, hostPort);
        }

        // Check if TCP connection was open successfully
        if (!success) {
            logger.info("Could not open TCP connection to server");
            close();
            return false;
        }
        
        packetsInFlight = 0;
        uint16_t index = 0;
        // 3.1.1 CONNECT Fixed Header
        if (!writeByte(&index, (uint8_t) (CTRL_CONNECT << 4))) {
            close();
            return false;
        }

        // 3.1.2 CONNECT Variable Header
        if (!writeUTF8String(&index, (const char[]) {'M','Q','T','T'}, 4)) {
            close();
            return false;
        }

        // 3.1.2.2 Protocol Version
        if (!writeByte(&index, 5)) {
            close();
            return false;
        }
        
        // 3.1.2.3 Connect Flags
        if (!writeByte(&index, 
            ((options.username != NULL) << 7) | 
            ((options.password != NULL) << 6) | 
            (options.willRetain << 5) | 
            (((uint8_t) options.willQos) << 3) | 
            ((options.willTopic && options.willPayload) << 2) | 
            (options.cleanStart << 1))) {
            close();
            return false;
        }

        // 3.1.2.10 Keep Alive
        if (!writeInt(&index, keepAlive)) {
            close();
            return false;
        }

        // 3.1.2.11 CONNECT Properties
        uint16_t indexProp = index;
        if (options.properties) {
            // 3.1.2.11.2 Session Expiry Interval
            if ((*options.properties).sessionExpiryInterval > 0) {
                if (!writeByte(&indexProp, PROP_SESSION_EXPIRY_INTERVAL)) {
                    close();
                    return false;
                }
                if (!writeInt(&indexProp, (*options.properties).sessionExpiryInterval)) {
                    close();
                    return false;
                }
            }

            // 3.1.2.11.3 Receive Maximum
            if ((*options.properties).receiveMaximum > 0) {
                if (!writeByte(&indexProp, PROP_RECEIVE_MAXIMUM)) {
                    close();
                    return false;
                }
                if (!writeInt(&indexProp, (*options.properties).receiveMaximum)) {
                    close();
                    return false;
                }
            }

            // 3.1.2.11.4 Maximum Packet Size
            uint32_t usedMaxPacketSize = (uint32_t) (
                (*options.properties).maximumPacketSize > 0 ?
                (*options.properties).maximumPacketSize : maxPacketSize);
            if (usedMaxPacketSize > 0) {
                if (!writeByte(&indexProp, PROP_MAXIMUM_PACKET_SIZE)) {
                    close();
                    return false;
                }
                if (!writeInt(&indexProp, usedMaxPacketSize)) {
                    close();
                    return false;
                }
            }

            // 3.1.2.11.5 Topic Alias Maximum
            if ((*options.properties).topicAliasMaximum > 0) {
                if (!writeByte(&indexProp, PROP_TOPIC_ALIAS_MAXIMUM)) {
                    close();
                    return false;
                }
                if (!writeInt(&indexProp, (*options.properties).topicAliasMaximum)) {
                    close();
                    return false;
                }
            }
        }

        // 3.1.2.11 CONNECT Properties length
        if (!writeVariableByteInteger(&index, indexProp - index)) {
            close();
            return false;
        }

        // 3.1.3.1 Client Identifier
        if (!writeUTF8String(&index, clientId, strlen(clientId))) {
            close();
            return false;
        }

        if (options.willTopic && options.willPayload) {
            // 3.1.3.2 Will Properties
            if (!writeVariableByteInteger(&index, 0)) {
                close();
                return false;
            }
            
            // 3.1.3.3 Will Topic
            if (!writeUTF8String(&index, options.willTopic, strlen(options.willTopic))) {
                close();
                return false;
            }

            // 3.1.3.4 Will Payload
            if (!writeUTF8String(&index, (const char*) options.willPayload, options.willPayloadLength)) {
                close();
                return false;
            }
        }

        // 3.1.3.5 User Name
        if (options.username && !writeUTF8String(&index, options.username, strlen(options.username))) {
            close();
            return false;
        }

        // 3.1.3.6 Password
        if (options.password && !writeUTF8String(&index, options.password, strlen(options.password))) {
            close();
            return false;
        }

        // 3.1.1 CONNECT Fixed Header remaining length
        uint16_t len = index - 1;
        index = 1;
        if (!writeVariableByteInteger(&index, len)) {
            close();
            return false;
        }

        return writeToSocket(index);
    } else { // Deny second connect packet as connection will be closed by the server
        logger.info("Already connected to server");
        return false;
    }
}

bool MQTT5::pubAck(uint16_t packetId) {
    uint16_t index = 0;
    // 3.4.1 PUBACK Fixed Header
    if (!writeByte(&index, (uint8_t) (CTRL_PUBACK << 4)))
        return false;

    // 3.4.2 PUBACK Variable Header
    if (!writeInt(&index, packetId))
        return false;

    // 3.4.1 PUBACK Fixed Header remaining length
    uint16_t len = index - 1;
    index = 1;
    if (!writeVariableByteInteger(&index, len))

    logger.info("Sending publish ack for packet %d", packetId);
    return writeToSocket(index, false);
}
bool MQTT5::pubRec(uint16_t packetId) {
    uint16_t index = 0;
    // 3.5.1 PUBREC Fixed Header
    if (!writeByte(&index, (uint8_t) (CTRL_PUBREC << 4)))
        return false;

    // 3.5.2 PUBREC Variable Header
    if (!writeInt(&index, packetId))
        return false;

    // 3.5.1 PUBREC Fixed Header remaining length
    uint16_t len = index - 1;
    index = 1;
    if (!writeVariableByteInteger(&index, len))

    logger.info("Sending publish rec for packet %d", packetId);
    return writeToSocket(index, false);
}
bool MQTT5::pubComp(uint16_t packetId) {
    uint16_t index = 0;
    // 3.7.1 PUBCOMP Fixed Header
    if (!writeByte(&index, (uint8_t) (CTRL_PUBCOMP << 4)))
        return false;

    // 3.7.2 PUBCOMP Variable Header
    if (!writeInt(&index, packetId))
        return false;

    // 3.7.1 PUBCOMP Fixed Header remaining length
    uint16_t len = index - 1;
    index = 1;
    if (!writeVariableByteInteger(&index, len))

    logger.info("Sending publish comp for packet %d", packetId);
    return writeToSocket(index, false);
}

bool MQTT5::publish(const char *topic, const char *payload, uint16_t payloadLength, MQTT5_QOS qos, uint16_t *packetId) {
    return publish(topic, payload, payloadLength, false, qos, false, packetId);
}

bool MQTT5::publish(const char *topic, const char *payload, uint16_t payloadLength, bool retain, MQTT5_QOS qos, bool dup, uint16_t *packetId, MQTT5PublishProperties properties) {
    return publish(topic, (const uint8_t*) payload, payloadLength, retain, qos, dup, packetId, properties);
}

bool MQTT5::publish(const char *topic, const uint8_t *payload, uint16_t payloadLength, bool retain, MQTT5_QOS qos, bool dup, uint16_t *packetId, MQTT5PublishProperties properties) {
    if (!connected())
        return false;

    uint16_t index = 0;
    // 3.3.1 PUBLISH Fixed Header
    if (!writeByte(&index, (uint8_t) (CTRL_PUBLISH << 4) |
                             (dup << 3) |
                             (((uint8_t) qos) << 1) |
                             retain))
        return false;

    // 3.3.2.1 Topic Name
    if (properties.topicAlias > 0 && topicAliasRegistered(properties.topicAlias)) {
        logger.info("Using topic alias %d", properties.topicAlias);
        if (!writeInt(&index, (uint16_t) 0))
            return false;
    } else {
        if (!writeUTF8String(&index, topic, strlen(topic)))
            return false;
    }
    
    if ((uint8_t) qos > 0) {
        // 3.3.2.2 Packet Identifier
        if (packetId)
            *packetId = nextPacketId;
        if (!writeInt(&index, nextPacketId++))
            return false;
    }

    // 3.3.2.3 PUBLISH Properties
    uint16_t indexProp = index;

    // 3.3.2.3.2 Payload Format Indicator
    if (properties.payloadFormatIndicator > 0) {
        if (!writeByte(&indexProp, PROP_PAYLOAD_FORMAT_INDICATOR))
            return false;

        if (!writeByte(&indexProp, properties.payloadFormatIndicator))
            return false;
    }

    // 3.3.2.3.3 Message Expiry Interval
    if (properties.payloadFormatIndicator > 0) {
        if (!writeByte(&indexProp, PROP_MESSAGE_EXPIRY_INTERVAL))
            return false;

        if (!writeInt(&indexProp, properties.messsageExpiryInterval))
            return false;
    }

    // 3.3.2.3.4 Topic Alias
    if (properties.topicAlias > 0) {
        if (!writeByte(&indexProp, PROP_TOPIC_ALIAS))
            return false;

        if (!writeInt(&indexProp, properties.topicAlias))
            return false;
    }

    // 3.3.2.3.5 Response Topic
    if (properties.responseTopic) {
        if (!writeByte(&indexProp, PROP_RESPONSE_TOPIC))
            return false;

        if (!writeUTF8String(&indexProp, properties.responseTopic, strlen(properties.responseTopic)))
            return false;
    }

    // 3.3.2.3.6 Correlation Data
    if (properties.correlationData) {
        if (!writeByte(&indexProp, PROP_CORRELATION_DATA))
            return false;

        if (!writeBytes(&indexProp, properties.correlationData, properties.correlationDataLength))
            return false;
    }

    // 3.3.2.3.1 Property Length
    if (!writeVariableByteInteger(&index, indexProp - index))
        return false;

    // 3.3.3 PUBLISH Payload
    if (!writeBytes(&index, payload, payloadLength))
        return false;

    // 3.3.1.4 Remaining Length
    uint16_t len = index - 1;
    index = 1;
    if (!writeVariableByteInteger(&index, len))
        return false;

    if ((uint8_t) qos > 0)
        logger.info("Publishing packet with id %d in topic %s with length %d", nextPacketId - 1, topic, payloadLength);
    else
        logger.info("Publishing packet in topic %s with length %d", topic, payloadLength); 
    
    // Save topic alias
    if (properties.topicAlias > 0 && topicAliasRegister(properties.topicAlias))
        logger.info("Topic %s was assigned to alias %d", topic, properties.topicAlias);
    return writeToSocket(index);
}

bool MQTT5::topicAliasRegister(uint16_t alias) {
    if (registeredTopicAliasLen >= maxTopicAlias || !registeredTopicAlias)
        return false;
    if (topicAliasRegistered(alias))
        return true;
    
    registeredTopicAlias[registeredTopicAliasLen] = alias;
    registeredTopicAliasLen++;
    return true;
}

bool MQTT5::topicAliasRegistered(uint16_t alias) {
    if (registeredTopicAliasLen <= 0 || !registeredTopicAlias)
        return false;

    for (int i = 0; i < registeredTopicAliasLen; i++) {
        if (registeredTopicAlias[i] == alias) 
            return true;
    }
    return false;
}

bool MQTT5::subscribe(const char *topic, MQTT5_QOS maxQos, bool noLocal, bool retain, MQTT5_RETAIN_HANDLING retainHandling) {
    MQTT5Subscription sub1 = { topic, maxQos, noLocal, retain, retainHandling };
    MQTT5Subscription subs[] = { sub1 };
    return subscribe(subs, 1);
}
bool MQTT5::subscribe(MQTT5Subscription subscriptions[], uint8_t subscriptionsLength) {
    if (subscriptionsLength == 0)
        return false;
    if (!connected())
        return false;
    
    uint16_t index = 0;
    // 3.8.1 SUBSCRIBE Fixed Header
    if (!writeByte(&index, (uint8_t) (CTRL_SUBSCRIBE << 4) | 0x2))
        return false;

    // 3.8.2 SUBSCRIBE Variable Header
    if (!writeInt(&index, nextPacketId++))
        return false;
    // 3.8.2.1.1 Property Length
    if (!writeVariableByteInteger(&index, 0))
            return false;

    // 3.8.3 SUBSCRIBE Payload
    for (int i = 0; i < subscriptionsLength; i++) {
        if (!subscriptions[i].topic)
            return false;
        if (!writeUTF8String(&index, subscriptions[i].topic, strlen(subscriptions[i].topic)))
            return false;
        if (!writeByte(&index, (uint8_t) subscriptions[i].maxQos | 
                              subscriptions[i].noLocal << 2 |
                              subscriptions[i].retain << 3 |
                              (uint8_t) subscriptions[i].retainHandling << 5))
            return false;
    }
    
    // 3.8.1 SUBSCRIBE Fixed Header Remaining Length
    uint16_t len = index - 1;
    index = 1;
    if (!writeVariableByteInteger(&index, len))
        return false;

    logger.info("Subscribe was issued for %d topics with packet id %d", subscriptionsLength, nextPacketId - 1);
    return writeToSocket(index);
}

bool MQTT5::unsubscribe(const char *topic) {
    const char *topics[] = { topic };
    return unsubscribe(topics, 1);
}
bool MQTT5::unsubscribe(const char *topic[], uint8_t topicLength) {
    if (topicLength == 0)
        return false;
    if (!connected())
        return false;

    uint16_t index = 0;
    // 3.10.1 UNSUBSCRIBE Fixed Header
    if (!writeByte(&index, (uint8_t) (CTRL_UNSUBSCRIBE << 4) | 0x2))
        return false;

    // 3.10.2 UNSUBSCRIBE Variable Header
    if (!writeInt(&index, nextPacketId++))
        return false;
    // 3.8.2.1.1 Property Length
    if (!writeVariableByteInteger(&index, 0))
            return false;

    // 3.10.3 UNSUBSCRIBE Payload
    for (int i = 0; i < topicLength; i++) {
        if (!topic[i])
            return false;
        if (!writeUTF8String(&index, topic[i], strlen(topic[i])))
            return false;
    }
    
    // 3.10.1 UNSUBSCRIBE Fixed Header Remaining Length
    uint16_t len = index - 1;
    index = 1;
    if (!writeVariableByteInteger(&index, len))
        return false;

    logger.info("Unsubscribe was issued for %d topics with packet id %d", topicLength, nextPacketId - 1);
    return writeToSocket(index);
}

bool MQTT5::ping() {
    uint16_t index = 0;
    // 3.12.1 PINGREQ Fixed Header
    if (!writeByte(&index, (uint8_t) (CTRL_PINGREQ << 4)))
        return false;

    if (!writeByte(&index, 0))
        return false;

    logger.info("Sending ping request");
    lastPingSent = millis();
    return writeToSocket(index, false);
}

bool MQTT5::publishRelease(uint16_t packetId, bool notFound) {
    if (!connected())
        return false;

    uint16_t index = 0;
    // 3.6.1 PUBREL Fixed Header
    if (!writeByte(&index, (CTRL_PUBREL << 4) | 0x2))
        return false;

    // 3.6.2 PUBREL Variable Header
    if (!writeInt(&index, packetId))
        return false;

    // 3.6.2.1 PUBREL Reason Code
    if (notFound) {
        if (!writeByte(&index, (uint8_t) MQTT5_REASON_CODE::PACKET_IDENTIFIER_NOT_FOUND))
            return false;

        if (!writeVariableByteInteger(&index, 0))
            return false;
    }
    
    // 3.6.1 PUBREL Fixed Header Remaining Length
    uint16_t len = index - 1;
    index = 1;
    if (!writeVariableByteInteger(&index, len))
        return false;

    logger.info("Publish release for packet %d", packetId);
    return writeToSocket(index);
}

void MQTT5::disconnect() {
    if (connected()) {
        disconnectWithReason(MQTT5_REASON_CODE::NORMAL_DISCONNECTION); 
    }
    close();
}

bool MQTT5::disconnectWithReason(MQTT5_REASON_CODE reason) {
    uint16_t index = 0;
    // 3.14.1 DISCONNECT Fixed Header
    if (!writeByte(&index, CTRL_DISCONNECT << 4))
        return false;

    // 3.14.2.1 Disconnect Reason Code
    if (!writeByte(&index, (uint8_t) reason))
        return false;
        
    // 3.14.2.2 DISCONNECT Properties
    if (!writeVariableByteInteger(&index, 0))
        return false;

    // 3.14.1 DISCONNECT Fixed Header Remaining Length
    uint16_t len = index - 1;
    index = 1;
    if (!writeVariableByteInteger(&index, len))
        return false;

    logger.info("Sending disconnect with reason %d", (uint8_t) reason);
    return writeToSocket(index);
}

bool MQTT5::readVariableByteInteger(uint16_t *position, uint16_t *value) {
    *value = 0;
    uint32_t multiplier = 1;
    do {
        if (multiplier != 1)
            (*position)++;
        *value += (buffer[*position] & 127) * multiplier;    
        if (multiplier > 128*128*128) {
            (*position)++;
            logger.warn("Malformed Variable Byte Integer");
            return false;
        }
        multiplier *= 128;
    } while ((buffer[*position] & 128) != 0);
    (*position)++;
    return true;
}

uint16_t MQTT5::readUTF8StringLength(uint16_t *position) {
    return readInt(position);
}

void MQTT5::readUTF8String(uint16_t *position, char *arr, uint16_t len) {
    memcpy(arr, buffer + (*position), len);
    arr[len] = 0;
    *position += len;
}

uint16_t MQTT5::readInt(uint16_t *position) {
    uint16_t result = buffer[*position] << 8 | buffer[*position + 1];
    (*position) += 2;
    return result;
}

uint32_t MQTT5::readLong(uint16_t *position) {
    uint32_t result = buffer[*position] << 24 | buffer[*position + 1] << 16 | buffer[*position + 2] << 8 | buffer[*position + 3];
    (*position) += 4;
    return result;
}

bool MQTT5::writeToSocket(uint16_t length, bool increasePacketsInFlight) {
#ifdef MQTT5_DEBUG
    logger.trace("Write %d bytes:", length);
    logger.dump(buffer, length);
    logger.print("\n");
#endif
    uint16_t bytesWritten = socket->write(buffer, length);
    socket->flush();
    lastOutbound = millis();
    if (increasePacketsInFlight)
        packetsInFlight++;
    return (bytesWritten == length);
}

bool MQTT5::writeVariableByteInteger(uint16_t *position, uint16_t length) {
    uint8_t lenBuf[4] = {0};
    uint8_t llen = 0;
    uint8_t digit;
    uint8_t pos = 0;
    uint16_t len = length;
    // Calculate variable byte integer (see 1.5.5)
    do {
        digit = len % 128;
        len = len / 128;
        if (len > 0) {
            digit |= 0x80;
        }
        lenBuf[pos++] = digit;
        llen++;
    } while(len > 0);
    // Add integer with zero
    if (len == 0) 
        llen = 1;

    // Check if content fits in buffer
    if ((*position) + llen + length > maxPacketSize) {
        logger.warn("Packet exceeds max packet size %d", maxPacketSize);
        return false;
    }

    // Move current contents of the buffer to make space for the integer
    memmove(buffer + (*position) + llen, buffer + (*position), length);
    memcpy(buffer + (*position), lenBuf, llen); // set remaining length
    *position += llen + length;
    return true;
}

bool MQTT5::writeUTF8String(uint16_t *position, const char* str, uint16_t length) {
    if ((*position) + length + 2 > maxPacketSize) {
        logger.warn("Packet exceeds max packet size %d", maxPacketSize);
        return false;
    }

    buffer[(*position)++] = length >> 8;
    buffer[(*position)++] = length & 0xFF;
    memcpy(buffer + (*position), str, length);
    *position += length;
    return true;
}

bool MQTT5::writeByte(uint16_t *position, uint8_t content) {
    if ((*position) + 1 > maxPacketSize) {
        logger.warn("Packet exceeds max packet size %d", maxPacketSize);
        return false;
    }
    buffer[(*position)++] = content;
    return true;
}

bool MQTT5::writeBytes(uint16_t *position, const uint8_t *payload, uint16_t payloadLength) {
    if ((*position) + payloadLength > maxPacketSize) {
        logger.warn("Packet exceeds max packet size %d", maxPacketSize);
        return false;
    }
    memcpy(buffer + (*position), payload, payloadLength);
    *position += payloadLength;
    return true;
}

bool MQTT5::writeInt(uint16_t *position, uint16_t content) {
    if ((*position) + 2 > maxPacketSize) {
        logger.warn("Packet exceeds max packet size %d", maxPacketSize);
        return false;
    }
    buffer[(*position)++] = content >> 8;
    buffer[(*position)++] = content & 0xFF;
    return position;
}

bool MQTT5::writeInt(uint16_t *position, uint32_t content) {
    if ((*position) + 4 > maxPacketSize) {
        logger.warn("Packet exceeds max packet size %d", maxPacketSize);
        return false;
    }
    buffer[(*position)++] = content >> 24;
    buffer[(*position)++] = content >> 16;
    buffer[(*position)++] = content >> 8;
    buffer[(*position)++] = content & 0xFF;
    return true;
}

void MQTT5::close() {
    free(registeredTopicAlias);
    registeredTopicAlias = NULL;
    registeredTopicAliasLen = 0;
    isConnecting = false;
    packetsInFlight = 0;
    socket->stop();
}

void MQTT5::onConnectSuccess(void (*callback)(bool)) {
    callbackConnectSuccess = callback;
}

void MQTT5::onConnectFailed(void (*callback)(MQTT5_REASON_CODE)) {
    callbackConnectFailed = callback;
}

void MQTT5::onDelivery(void (*callback)(unsigned int, bool)) {
    callbackQOS = callback;
}

void MQTT5::onPublishFailed(void (*callback)(MQTT5_REASON_CODE)) {
    callbackPublishFailed = callback;
}

void MQTT5::onSubscribeFailed(void (*callback)(MQTT5_REASON_CODE)) {
    callbackSubscribeFailed = callback;
}

void MQTT5::onUnsubscribeFailed(void (*callback)(MQTT5_REASON_CODE)) {
    callbackUnsubscribeFailed = callback;
}

void MQTT5::onPacketReceived(void (*callback)(char*, uint8_t*, uint16_t, bool, MQTT5_QOS, bool)) {
    callbackPacketReceived = callback;
}

MQTT5::~MQTT5() {
    if (connected()) {
        disconnect();
        free(buffer);
    }
}