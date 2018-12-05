/*
 * This is a basic example on how to use Espalexa and its device declaration methods.
 */
#include <Espalexa.h>

#define LED_PIN       (2)
#define LED_OFF       (HIGH)
#define LED_ON        (LOW)

// prototypes
boolean connectWifi();

// Our callback function
void onDeviceChange(
  const uint8_t deviceID, const char* deviceName, const uint8_t value)
{
  Serial.print("[");
  Serial.print(deviceName);
  Serial.print("] #");
  Serial.print(deviceID);
  Serial.print(" changed to: ");
  if (value>0) {
    Serial.print("ON, value: ");
    Serial.println(value);
    digitalWrite(LED_PIN, LED_ON);
  } else  {
    Serial.println("OFF");
    digitalWrite(LED_PIN, LED_OFF);
  }
}

// Change this!!
const char* ssid     = "<my_SSID>";
const char* password = "<my_PASSWORD>";

boolean wifiConnected = false;

Espalexa espalexa;

void setup(void)
{
  Serial.begin(115200);
  while ( !Serial ) {}

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);
  
  // Initialise wifi connection
  wifiConnected = connectWifi();
  
  if ( wifiConnected ) {  
    // Define your devices here. 
    //espalexa.addDevice("light 1", onDeviceChange); // simplest definition, default state off
    espalexa.addDevice("light 2", onDeviceChange, 0); // third parameter is beginning state (here fully off)
    
    espalexa.begin();
  } else {
    while (1) {
      Serial.println("Cannot connect to WiFi. Please check data and reset the ESP.");
      delay(2500);
    }
  }
}
 
void loop(void)
{
   espalexa.loop();
   delay(1);
}

// connect to wifi – returns true if successful or false if not
boolean connectWifi(void)
{
  boolean state = true;
  int i = 0;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 20){
      state = false; break;
    }
    i++;
  }
  
  Serial.println("");
  if (state) {
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Connection failed.");
  }
  return state;
}
