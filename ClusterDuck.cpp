#include "ClusterDuck.h"

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);

IPAddress apIP(192, 168, 1, 1);
WebServer webServer(80);

auto tymer = timer_create_default(); // create a timer with default settings

String ClusterDuck::_deviceId = "";

ClusterDuck::ClusterDuck() {


}

void ClusterDuck::setDeviceId(String deviceId, const int formLength) {
  _deviceId = deviceId;

  byte codes[15] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xB1, 0xB2, 0xB3, 0xB4, 0xC1, 0xC2, 0xD1, 0xD2, 0xD3, 0xE4};
  for (int i = 0; i < 15; i++) {
    byteCodes[i] = codes[i];
  }

  String * values = new String[formLength];

  formArray = values;
  fLength = formLength;
}

void ClusterDuck::begin(int baudRate) {
  Serial.begin(baudRate);
  Serial.println("Serial start");
  //Serial.println(_deviceId + " says Quack!");
}

void ClusterDuck::setupDisplay(String deviceType)  {
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  u8x8.setCursor(0, 1);
  u8x8.print("    ((>.<))    ");

  u8x8.setCursor(0, 2);
  u8x8.print("  Project OWL  ");
  
  u8x8.setCursor(0, 4);
  u8x8.print("Device: " + deviceType);
  
  u8x8.setCursor(0, 5);
  u8x8.print("Status: Online");

  u8x8.setCursor(0, 6);
  u8x8.print("ID:     " + _deviceId); 

  u8x8.setCursor(0, 7);
  u8x8.print(duckMac(false));
}

// Initial LoRa settings
void ClusterDuck::setupLoRa(long BAND, int SS, int RST, int DI0, int TxPower) {
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  LoRa.setTxPower(TxPower);
  //LoRa.setSignalBandwidth(62.5E3);

  //Initialize LoRa
  if (!LoRa.begin(BAND))
  {
    u8x8.clear();
    u8x8.drawString(0, 0, "Starting LoRa failed!");
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  else
  {
    Serial.println("LoRa On");
  }

  //  LoRa.setSyncWord(0xF3);         // ranges from 0-0xFF, default 0x34
  LoRa.enableCrc();             // Activate crc
}

//Setup Captive Portal
void ClusterDuck::setupPortal(const char *AP) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP);
  delay(200); // wait for 200ms for the access point to start before configuring

  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  Serial.println("Created Hotspot");

  dnsServer.start(DNS_PORT, "*", apIP);

  webServer.onNotFound([&]()
  {
    webServer.send(200, "text/html", portal);
  });

  webServer.on("/", [&]()
  {
    webServer.send(200, "text/html", portal);
  });

  webServer.on("/id", [&]() {
    webServer.send(200, "text/html", _deviceId);
  });

  webServer.on("/restart", [&]()
  {
    webServer.send(200, "text/plain", "Restarting...");
    delay(1000);
    restartDuck();
  });

  webServer.on("/mac", [&]() {
    String mac = duckMac(true);
    webServer.send(200, "text/html", mac);
  });

  // Test 👍👌😅

  webServer.begin();

  if (!MDNS.begin(DNS))
  {
    Serial.println("Error setting up MDNS responder!");
  }
  else
  {
    Serial.println("Created local DNS");
    MDNS.addService("http", "tcp", 80);
  }
}

//Run Captive Portal
bool ClusterDuck::runCaptivePortal() {
  dnsServer.processNextRequest();
  webServer.handleClient();
  if (webServer.arg(1) != "" || webServer.arg(1) != NULL) {
    Serial.println("data received");
    Serial.println(webServer.arg(1));
    return true;
  } else {
    return false;
  }
}

//Setup premade DuckLink with default settings
void ClusterDuck::setupDuckLink() {
  setupDisplay("Duck");
  setupLoRa();
  setupPortal();

  Serial.println("Duck Online");
}

void ClusterDuck::runDuckLink() { //TODO: Add webserver clearing after message sent

  if (runCaptivePortal()) {
    Serial.println("Portal data received");
    sendPayloadMessage(getPortalDataString());
    Serial.println("Message Sent");
  }

}

void ClusterDuck::setupMamaDuck() {
  setupDisplay("Mama");
  setupPortal();
  setupLoRa();

  //LoRa.onReceive(repeatLoRaPacket);
  LoRa.receive();

  Serial.println("MamaDuck Online");

  tymer.every(1800000, imAlive);
  tymer.every(43200000, reboot);
}

void ClusterDuck::runMamaDuck() {
  tymer.tick();

  int packetSize = LoRa.parsePacket();
  if (packetSize != 0) {
    byte whoIsIt = LoRa.peek();
    if(whoIsIt == senderId_B) {
      String * msg = getPacketData(packetSize);
      if(checkPath(_lastPacket.path)) {
        sendPayloadStandard(_lastPacket.payload, _lastPacket.senderId, _lastPacket.messageId, _lastPacket.path);
        LoRa.receive();
      }
    } else if(whoIsIt == ping_B) {
      LoRa.beginPacket();
      couple(iamhere_B, "1");
      LoRa.endPacket();
      Serial.println("Pong packet sent");
      LoRa.receive();
    }
  }
  if (runCaptivePortal()) {
    Serial.println("Portal data received");
    sendPayloadStandard(getPortalDataString());
    Serial.println("Message Sent");
    LoRa.receive();
  }
}

/**
  getPortalData
  Reads WebServer Parameters and couples into Data Struct
  @return coupled Data Struct
*/
String * ClusterDuck::getPortalDataArray() {
  //Array holding all form values
  String * val = formArray;

  for (int i = 0; i < fLength; ++i) {
    val[i] = webServer.arg(i);
  }

  return val;
}

String ClusterDuck::getPortalDataString() {
  //String holding all form values
  String val = "";

  for (int i = 0; i < fLength; ++i) {
    val = val + webServer.arg(i) + "*";
  }

  return val;
}

void ClusterDuck::sendPayloadMessage(String msg) {
  LoRa.beginPacket();
  couple(senderId_B, _deviceId);
  couple(messageId_B, uuidCreator());
  couple(payload_B, msg);
  couple(path_B, _deviceId);
  LoRa.endPacket(); 
}

void ClusterDuck::sendPayloadStandard(String msg, String senderId, String messageId, String path) {
  if(senderId == "") senderId = _deviceId;
  if(messageId == "") messageId = uuidCreator();
  if(path == "") {
    path = _deviceId;
  } else {
    path = path + "," + _deviceId;
  }
  
  LoRa.beginPacket();
  couple(senderId_B, senderId);
  couple(messageId_B, messageId);
  couple(path_B, path);
  LoRa.endPacket();
  
  Serial.println("Packet sent");
}

void ClusterDuck::couple(byte byteCode, String outgoing) {
  LoRa.write(byteCode);               // add byteCode
  LoRa.write(outgoing.length());      // add payload length
  LoRa.print(outgoing);               // add payload
}

//Decode LoRa message
String ClusterDuck::readMessages(byte mLength)  {
  String incoming = "";

  for (int i = 0; i < mLength; i++)
  {
    incoming += (char)LoRa.read();
  }
  return incoming;
}

char * ClusterDuck::readPath(byte mLength)  {
  char * arr = new char[mLength];

  for (int i = 0; i < mLength; i++)
  {
    arr[i] = (char)LoRa.read();
  }
  return arr;

}

/**
  receive
  Reads and Parses Received Packets
  @param packetSize
*/

bool ClusterDuck::checkPath(String path) {
  Serial.println("Checking Path");
  String temp = "";
  int len = path.length() + 1;
  char arr[len];
  path.toCharArray(arr, len);

  for (int i = 0; i < len; i++) {
    if (arr[i] == ',' || i == len - 1) {
      if (temp == _deviceId) {
        Serial.print(path);
        Serial.print("false");
        return false;
      }
      temp = "";
    } else {
      temp += arr[i];
    }
  }
  Serial.println("true");
  Serial.println(path);
  return true;
}

String * ClusterDuck::getPacketData(int pSize) {
  String * packetData = new String[pSize];
  int i = 0;
  byte byteCode, mLength;

  while (LoRa.available())
  {
    byteCode = LoRa.read();
    mLength  = LoRa.read();
    if (byteCode == senderId_B)
    {
      _lastPacket.senderId  = readMessages(mLength);
      Serial.println("User ID: " + _lastPacket.senderId);
    }
    else if (byteCode == messageId_B) {
      _lastPacket.messageId = readMessages(mLength);
      Serial.println("Message ID: " + _lastPacket.messageId);
    }
    else if (byteCode == payload_B) {
      _lastPacket.payload = readMessages(mLength);
      Serial.println("Message: " + _lastPacket.payload);
    }
    else if (byteCode == path_B) {
      _lastPacket.path = readMessages(mLength);
      Serial.println("Path: " + _lastPacket.path);
    } else {
      packetData[i] = readMessages(mLength);
      _lastPacket.payload = _lastPacket.payload + packetData[i];
      _lastPacket.payload = _lastPacket.payload + "*";
      //Serial.println("Data" + i + ": " + packetData[i]);
      i++;
    }
  }
  return packetData;
}

/**
  restart
  Only restarts ESP
*/
void ClusterDuck::restartDuck()
{
  Serial.println("Restarting Duck...");
  ESP.restart();
}

//Timer reboot
bool ClusterDuck::reboot(void *) {
  String reboot = "REBOOT";
  Serial.println(reboot);
  sendPayloadMessage(reboot);
  restartDuck();

  return true;
}

bool ClusterDuck::imAlive(void *) {
  String alive = "1";
  sendPayloadMessage(alive);
  Serial.print("alive");

  return true;
}

//Get Duck MAC address
String ClusterDuck::duckMac(boolean format)
{
  char id1[15];
  char id2[15];

  uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
  uint16_t chip = (uint16_t)(chipid >> 32);

  snprintf(id1, 15, "%04X", chip);
  snprintf(id2, 15, "%08X", (uint32_t)chipid);

  String ID1 = id1;
  String ID2 = id2;

  String unformattedMac = ID1 + ID2;

  if(format == true){
    String formattedMac = "";
    for(int i = 0; i < unformattedMac.length(); i++){
      if(i % 2 == 0 && i != 0){
        formattedMac += ":";
        formattedMac += unformattedMac[i];
      } 
      else {
        formattedMac += unformattedMac[i];
      }
    }
    return formattedMac;
  } else {
    return unformattedMac;
  }
}

//Create a uuid
String ClusterDuck::uuidCreator() {
  byte randomValue;
  char msg[50];     // Keep in mind SRAM limits
  int numBytes = 0;
  int i;

  numBytes = atoi("8");
  if (numBytes > 0)
  {
    memset(msg, 0, sizeof(msg));
    for (i = 0; i < numBytes; i++) {
      randomValue = random(0, 37);
      msg[i] = randomValue + 'a';
      if (randomValue > 26) {
        msg[i] = (randomValue - 26) + '0';
      }
    }
  }

  return String(msg);
}

void ClusterDuck::packetAvailable(int pSize) {
  _packetAvailable = true;
  _packetSize = pSize;
}

void ClusterDuck::loRaReceive() {
  LoRa.receive();
}

//Getters

String ClusterDuck::getDeviceId() {
  return _deviceId;
}

Packet ClusterDuck::getLastPacket() {
  return _lastPacket;
}

DNSServer ClusterDuck::dnsServer;
const char * ClusterDuck::DNS  = "duck";
const byte ClusterDuck::DNS_PORT = 53;

int ClusterDuck::_rssi = 0;
float ClusterDuck::_snr;
long ClusterDuck::_freqErr;
int ClusterDuck::_availableBytes;
int ClusterDuck::_packetSize = 0;

byte ClusterDuck::byteCodes[15];
String * ClusterDuck::formArray;
int ClusterDuck::fLength;

Packet ClusterDuck::_lastPacket;

byte ClusterDuck::ping_B       = 0xF4;
byte ClusterDuck::senderId_B   = 0xF5;
byte ClusterDuck::messageId_B  = 0xF6;
byte ClusterDuck::payload_B    = 0xF7;
byte ClusterDuck::iamhere_B    = 0xF8;
byte ClusterDuck::path_B       = 0xF3;
bool ClusterDuck::_packetAvailable = false;

String ClusterDuck::portal = MAIN_page;
