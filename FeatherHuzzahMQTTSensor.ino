#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>

#define MQTT_MAX_PACKET_SIZE 512
#include <PubSubClient.h>

#include "config.h"
#include "Adafruit_SHT31.h"
#include <EEPROM.h>
#include <ArduinoJson.h>

// how long to sleep between checking the door state (in seconds)
#define SLEEP_LENGTH 60

#define REPORT_INTERVAL_S 1
//****************************** Connection Settings
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
char message_buff[100];

bool enableHeater = false;
uint8_t loopCnt = 0;

String chip_name = "feather_huzzah_01";
String rootTopic = "home/nodes/sensor/" + chip_name;
String stateTopic = "home/nodes/sensor/" + chip_name + "/state";

Adafruit_SHT31 sht31 = Adafruit_SHT31();


void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  WiFi.setAutoConnect(true);

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

  // start the serial connection
  Serial.begin(115200);
  Serial.setTimeout(2000);

  EEPROM.begin(512);

  // get the current count position from eeprom
  byte battery_count = EEPROM.read(0);

  Serial.println("battery_count = "); Serial.println(battery_count);
  Serial.println("check_val = "); Serial.println(REPORT_INTERVAL_S);

  reportData();
  // we only need this to happen once every X minutes,
  // so we use eeprom to track the count between resets.
  if(battery_count >= REPORT_INTERVAL_S) {
    // reset counter
    battery_count = 0;

    // report battery level to Adafruit IO
    // reportData();
  }
  else {
    // increment counter
    battery_count++;
  }

  // save the current count
  EEPROM.write(0, battery_count);
  EEPROM.commit();

  // we are done here. go back to sleep.
  ESP.deepSleep(SLEEP_LENGTH * 1000000);
  // ESP.deepSleep(SLEEP_LENGTH * 100000);
}

float toDegrees(float other) {
  return other * 1.8f + 32.0f;
}

void sendStateData() {
    StaticJsonDocument<512> doc;

    float t = sht31.readTemperature();
    float h = sht31.readHumidity();

    int bat = battery_level();
    if (bat > 0)
    {
        doc["battery"] = bat;
    }

    if (! isnan(t)) {  // check if 'is not a number'
        doc["temperature"] = (int)toDegrees(t);
    } else {
        Serial.println("Failed to read temperature");
    }

    if (! isnan(h)) {  // check if 'is not a number'
        doc["humidity"] = (int)h;
    } else {
        Serial.println("Failed to read humidity");
    }

    StaticJsonDocument<256> attributes;
    attributes["friendly_name"] = chip_name;
    // attributes["device_id"] = chip_name;

    doc["attributes"] = attributes;
    doc["object_id"] = chip_name;

    char buffer[512];
    auto topic = stateTopic.c_str();
    size_t bufferSize = serializeJson(doc, buffer);

    client.publish(topic, buffer, bufferSize);
    Serial.print("Res = [size="); Serial.print(bufferSize); Serial.print("] "); Serial.println(topic); Serial.println(buffer);

    client.publish((rootTopic + "/status").c_str(), "online");
}

void reportData() {
  if (! sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  if (!client.connected()) {
    reconnect();
  }

  
  delay(100);
   sendMQTTHumidityDiscoveryMsg();
  delay(100);
  sendMQTTBatteryDiscoveryMsg();
  delay(100);
   sendMQTTTemperatureDiscoveryMsg();
  delay(100);

  sendStateData();

  delay(1000);
  client.disconnect();
  WiFi.disconnect();
}

void loop() {

}


void reconnect() {
  int tryCount = 10;
  // Loop until we're reconnected
  while (!client.connected() && tryCount-- > 0) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(chip_name.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void sendMQTTBatteryDiscoveryMsg() {
    String discoveryTopic = "homeassistant/sensor/" + chip_name + "/battery/config";

    DynamicJsonDocument doc(1024);
    char buffer[512];

    StaticJsonDocument<256> device;
    device["identifiers"] = JsonArray();
    device["identifiers"].add(chip_name);
    device["name"] = "Feather Huzzah Wifi";
    device["manufacturer"] = "Ken";
    device["model"] = "Feather Huzzah ESP8266";

    doc["device"] = device;
    doc["name"] = chip_name + "_battery";
    doc["~"] = rootTopic;
    doc["device_class"] = "battery";
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "%";
    doc["state_topic"]   = "~/state";
    doc["avty_t"] = "~/status";
    doc["platform"] = "mqtt";
    doc["value_template"] = "{{ value_json.battery }}";
    doc["pl_avail"] = "online";
    doc["pl_not_avail"] = "offline";
    doc["uniq_id"] = "huzzah_01_bat";

    size_t n = serializeJson(doc, buffer);

    auto res = client.publish(discoveryTopic.c_str(), buffer, n);
    Serial.println("");
    Serial.print("sendMQTTBatteryDiscoveryMsg = [size="); Serial.print(n); Serial.print("] "); Serial.println(discoveryTopic); Serial.println(buffer);
    Serial.print("Publish Result="); Serial.println(res);

}

int battery_level() {
    // read the battery level from the ESP8266 analog in pin.
    // analog read level is 10 bit 0-1023 (0V-1V).
    // our 1M & 220K voltage divider takes the max
    // lipo value of 4.2V and drops it to 0.758V max.
    // this means our min analog read value should be 580 (3.14V)
    // and the max analog read value should be 774 (4.2V).
    int level = analogRead(A0);

    // convert battery level to percent
    level = map(level, 580, 774, 0, 100);
    return level;
}

void callback(char* topic, byte* payload, unsigned int length) {

    Serial.print("On Callback! = "); Serial.println(topic);

    StaticJsonDocument<256> doc;
    deserializeJson(doc, payload, length);
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

void sendMQTTTemperatureDiscoveryMsg() {
    String discoveryTopic = "homeassistant/sensor/" + chip_name + "/temperature/config";

    DynamicJsonDocument doc(1024);
    char buffer[512];


    StaticJsonDocument<256> device;
    device["identifiers"] = JsonArray();
    device["identifiers"].add(chip_name);
    device["name"] = "Feather Huzzah Wifi";
    device["manufacturer"] = "Ken";
    device["model"] = "Feather Huzzah ESP8266";

    doc["device"] = device;
    doc["name"] = chip_name + "_temp";
    doc["~"] = rootTopic;
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "°F";
    doc["device_class"] = "temperature";
    doc["state_topic"]   = "~/state";
    doc["avty_t"] = "~/status";
    doc["platform"] = "mqtt";
    doc["value_template"] = "{{ value_json.temperature }}";
    doc["pl_avail"] = "online";
    doc["pl_not_avail"] = "offline";
    doc["uniq_id"] = "huzzah_01_temp";

    size_t n = serializeJson(doc, buffer);
  
    if (!client.connected()) {
      reconnect();
    }
    Serial.println("");
    auto res = client.publish(discoveryTopic.c_str(), buffer, n);
    Serial.print("sendMQTTTemperatureDiscoveryMsg = [size="); Serial.print(n); Serial.print("] "); Serial.println(discoveryTopic); Serial.println(buffer);
    Serial.print("Publish Result="); Serial.println(res);
}

void sendMQTTHumidityDiscoveryMsg() {
    String discoveryTopic = "homeassistant/sensor/" + chip_name + "/humidity/config";

    DynamicJsonDocument doc(1024);
    char buffer[512];

    StaticJsonDocument<256> device;
    device["identifiers"] = JsonArray();
    device["identifiers"].add(chip_name);
    device["name"] = "Feather Huzzah Wifi";
    device["manufacturer"] = "Ken";
    device["model"] = "Feather Huzzah ESP8266";

    doc["device"] = device;
    doc["name"] = chip_name + "_hum";
    doc["~"] = rootTopic;
    doc["state_class"] = "measurement";
    doc["unit_of_measurement"] = "%";
    doc["device_class"] = "humidity";
    doc["state_topic"]   = "~/state";
    doc["avty_t"] = "~/status";
    doc["platform"] = "mqtt";
    doc["value_template"] = "{{ value_json.humidity }}";
    doc["pl_avail"] = "online";
    doc["pl_not_avail"] = "offline";
    doc["uniq_id"] = "huzzah_01_hum";

    size_t n = serializeJson(doc, buffer);

    if (!client.connected()) {
      reconnect();
    }
    auto res = client.publish(discoveryTopic.c_str(), buffer, n);


    Serial.println("");
    Serial.print("sendMQTTHumidityDiscoveryMsg = [size="); Serial.print(n); Serial.print("] "); Serial.println(discoveryTopic); Serial.println(buffer);
    Serial.print("Publish Result="); Serial.println(res);
}
