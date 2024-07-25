#line 1 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino"
// ===================
//
//
// =================
#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>


const char* ssid = "HANO";
const char* password = "hanO1234";

const size_t BUFFER_SIZE = 256; 
byte buffer[BUFFER_SIZE];
size_t bufferIndex = 0;

const byte _header[] = { 0xC8, 0x8C };
const byte _endFrame[] = { 0x0D, 0x0A };
const byte getHardwareVersion[] = { 0xC8, 0x8C, 0x00, 0x08, 0x00, 0x08, 0x0D, 0x0A };
const byte getFirmwareVersion[] = { 0xC8, 0x8C, 0x00, 0x08, 0x02, 0x0A, 0x0D, 0x0A };
const byte continouesScan[] = { 0xC8, 0x8C, 0x00, 0x0A, 0x82, 0x27, 0x10, 0xBF, 0x0D, 0x0A };
const byte stopScan[] = { 0xC8, 0x8C, 0x00, 0x08, 0x8C, 0x84, 0x0D, 0x0A };
const byte getTransmitPower[] = { 0xC8, 0x8C, 0x00, 0x08, 0x12, 0x1A, 0x0D, 0x0A };
const byte getAntennaStatus[] = { 0xC8, 0x8C, 0x00, 0x08, 0x2A, 0x22, 0x0D, 0x0A };

AsyncWebSocketClient *_ctxSocket = nullptr;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");


#line 37 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino"
void setup();
#line 65 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino"
void loop();
#line 104 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino"
void Rx();
#line 122 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino"
void processBuffer();
#line 160 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino"
void onWebSocketEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
#line 179 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino"
void sendMessageToClient(const char* message);
#line 188 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino"
void RxDecode(const byte* rx, size_t length);
#line 37 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino"
void setup() {
  Serial.begin(115200);  
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  
  // Connect to your home network
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Attach WebSocket event handler
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // Start server
  server.begin();
  Serial.println("WebSocket server started");
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    if (command == "hardware") {
      Serial2.write(getHardwareVersion, sizeof(getHardwareVersion));
      Serial.println("[tx] hardware-version");
      delay(100); 
    }
    else if (command == "firmware") {
      Serial2.write(getFirmwareVersion, sizeof(getFirmwareVersion));
      Serial.println("[tx] firmware-version:");
      delay(100); 
    }
    else if (command == "get-power"){
      Serial2.write(getTransmitPower, sizeof(getTransmitPower));
      Serial.prtinln("[tx] get-power");
      delay(100);
    }
    else if (command == "scan-on") {
      Serial2.write(continouesScan, sizeof(continouesScan));
      Serial.println("[tx] scan-on");
      delay(100); 
    }
    else if (command == "scan-off") {
      Serial2.write(stopScan, sizeof(stopScan));
      Serial.println("[tx] scan-off");
      delay(100);
    }
    else {
      Serial.println("Unknown command");
    }
  }

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
          delay(50);
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
    // Echo the received message back to the client
    client->text((char*)data, len);
  }
}

void sendMessageToClient(const char* message) {
  if (_ctxSocket != nullptr) {
    _ctxSocket->text(message);
  } else {
    Serial.println("No client connected");
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
      sendMessageToClient(firmwareStr);
      Serial.println();
      break;
    }
    case 0x13: {
      if (length < 9) {
        Serial.println("Frame too short for power data.");
        return;
      }
      byte power[] = { rx[8], rx[7] };
      uint16_t intValue = (power[1] << 8) | power[0];  // Convert to uint16_t
      Serial.print("Power dBm: ");
      Serial.println(intValue / 100);
      break;
    }
    case 0x83: {
      Serial.print("Tag: ");
      String message;
      for (size_t i = 0; i < length; ++i) {
        if (rx[i] < 0x10) message += "0";  // Add leading zero for single hex digits
        message += String(rx[i], HEX);
      }
      Serial.println(message);
      sendMessageToClient(message.c_str());
      break;
    }
    default:
      Serial.println("Unknown command.");
      break;
  }
}

