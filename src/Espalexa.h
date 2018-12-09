#ifndef Espalexa_h
#define Espalexa_h
#include <stdint.h>
#include "Arduino.h"

#ifdef ARDUINO_ARCH_ESP32
#include "dependencies/webserver/WebServer.h"
#include <WiFi.h>
#else
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
typedef ESP8266WebServer WebServer;
#endif

// this limit only has memory reasons, set it higher should you need to
#define ALEXA_MAX_DEVICES    (16) 
#define ALEXA_DEBOUNCE      (250)

typedef void (*CallbackBriFunction)(const uint8_t deviceID,
  const char* deviceName, const uint8_t value);

class EspalexaDevice {
private:
  String _deviceName;
  CallbackBriFunction _callback;
  const uint32_t _debounce;
  unsigned long _debounce_ts;
  uint8_t _deviceID;
  uint8_t _val_curr;
  uint8_t _val_last;

public:
  EspalexaDevice(void);
  ~EspalexaDevice(void);
  EspalexaDevice(
    const char* deviceName,
    CallbackBriFunction callback,
    const uint8_t value=0,
    const uint32_t debounce=ALEXA_DEBOUNCE);
    
  String   getName(void);
  void     setName(const String name);

  uint8_t  getValue(void);  
  void     setValue(const uint8_t value);
  
  uint8_t  getID(void);
  void     setID(const uint8_t id);

  void     triggerCallback(void);
  void     executeCallback(void);
  
  uint8_t  getLastValue(void); // last value that was not off (1-255)
};

class Espalexa {
private:
  void startHttpServer(void);
  String getMac(void);
  String deviceJsonString(const uint8_t deviceId);

  bool addDevice(EspalexaDevice* device);

  bool handleAlexaApiCall(String req, String body);
  void respondToSearch(void);
  String boolString(const bool st);
  bool connectUDP(void);

  void alexaOn(const uint8_t deviceId);
  void alexaOff(const uint8_t deviceId);
  void alexaValue(const uint8_t deviceId, const uint8_t value);

  const IPAddress IP;
  const uint32_t  port;
  bool udpConnected;
  uint32_t deviceCount;
  WebServer* server;
  EspalexaDevice* devices[ALEXA_MAX_DEVICES];

public:
  Espalexa(void);
  ~Espalexa(void);
  bool begin(WebServer* externalServer=NULL);

  bool addDevice(const char* deviceName, CallbackBriFunction callback, const uint8_t value=0);

  void loop(void);

  void servePage(void);
  void serveNotFound(void);
  void serveDescription(void);
};

#endif

