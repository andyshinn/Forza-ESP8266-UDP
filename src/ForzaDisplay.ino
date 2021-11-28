#include "SuppressWarning.h"
#include "ssid_creds.h"

#ifdef ESP32
#include <ESPmDNS.h>
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#endif

#include "ForzaData.h"
#include <WiFiUdp.h>

#include <ArduinoOTA.h>
#include <FastLED.h>

#include <PubSubClient.h>

#define DATA_PIN 2
#define NUM_LEDS 12
#define MSG_BUFFER_SIZE (50)
#define LED_BRIGHTNESS 50
#define LED_BRIGHTNESS_MENU 10

// CRGB ledColourMap[NUM_LEDS] = {
//     CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green,
//     CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green,
//     CRGB::Green,  CRGB::Green, CRGB::Green, CRGB::Yellow, CRGB::Yellow, CRGB::Yellow, CRGB::Yellow, CRGB::Orange,
//     CRGB::Orange, CRGB::Orange, CRGB::Orange, CRGB::Blue,    CRGB::Blue,    CRGB::Blue,    CRGB::Blue,    CRGB::Blue,
// };

CRGB ledColourMap[NUM_LEDS] = {
    CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green,  CRGB::Green, CRGB::Yellow,
    CRGB::Yellow, CRGB::Yellow, CRGB::Orange, CRGB::Orange, CRGB::Blue,  CRGB::Blue,
};

CRGB leds[NUM_LEDS];

const uint32_t UDP_RX_PACKET_SIZE = 324U;
const char *mqtt_server = "192.168.1.80";
unsigned int localPort = 2808; // local port to listen on
WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMsg = 0;

char msg[MSG_BUFFER_SIZE];
char debugMsg[256];
int value = 0;

// buffers for receiving and sending data
char packetBuffer[UDP_RX_PACKET_SIZE]; // buffer to hold incoming packet,
ForzaData_t *forzaData = (ForzaData_t *)packetBuffer;
WiFiUDP Udp;

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);
  } else {
    digitalWrite(BUILTIN_LED, HIGH);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("outTopic", "hello world");
      // ... and resubscribe
      mqttClient.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void sendMessage(const char *topic, const char *message) {
  if (mqttClient.connected()) {
    mqttClient.publish(topic, message);
  }
}

void setupMqtt() {
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  reconnect();
}

void setupOta() {
  ArduinoOTA.setHostname("ForzaDisplay");

  ArduinoOTA.onStart([]() { Serial.println("Starting OTA upgrade"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd of OTA upgrade"); });
  ArduinoOTA.onProgress(
      [](unsigned int progress, unsigned int total) { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void setupUDP() { Udp.begin(localPort); }

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  Serial.begin(115200);
  Serial.print("Connecting to SSID: " + String(STASSID));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupLEDS() {
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
}

void setup() {
  setupWifi();
  // setupMqtt();
  setupOta();
  setupUDP();
  setupLEDS();
}

void loop() {
  ArduinoOTA.handle();
  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    // read the packet into packetBufffer
    Udp.read(packetBuffer, UDP_RX_PACKET_SIZE);

    // float mph = forzaData->Speed * 2.237;

    if (forzaData->IsRaceOn) {
      uint8_t numLEDs =
          (uint8_t)map(forzaData->CurrentEngineRpm, forzaData->EngineIdleRpm, forzaData->EngineMaxRpm, 0, NUM_LEDS - 1);

      if (numLEDs <= NUM_LEDS) {
        for (uint8_t led = 0; led < NUM_LEDS; led++) {
          if ((led) <= numLEDs) {
            leds[led] = ledColourMap[led];
          } else {
            leds[led] = CRGB::Black;
          }
        }
        FastLED.setBrightness(LED_BRIGHTNESS);
        FastLED.show();
      }
    } else { // in menu or not in map
      for (uint8_t led = 0; led < NUM_LEDS; led++) {
        leds[led] = CRGB::Blue;
      }
      FastLED.setBrightness(LED_BRIGHTNESS_MENU);
      FastLED.show();
    }
  }

  String sizeString = String(packetSize);
  const char *sizeChar = sizeString.c_str();
  sendMessage("outTopic", sizeChar);

  // mqttClient.loop();

  // delay(1000);
}
