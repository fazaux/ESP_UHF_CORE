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
#include "Utils.h"
#include "HTMLPage.h"

#define BUZZER_PIN 18
#define BUTTON_PIN 4

int buttonState = 0;    
int lastButtonState = 0;
bool BTN_SCAN_ON_STATE = false;

String ssid;
String password;
const char* ap_ssid = "UHF_RFID_ESP32_WROOM";
const byte DNS_PORT = 53;

const size_t BUFFER_SIZE = 1024; 
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
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  initWifiConfig();
  TX_StopScan();
  RingTone();
}

void loop() {
  if (shouldSaveConfig) {
      writeEEPROM(0, 32, ssid);
      writeEEPROM(32, 32, password);
      shouldSaveConfig = false;
  }
  dnsServer.processNextRequest();
  
  //** handle Button function**
  buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == LOW && lastButtonState == HIGH) {
    delay(500);
    BTN_SCAN_ON_STATE = !BTN_SCAN_ON_STATE;
    Serial.print("button:");
    Serial.println(BTN_SCAN_ON_STATE);
    if (BTN_SCAN_ON_STATE) {
      Serial.println("[Start]");
      TX_StartScan();
    } else {
      Serial.println("[Stop]");
      TX_StopScan();
    }
    lastButtonState = buttonState;
  }

  lastButtonState = buttonState;
 

  // ***Handle Rx gate from sensor**
  // *******************************
   Rx(); 
  // *******************************

}

void Rx() {
    int bytesToRead = Serial2.available();
    if (bytesToRead > 0) {
        byte buffer[bytesToRead];
        Serial2.readBytes(buffer, bytesToRead);

        int i = 0;

        while (i <= bytesToRead - sizeof(_header) - 2 - sizeof(_endFrame)) {
            // Check for header
            if (buffer[i] == _header[0] && buffer[i + 1] == _header[1]) {
                // Check for frame length
                int frameLength = (buffer[i + 2] << 8) + buffer[i + 3];

                // Check if the frame length matches the remaining data length
                if (i + frameLength <= bytesToRead) {
                    // Check for footer
                    if (buffer[i + frameLength - 2] == _endFrame[0] && buffer[i + frameLength - 1] == _endFrame[1]) {
                        // Valid frame found
                        byte frame[frameLength];
                        memcpy(frame, buffer + i, frameLength);
                        RxDecode(frame, frameLength);
                        // Move index to the end of the current frame
                        i += frameLength;
                    } else {
                        // Invalid footer, skip this frame and continue searching
                        i += 2;
                    }
                } else {
                    // Incomplete frame, wait for more data
                    break;
                }
            } else {
                i++; // Move forward to continue searching for the next header
            }
        }
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
    TX_StopScan();
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.println("WebSocket client disconnected");
    TX_StopScan();
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
      tone(BUZZER_PIN,3300, 20);
      delay(50);
      WssResponseRfidEvent("response-rfid-result", 1, epc.c_str(), tid.c_str(), rssi.c_str(), ant.c_str());
      break;
    }
    default:
      for (int i = 0; i < length; i++) {
            if (rx[i] < 0x10) {
                Serial.print("0"); // Add a leading zero for single-digit values
            }
            Serial.print(rx[i], HEX);
            Serial.print(" ");
      }
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
        request->send(200, "text/html", HTMLPage());
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

// String HTMLcaptivePortal() {
//     String page = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">";
//     page += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
//     page += "<title>WiFi Configuration</title></head><body>";
//     page += "<h1>Configure WiFi</h1><form action=\"/save\" method=\"post\">";
//     page += "<label for=\"ssid\">SSID:</label><input type=\"text\" id=\"ssid\" name=\"ssid\"><br>";
//     page += "<label for=\"password\">Password:</label><input type=\"password\" id=\"password\" name=\"password\"><br>";
//     page += "<input type=\"submit\" value=\"Save\"></form></body></html>";
//     return page;
// }