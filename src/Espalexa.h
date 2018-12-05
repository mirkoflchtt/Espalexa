#ifndef Espalexa_h
#define Espalexa_h

#include "Arduino.h"
//#include <WiFiUDP.h>

#ifdef ARDUINO_ARCH_ESP32
#include "dependencies/webserver/WebServer.h"
#else
#include <ESP8266WebServer.h>
#endif

typedef void (*CallbackBriFunction) (
  const uint8_t deviceID, const char* deviceName, const uint8_t value);

class EspalexaDevice {
private:
  String _deviceName;
  CallbackBriFunction _callback;
  uint8_t _deviceID;
  uint8_t _val, _val_last;

public:
  EspalexaDevice(void);
  ~EspalexaDevice(void);
  EspalexaDevice(
    const char* deviceName,
    CallbackBriFunction callback,
    const uint8_t value=0);
    
  String   getName(void);
  void     setName(const String name);

  uint8_t  getValue(void);  
  void     setValue(const uint8_t value);
  
  uint8_t  getID(void);
  void     setID(const uint8_t id);

  void     doCallback(void);
  
  uint8_t  getLastValue(void); //last value that was not off (1-255)
};

class Espalexa {
private:
  void startHttpServer(void);
  String deviceJsonString(const uint8_t deviceId);
  void handleDescriptionXml(void);
  void respondToSearch(void);
  String boolString(const bool st);
  bool connectUDP(void);
  void alexaOn(const uint8_t deviceId);
  void alexaOff(const uint8_t deviceId);
  void alexaValue(const uint8_t deviceId, const uint8_t value);

public:
  Espalexa(void);
  ~Espalexa(void);
  #ifdef ARDUINO_ARCH_ESP32
  WebServer* server;
  bool begin(WebServer* externalServer=nullptr);
  #else
  ESP8266WebServer* server;
  bool begin(ESP8266WebServer* externalServer=nullptr);
  #endif

  String getMac(void);

  bool addDevice(EspalexaDevice* device);
  bool addDevice(const char* deviceName, CallbackBriFunction callback, const uint8_t value=0);

  void loop(void);
    
  bool handleAlexaApiCall(String req, String body);
};

#endif

