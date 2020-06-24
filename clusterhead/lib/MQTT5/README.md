# MQTT5 for Particle Device OS

A particle library for MQTT5. MQTT5 supports publish/subscribe and request/response pattern. The packet exchange happens asynchronous.

# Features
The following features are coverd by this library:

* Publish/Subscribe messages
* Quality of Service
* Retained Messages
* Keep Alive
* Username/Password authentication
* Last Will and Testament
* Session and Message Expiry Intervals
* Topic alias
* Better error handling (negative ACKs)
* Additional properties for Connect and Publish (Maximum Packet Size & Server Keep Alive, ...)

Not yet implemented:

* Challenge-Response authentication
* Request/Response messages
* User Properties

# Requirements

This library builds heavily on `TCPClient` library from Particle Device OS. If you use Particle Device OS you are good to go.

# Example

See the examples folder.

# FAQ

## Change packet size or get "Packet exceeds max packet size"?
Change the maximum packet size in the construct.
```C++
MQTT5 client; // Default max packet size
MQTT5 client(512); // 512 bytes max packet size
```

## Message gets truncated
If you send binary data make sure you use the method where you can specify the length.

## I get disconnected when publishing
Make sure your topic string contains only valid characters. Some servers disconnect immediately if they receive malformed packets.

## I get disconnected after some time
Make sure you periodically call `loop()`.

## Something weird is happening
Make sure you are using the latest Device OS version. Make sure your flased Device OS version matches your building version.

# Debugging
To enable logging you have to declare a `SerialLogHandler` and start the serial port:

```C++
SerialLogHandler logHandler(LOG_LEVEL_NONE, {
    { "app.MQTT", LOG_LEVEL_ALL }
});

void setup() {
  Serial.begin();
  ...
}
```
To further increase the amount of logs you can add in `MQTT5.h`:

```C++
#define MQTT5_DEBUG
```

# Contribute
Feel free to open issues and pull requests.

# LICENSE
Copyright 2020 Thomas Peroutka <thomas.peroutka@gmail.com>

Licensed under the MIT license
