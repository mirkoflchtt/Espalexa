/*
 * Alexa Voice On/Off/Brightness Control. Emulates a Philips Hue bridge to Alexa.
 * 
 * This was put together from these two excellent projects:
 * https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch
 * https://github.com/probonopd/ESP8266HueEmulator
 */
/*
 * @title Espalexa library
 * @version 2.1.2
 * @author Christian Schwinne
 * @license MIT
 */
//#define DEBUG_ESPALEXA
 
#ifdef DEBUG_ESPALEXA
 #define DEBUG(x)   Serial.print(x)
 #define DEBUGLN(x) Serial.println(x)
#else
 #define DEBUG(x)
 #define DEBUGLN(x)
#endif
 
#ifdef ARDUINO_ARCH_ESP32
  #include "dependencies/webserver/WebServer.h" // https://github.com/bbx10/WebServer_tng
  #include <WiFi.h>
#else
  #include <ESP8266WebServer.h>
  #include <ESP8266WiFi.h>
#endif
#include <WiFiUdp.h>
#include "Espalexa.h"

// this limit only has memory reasons, set it higher should you need to
#define MAXDEVICES    (16) 

uint8_t currentDeviceCount = 0;

EspalexaDevice* devices[MAXDEVICES] = {};

//Keep in mind that Device IDs go from 1 to DEVICES, cpp arrays from 0 to DEVICES-1!!

WiFiUDP UDP;
static const IPAddress ipMulti(239, 255, 255, 250);
static const unsigned int portMulti = 1900;      // local port to listen on
bool udpConnected = false;
char packetBuffer[256]; // buffer to hold incoming packet,
Espalexa* instance = NULL;

Espalexa::Espalexa(void)
{
  instance = this;
}

Espalexa::~Espalexa(void)
{
}

#ifdef ARDUINO_ARCH_ESP32
bool Espalexa::begin(WebServer* externalServer)
#else
bool Espalexa::begin(ESP8266WebServer* externalServer)
#endif
{
  DEBUGLN("Espalexa Begin...");
  
  server = externalServer;
  udpConnected = connectUDP();
    
  if (udpConnected) {
    startHttpServer();
    DEBUGLN("Done");
    return true;
  }
  DEBUGLN("Failed");
  return false;
}

void Espalexa::loop(void)
{
  if (server) {
    server->handleClient();
  }

  if (udpConnected) {    
    const int packetSize = UDP.parsePacket();
      
    if (packetSize>0) {
      DEBUGLN("Got UDP!");
      const int len = UDP.read(packetBuffer, sizeof(packetBuffer)-1);
      if (len > 0) {
        packetBuffer[len] = 0;
      }

      String request = packetBuffer;
      DEBUGLN(request);
      if (request.indexOf("M-SEARCH") >= 0) {
        if ( (request.indexOf("upnp:rootdevice")>0) || 
             (request.indexOf("device:basic:1")>0) ) {
          respondToSearch();
        }
      }
    }
  }
}

String Espalexa::getMac(void)
{
  String res = WiFi.macAddress();
  res.replace(":", "");
  res.toLowerCase();
  return res;
}

bool Espalexa::addDevice(EspalexaDevice* device)
{
  if ( !device ) return false;

  DEBUG("Adding device ");
  DEBUGLN((currentDeviceCount+1));
  if (currentDeviceCount >= MAXDEVICES) return false;
  device->setID(currentDeviceCount+1);
  devices[currentDeviceCount] = device;
  currentDeviceCount++;
  return true;
}

bool Espalexa::addDevice(const char* deviceName, CallbackBriFunction callback, const uint8_t value)
{
  if ( !deviceName ) return false;
  DEBUG("Constructing device ");
  DEBUGLN((currentDeviceCount+1));
  if (currentDeviceCount >= MAXDEVICES) return false;
  EspalexaDevice* d = new EspalexaDevice(deviceName, callback, value);
  return addDevice(d);
}

String Espalexa::deviceJsonString(const uint8_t deviceId)
{
  if (deviceId < 1 || deviceId > currentDeviceCount) return "{}"; // error

  String res = "{";
  res += "\"type\":\"Extended color light\",";
  res += "\"manufacturername\":\"OpenSource\",\"swversion\":\"0.1\",";
  res += "\"name\":\"" + devices[deviceId-1]->getName() + "\",";
  res += "\"uniqueid\":\"" + WiFi.macAddress();
  res += "-" + String(devices[deviceId-1]->getID()) + "\",";
  res += "\"modelid\":\"LST001\",\"state\":{\"on\":";
  res += boolString(devices[deviceId-1]->getValue());
  res += ",\"bri\":";
  res += String(devices[deviceId-1]->getLastValue()-1);
  res += ",\"xy\":[0.00000,0.00000],\"colormode\":\"hs\",\"effect\":\"none\",";
  res += "\"ct\":500,\"hue\":0,\"sat\":0,\"alert\":\"none\",\"reachable\":true}";
  res += "}";
  return res;
}

bool Espalexa::handleAlexaApiCall(String req, String body) // basic implementation of Philips hue api functions needed for basic Alexa control
{
  if ( !server ) return false;

  DEBUGLN("AlexaApiCall");
  if ( req.indexOf("api")<0 ) return false; //return if not an API call
  DEBUGLN("ok");
  
  if ( body.indexOf("devicetype")>0 ) //client wants a hue api username, we dont care and give static
  {
    DEBUGLN("devType");
    server->send(200, "application/json", "[{\"success\":{\"username\": \"2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr\"}}]");
    return true;
  }

  if ( req.indexOf("state")>0 ) //client wants to control light
  {
    const int tempDeviceId = req.substring(req.indexOf("lights")+7).toInt();
    DEBUG("ls"); DEBUGLN(tempDeviceId);
    if ( body.indexOf("bri")>0 ) {
      alexaValue(tempDeviceId, body.substring(body.indexOf("bri")+5).toInt());
      return true;
    }
    if ( body.indexOf("false")>0 ) {
      alexaOff(tempDeviceId);
      return true;
    }
    alexaOn(tempDeviceId);
    
    return true;
  }
  
  int pos = req.indexOf("lights");
  if (pos > 0) //client wants light info
  {
    const int tempDeviceId = req.substring(pos+7).toInt();
    DEBUG("l"); DEBUGLN(tempDeviceId);

    if (tempDeviceId == 0) //client wants all lights
    {
      DEBUGLN("lAll");
      String jsonTemp = "{";
      for (int i = 0; i<currentDeviceCount; i++)
      {
        const uint8_t id = devices[i]->getID(); // (i+1);
        jsonTemp += "\"" + String(id) + "\":";
        jsonTemp += deviceJsonString(id);
        jsonTemp += (i<(currentDeviceCount-1))  ? "," : "";
      }
      jsonTemp += "}";
      server->send(200, "application/json", jsonTemp);
    } else //client wants one light (tempDeviceId)
    {
      server->send(200, "application/json", deviceJsonString(tempDeviceId));
    }
    
    return true;
  }

  //we dont care about other api commands at this time and send empty JSON
  server->send(200, "application/json", "{}");
  return true;
}

void servePage(void)
{
  if ( !(instance && instance->server) ) return;

  DEBUGLN("HTTP Req espalexa ...\n");
  String res = "Hello from Espalexa!\r\n\r\n";
  for (int i=0; i<currentDeviceCount; i++)
  {
    const uint8_t id = devices[i]->getID(); // (i+1);
    res += "Value of device " + String(id) + " (" + devices[i]->getName() + "): " + String(devices[i]->getValue()) + "\r\n";
  }
  res += "\r\nFree Heap: " + String(ESP.getFreeHeap());
  res += "\r\nUptime: " + String(millis());
  res += "\r\n\r\nEspalexa library V2.1.2 by Christian Schwinne 2018";
  instance->server->send(200, "text/plain", res);
}

void serveNotFound(void)
{
  if ( !(instance && instance->server) ) return;

  DEBUGLN("Not-Found HTTP call:");
  DEBUGLN("URI: " + instance->server->uri());
  DEBUGLN("Body: " + instance->server->arg(0));
  if(!instance->handleAlexaApiCall(instance->server->uri(),instance->server->arg(0))) {
    instance->server->send(404, "text/plain", "Not Found (espalexa-internal)");
  }
}

void serveDescription(void)
{
  DEBUGLN("# Responding to description.xml ... #\n");
  IPAddress localIP = WiFi.localIP();
  char s[16];
  if ( !(instance && instance->server) ) return;

  sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    
  const String escapedMac = instance->getMac();

  const String setup_xml = "<?xml version=\"1.0\" ?>"
          "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
          "<specVersion><major>1</major><minor>0</minor></specVersion>"
          "<URLBase>http://"+ String(s) +":80/</URLBase>"
          "<device>"
            "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>"
            "<friendlyName>Philips hue ("+ String(s) +")</friendlyName>"
            "<manufacturer>Royal Philips Electronics</manufacturer>"
            "<manufacturerURL>http://www.philips.com</manufacturerURL>"
            "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>"
            "<modelName>Philips hue bridge 2012</modelName>"
            "<modelNumber>929000226503</modelNumber>"
            "<modelURL>http://www.meethue.com</modelURL>"
            "<serialNumber>"+ escapedMac +"</serialNumber>"
            "<UDN>uuid:2f402f80-da50-11e1-9b23-"+ escapedMac +"</UDN>"
            "<presentationURL>index.html</presentationURL>"
            "<iconList>"
            "  <icon>"
            "    <mimetype>image/png</mimetype>"
            "    <height>48</height>"
            "    <width>48</width>"
            "    <depth>24</depth>"
            "    <url>hue_logo_0.png</url>"
            "  </icon>"
            "  <icon>"
            "    <mimetype>image/png</mimetype>"
            "    <height>120</height>"
            "    <width>120</width>"
            "    <depth>24</depth>"
            "    <url>hue_logo_3.png</url>"
            "  </icon>"
            "</iconList>"
          "</device>"
          "</root>";
            
  instance->server->send(200, "text/xml", setup_xml.c_str());
        
  DEBUG("Sending :");
  DEBUGLN(setup_xml);
}

void Espalexa::startHttpServer(void)
{
  if (server == nullptr) {
#ifdef ARDUINO_ARCH_ESP32
    server = new WebServer(80);
#else
    server = new ESP8266WebServer(80);  
#endif
    server->onNotFound(serveNotFound);
  }
  
  if ( !server ) return;

  server->on("/espalexa", HTTP_GET, servePage);
  server->on("/description.xml", HTTP_GET, serveDescription);
  server->begin();
}

void Espalexa::alexaOn(const uint8_t deviceId)
{
  if ( !server ) return;

  String body = "[{\"success\":{\"/lights/"+ String(deviceId) +"/state/on\":true}}]";

  server->send(200, "text/xml", body.c_str());

  devices[deviceId-1]->setValue(devices[deviceId-1]->getLastValue());
  devices[deviceId-1]->doCallback();
}

void Espalexa::alexaOff(const uint8_t deviceId)
{
  if ( !server ) return;

  String body = "[{\"success\":{\"/lights/"+ String(deviceId) +"/state/on\":false}}]";

  server->send(200, "application/json", body.c_str());
  devices[deviceId-1]->setValue(0);
  devices[deviceId-1]->doCallback();
}

void Espalexa::alexaValue(const uint8_t deviceId, const uint8_t value)
{
  if ( !server ) return;

  String body = "[{\"success\":{\"/lights/"+ String(deviceId) +"/state/bri\":"+ String(value) +"}}]";

  server->send(200, "application/json", body.c_str());
  
  if (value == 255)
  {
   devices[deviceId-1]->setValue(255);
  } else {
   devices[deviceId-1]->setValue(value+1); 
  }
  devices[deviceId-1]->doCallback();
}

void Espalexa::respondToSearch(void)
{
  if ( !(instance && instance->server) ) return;

  const IPAddress localIP = WiFi.localIP();
  char s[16];
  snprintf(s, sizeof(s), "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

  const String escapedMac = instance->getMac();
  const String response = 
    "HTTP/1.1 200 OK\r\n"
    "EXT:\r\n"
    "CACHE-CONTROL: max-age=100\r\n" // SSDP_INTERVAL
    "LOCATION: http://"+ String(s) +":80/description.xml\r\n"
    "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n" // _modelName, _modelNumber
    "hue-bridgeid: "+ escapedMac +"\r\n"
    "ST: urn:schemas-upnp-org:device:basic:1\r\n"  // _deviceType
    "USN: uuid:2f402f80-da50-11e1-9b23-"+ escapedMac +"::upnp:rootdevice\r\n" // _uuid::_deviceType
    "\r\n";

  DEBUGLN("Responding search req...");

  UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
#ifdef ARDUINO_ARCH_ESP32
  UDP.write((uint8_t*)response.c_str(), response.length());
#else
  UDP.write(response.c_str());
#endif
  UDP.endPacket();                    
}

String Espalexa::boolString(const bool st)
{
  return (st) ? "true" : "false";
}

bool Espalexa::connectUDP(void)
{
#ifdef ARDUINO_ARCH_ESP32
  return UDP.beginMulticast(ipMulti, portMulti);
#else
  return UDP.beginMulticast(WiFi.localIP(), ipMulti, portMulti);
#endif
}

//EspalexaDevice Class

EspalexaDevice::EspalexaDevice(void) :
_deviceName(),
_callback(NULL),
_deviceID(0),
_val(0),
_val_last(0)
{
}

EspalexaDevice::EspalexaDevice(
  const char* deviceName, CallbackBriFunction callback, const uint8_t value) :
_deviceName(deviceName),
_callback(callback),
_deviceID(0),
_val(value),
_val_last(value)
{ 
}

EspalexaDevice::~EspalexaDevice(void)
{
}

String EspalexaDevice::getName(void)
{
  return _deviceName;
}

void EspalexaDevice::setName(const String name)
{
  _deviceName = name;
}

uint8_t EspalexaDevice::getValue(void)
{
  return _val;
}

uint8_t EspalexaDevice::getLastValue(void)
{
  return (_val_last == 0) ? 255 : _val_last;
}

void EspalexaDevice::setValue(const uint8_t val)
{
  if (_val != 0) {
    _val_last = _val;
  }
  if (val != 0) {
    _val_last = val;
  }
  _val = val;
}

uint8_t EspalexaDevice::getID(void)
{
  return _deviceID;
}

void EspalexaDevice::setID(const uint8_t id)
{
  _deviceID = id;
}

void EspalexaDevice::doCallback(void)
{
  _callback(_deviceID, _deviceName.c_str(), _val);
}