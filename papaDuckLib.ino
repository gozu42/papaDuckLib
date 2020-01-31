#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "timer.h"

#define SSID
#define PASSWORD

#define ORG         "9c6nfo"                  // "quickstart" or use your organisation
#define DEVICE_ID   "UPRM_CID"
#define DEVICE_TYPE "PAPA"                // your device type or not used for "quickstart"
#define TOKEN       "e6hd?lawoVma4zk1-_"      // your device token or not used for "quickstart"#define SSID        "nick_owl" // Type your SSID

char server[]           = ORG ".messaging.internetofthings.ibmcloud.com";
char topic[]            = "iot-2/evt/status/fmt/json";
char authMethod[]       = "use-token-auth";
char token[]            = TOKEN;
char clientId[]         = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

typedef struct
{
  String senderId;
  String messageId;
  String payload;
  String path;
} Packet;


ClusterDuck duck;

auto timer = timer_create_default(); // create a timer with default settings

WiFiClientSecure wifiClient;
PubSubClient client(server, 8883, wifiClient);


void setup() {
  // put your setup code here, to run once:

  duck.begin();
  duck.setupDeviceId("Papa");

  duck.setupLoRa();
  LoRa.receive();
  duck.setupDisplay("Papa");

  setupWiFi();
  
  Serial.println("PAPA Online");
}

void loop() {
  // put your main code here, to run repeatedly:
  if(WiFi.status() != WL_CONNECTED)
  {
    Serial.print("WiFi disconnected, reconnecting to local network: ");
    Serial.print(SSID);
    setupWiFi();
    disconnected = false;
  }
  setupMQTT();

  int packetSize = LoRa.parsePacket();
  if (packetSize != 0) {
    String * val = duck.getPacketData(packetSize);
    quackJson(buildPayload(val, packetSize ));
  }

  
  timer.tick();
}



void setupWiFi()
{
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(SSID);

  // Connect to Access Poink
  WiFi.begin(SSID, PASSWORD);
  u8x8.drawString(0, 1, "Connecting...");

  while (WiFi.status() != WL_CONNECTED)
  {
    timer.tick(); //Advance timer to reboot after awhile
    //delay(500);
    Serial.print(".");
  }

  // Connected to Access Point
  Serial.println("");
  Serial.println("WiFi connected - PAPA ONLINE");
  u8x8.drawString(0, 1, "PAPA Online");
}

void setupMQTT()
{
  if (!!!client.connected()) {
    Serial.print("Reconnecting client to "); Serial.println(server);
    while ( ! (ORG == "quickstart" ? client.connect(clientId) : client.connect(clientId, authMethod, token)))
    {
      timer.tick(); //Advance timer to reboot after awhile
      Serial.print(".");
      //delay(500);
    }
  }
}

void quackJson(String payload)
{
  const int bufferSize = 4 * JSON_OBJECT_SIZE(1);
  DynamicJsonBuffer jsonBuffer(bufferSize);

  JsonObject& root = jsonBuffer.createObject();

  Packet lastPacket = duck.getLastPacket();

  root["DeviceID"]        = lastPacket.senderId;
  root["MessageID"]       = lastPacket.messageId;
  root["Payload"]         = payload;

  root["path"]            = lastPacket.path + "," + duck.getDeviceId();

  offline.path = "";

  String jsonstat;
  root.printTo(jsonstat);
  root.prettyPrintTo(Serial);

  if (client.publish(topic, jsonstat.c_str()))
  {
    Serial.println("Publish ok");
    root.prettyPrintTo(Serial);
    Serial.println("");
  }
  else
  {
    Serial.println("Publish failed");
  }
}

String buildPayload(String * val, int packetSize) {
  
}
