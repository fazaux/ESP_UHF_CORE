// =====================================
//  ,__,  
//  (O.o)   .aux was here | beware of the owl
//  /)__)    UART ttl UHF RFID.
// =="="== 
//
// =====================================
#include <vector>
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <EEPROM.h>
// ========= include custom module =====
#include "WebSocketHandler.h"

#define BUZZER_PIN 18
String ssid;
String password;
const char* ap_ssid = "WIFI_CONFIG";
const byte DNS_PORT = 53;

const size_t BUFFER_SIZE = 256; 
byte buffer[BUFFER_SIZE];
size_t bufferIndex = 0;

const byte _header[] = { 0xC8, 0x8C };
const byte _endFrame[] = { 0x0D, 0x0A };

DNSServer dnsServer;
AsyncWebSocketClient *_ctxSocket = nullptr;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool shouldSaveConfig = true;

void setup() {
  EEPROM.begin(512);
  Serial.begin(115200);  
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  
  initWifiConfig();
}

void loop() {
  if (shouldSaveConfig) {
      writeEEPROM(0, 32, ssid);
      writeEEPROM(32, 32, password);
      shouldSaveConfig = false;
  }
  dnsServer.processNextRequest();

  // ***Handle Rx gate from sensor**
  // *******************************
   Rx(); 
  // *******************************

}

void Rx() {
  // **Read wait until Rx not avail
  while (Serial2.available()) {
    byte b = Serial2.read();
    if (bufferIndex < BUFFER_SIZE) {
      buffer[bufferIndex++] = b;
    } else {
      Serial.println("Buffer overflow.");
      bufferIndex = 0;
      memset(buffer, 0, BUFFER_SIZE);
      return;
    }
  }
  //**process**
  //***************
  processBuffer();
  //***************
  
}

void processBuffer() {
  size_t i = 0;

  while (i <= bufferIndex - sizeof(_header) - sizeof(_endFrame)) {
    // Check for header
    if (buffer[i] == _header[0] && buffer[i + 1] == _header[1]) {
      // Frame length
      int frameLength = (buffer[i + 2] << 8) + buffer[i + 3];
      // Check if the frame length matches the remaining data length
      if (i + frameLength <= bufferIndex) {
        // Check for footer
        if (buffer[i + frameLength - sizeof(_endFrame)] == _endFrame[0] &&
            buffer[i + frameLength - 1] == _endFrame[1]) {
          byte frame[frameLength]; // Valid frame found
          memcpy(frame, buffer + i, frameLength);
          delay(10);
          RxDecode(frame, frameLength); // Move index to the end of the current frame
          i += frameLength;
        } else {
          i += 2; // Invalid footer, skip this frame and continue searching
        }
      } else {
        break; // Incomplete frame, wait for more data
      }
    } else {
      i++;
    }
  }
  // Clear buffer if there are any remaining bytes after processing
  if (i < bufferIndex) {
    memmove(buffer, buffer + i, bufferIndex - i);
    bufferIndex -= i;
  } else {
    bufferIndex = 0; // Reset buffer if no bytes remain
  }
}

void onWebSocketEvent(AsyncWebSocket * server, 
                      AsyncWebSocketClient * client, 
                      AwsEventType type, 
                      void * arg, 
                      uint8_t *data, 
                      size_t len) {

  if (type == WS_EVT_CONNECT) {
    Serial.println("WebSocket client connected");
    _ctxSocket = client;
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.println("WebSocket client disconnected");
  } else if (type == WS_EVT_DATA) {
    Serial.printf("Data received: %s\n", data);
    char* receivedData = (char*)data;
    receivedData[len] = '\0';
    _WssListenHandle(receivedData, len);
  }
}


// Filter from raw to string!
void RxDecode(const byte* rx, size_t length) {
  if (length < 5) {
    Serial.println("Invalid frame length.");
    return;
  }

  // Process based on the command byte (rx[4])
  switch (rx[4]) {
    case 0x03: {
      if (length < 8) {
        Serial.println("Frame too short for firmware data.");
        return;
      }
      byte firmware[] = { rx[5], rx[6], rx[7] };
      Serial.print("Firmware: ");
      for (byte b : firmware) {
        Serial.print(b, HEX);
        Serial.print(".");
      }
      char firmwareStr[9]; // 2 characters per byte + null terminator
      sprintf(firmwareStr, "%02X%02X%02X", firmware[0], firmware[1], firmware[2]);
      Serial.println(firmwareStr);
      break;
    }
    case 0x13: {
      if (length < 9) {
        Serial.println("Frame too short for power data.");
        return;
      }
      byte power[] = { rx[8], rx[7] };
      uint16_t intValue = (power[1] << 8) | power[0];  // Convert to uint16_t
      String resString = String(intValue / 100);
      Serial.print("Power dBm: ");
      Serial.println(resString);
      WssResponseJson("get-rfid-power", 1, resString.c_str());
      break;
    }
    case 0x83: {
      const size_t HEADER_LENGTH = 5;
      const size_t END_FRAME_LENGTH = 3;
      const size_t TID_LENGTH = 12;
      const size_t RSSI_LENGTH = 2;
      const size_t ATN_LENGTH = 1;
      // Calculate EPC length dynamically
      size_t epcStart = HEADER_LENGTH;
      size_t epcLength = length - (HEADER_LENGTH + TID_LENGTH + RSSI_LENGTH + ATN_LENGTH + END_FRAME_LENGTH);
      size_t epcEnd = epcStart + epcLength;

      // Extract EPC
      String epc;
      for (size_t i = epcStart; i < epcEnd; ++i) {
          if (rx[i] < 0x10) epc += "0";  // Add leading zero for single hex digits
          epc += String(rx[i], HEX);
      }
      
      // Extract TID
      size_t tidStart = epcEnd;
      size_t tidEnd = tidStart + TID_LENGTH;
      String tid;
      for (size_t i = tidStart; i < tidEnd; ++i) {
          if (rx[i] < 0x10) tid += "0";  // Add leading zero for single hex digits
          tid += String(rx[i], HEX);
      }
      
      // Extract RSSI
      size_t rssiStart = tidEnd;
      size_t rssiEnd = rssiStart + RSSI_LENGTH;
      String rssi;
      for (size_t i = rssiStart; i < rssiEnd; ++i) {
          if (rx[i] < 0x10) rssi += "0";  // Add leading zero for single hex digits
          rssi += String(rx[i], HEX);
      }
      
      // Extract ATN
      size_t antStart = rssiEnd;
      String ant;
      if (rx[antStart] < 0x10) ant += "0";  // Add leading zero for single hex digits
      ant += String(rx[antStart], HEX);
                  // Print extracted values
      Serial.print("EPC: ");
      Serial.println(epc);
      Serial.print("TID: ");
      Serial.println(tid);
      Serial.print("RSSI: ");
      Serial.println(rssi);
      Serial.print("ATN: ");
      Serial.println(ant);

      tone(BUZZER_PIN,3300, 50);
      // WssResponseRfidEvent(const char* event, int statuscode, const char* epc, const char* tid, const char* rssi, const char* ant)
      WssResponseRfidEvent("response-rfid-result", 1, epc.c_str(), tid.c_str(), rssi.c_str(), ant.c_str());
      break;
    }
    default:
      Serial.println("Unknown command.");
      break;
  }
}

// =================================
// helper function                ==
// =================================

// ************* EEPROM write & read *****************
String readEEPROM(int start, int length) {
    String value = "";
    for (int i = start; i < start + length; i++) {
        value += char(EEPROM.read(i));
    }
    return value;
}

void writeEEPROM(int start, int length, String value) {
    for (int i = start; i < start + length; i++) {
        if (i - start < value.length()) {
            EEPROM.write(i, value[i - start]);
        } else {
            EEPROM.write(i, 0);
        }
    }
    EEPROM.commit();
}
// ************ Access point **************************
void initWifiConfig(){
    ssid = readEEPROM(0, 32);
    password = readEEPROM(32, 32);

    // Print the loaded credentials
    Serial.print("Loaded SSID: ");
    Serial.println(ssid.c_str());
    Serial.print("Loaded Password: ");
    Serial.println(password.c_str());

    startAP();
      // If we have valid SSID and password, try to connect to Wi-Fi
    if (ssid.length() > 0 && password.length() > 0) {
        Serial.print("Connecting to ");
        Serial.println(ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());
        if (WiFi.waitForConnectResult() == WL_CONNECTED) {
            Serial.println("Connected to Wi-Fi!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            ws.onEvent(onWebSocketEvent);
            server.addHandler(&ws);
            Serial.println("WebSocket server started");
        } else {
            Serial.println("Failed to connect to Wi-Fi");
        }
    }
    capasitivePortDomain();
}

void startAP() {
    WiFi.mode(WIFI_AP_STA); // Enable both AP and STA modes
    WiFi.softAP(ap_ssid); // Start the AP without a password
    Serial.println("Access Point started");
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());

    // Setup DNS server to redirect all queries to the captive portal
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
}

void capasitivePortDomain(){
  // Start the web server for the captive portal
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", HTMLcaptivePortal());
    });
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            ssid = request->getParam("ssid", true)->value();
            password = request->getParam("password", true)->value();
            shouldSaveConfig = true;
            request->send(200, "text/html", "<h1>Configuration saved. Rebooting...</h1>");
            delay(1000);
            ESP.restart();
        } else {
            request->send(400, "text/html", "<h1>Invalid request</h1>");
        }
    });
    server.begin();
    // Set up mDNS
    if (!MDNS.begin("deras-rfid-config")) {
        Serial.println("Error starting mDNS");
    } else {
        Serial.println("mDNS started with domain: deras-rfid-config.local");
    }
}

String HTMLcaptivePortal() {
    String page = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">";
    page += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    page += "<title>WiFi Configuration</title></head><body>";
    page += "<h1>Configure WiFi</h1><form action=\"/save\" method=\"post\">";
    page += "<label for=\"ssid\">SSID:</label><input type=\"text\" id=\"ssid\" name=\"ssid\"><br>";
    page += "<label for=\"password\">Password:</label><input type=\"password\" id=\"password\" name=\"password\"><br>";
    page += "<input type=\"submit\" value=\"Save\"></form></body></html>";
    return page;
}