/*************************************************** 
  This is an example for the SHT31-D Humidity & Temp Sensor

  Designed specifically to work with the SHT31-D sensor from Adafruit
  ----> https://www.adafruit.com/products/2857

  These sensors use I2C to communicate, 2 pins are required to  
  interface
 ****************************************************/
 
#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "Adafruit_SHT31.h"

#define chip_name "FeatherHuzzah01"

#define MQTTTempSensor "house/sensor/01/temp"
#define MQTTHumiditySensor "house/sensor/01/humid"

//****************************** Connection Settings
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
char message_buff[100];

bool enableHeater = false;
uint8_t loopCnt = 0;

Adafruit_SHT31 sht31 = Adafruit_SHT31();


void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}


void setup() {
  Serial.begin(9600);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  while (!Serial)
    delay(10);     // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("SHT31 test");
  if (! sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  Serial.print("Heater Enabled State: ");
  if (sht31.isHeaterEnabled())
    Serial.println("ENABLED");
  else
    Serial.println("DISABLED");
}

float toDegrees(float other) {
  return other * 1.8f + 32.0f;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  if (! isnan(t)) {  // check if 'is not a number'
    char temp[6];
    dtostrf(toDegrees(t), 3, 1, temp);
    client.publish(MQTTTempSensor, temp);
    Serial.print("Temp *C = "); Serial.print(t); Serial.print("\t\t");
    Serial.print("Temp *F = "); Serial.print(toDegrees(t)); Serial.print("\t\t");
  } else { 
    Serial.println("Failed to read temperature");
  }
  
  if (! isnan(h)) {  // check if 'is not a number'
    char temp[6];
    dtostrf(t, 3, 1, temp);
    Serial.print("Hum. % = "); Serial.println(h);
    client.publish(MQTTHumiditySensor, temp);
  } else { 
    Serial.println("Failed to read humidity");
  }

  delay(30000);

  // Toggle heater enabled state every 30 seconds
  // An ~3.0 degC temperature increase can be noted when heater is enabled
//  if (loopCnt >= 30) {
//    enableHeater = !enableHeater;
//    sht31.heater(enableHeater);
//    Serial.print("Heater Enabled State: ");
//    if (sht31.isHeaterEnabled())
//      Serial.println("ENABLED");
//    else
//      Serial.println("DISABLED");

//    loopCnt = 0;
//  }
//  loopCnt++;
  client.loop();
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(chip_name, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
     // client.publish(MQTTfan, "OFF");
     // client.publish(MQTTled1Color, "255,255,255");
     // client.publish(MQTTled1, "OFF");
     // client.publish(MQTTled2Color, "255,255,255");
     // client.publish(MQTTled2, "OFF");
     // client.publish(MQTTled3Color, "255,255,255");
     // client.publish(MQTTled3, "OFF");
     // client.publish(MQTTled4Color, "255,255,255");
     // client.publish(MQTTled4, "OFF");
     // client.publish(MQTTled5Color, "255,255,255");
     // client.publish(MQTTled5, "OFF");
// ... and resubscribe
      
//      client.subscribe(MQTTled5Color);
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  int i = 0;
  char message_buff[100];
  String StrPayload;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (i = 0; i < length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  StrPayload = String(message_buff);
  int IntPayload = StrPayload.toInt();
  Serial.print(StrPayload);

  Serial.println();
}
