# 1 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino"
// ===================
//
//
// =================
# 6 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino" 2

# 8 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino" 2
# 9 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino" 2




# 14 "C:\\Users\\Administrator\\Desktop\\Magic\\playground\\ESP32_UHF_READER_FIRMWARE\\ESP32_UHF_READER_FIRMWARE.ino" 2


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


void setup() {
  Serial0.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);

  // Connect to your home network
  WiFi.begin(ssid, password);
  Serial0.println("Connecting to WiFi...");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial0.print(".");
  }

  Serial0.println();
  Serial0.println("Connected to WiFi");
  Serial0.print("IP address: ");
  Serial0.println(WiFi.localIP());

  // Attach WebSocket event handler
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // Start server
  server.begin();
  Serial0.println("WebSocket server started");
}

void loop() {
  if (Serial0.available()) {
    String command = Serial0.readStringUntil('\n');
    if (command == "hardware") {
      Serial2.write(getHardwareVersion, sizeof(getHardwareVersion));
      Serial0.println("[tx] hardware-version");
      delay(100);
    }
    else if (command == "firmware") {
      Serial2.write(getFirmwareVersion, sizeof(getFirmwareVersion));
      Serial0.println("[tx] firmware-version:");
      delay(100);
    }
    else if (command == "get-power"){
      Serial2.write(getTransmitPower, sizeof(getTransmitPower));
      Serial0.prtinln("[tx] get-power");
      delay(100);
    }
    else if (command == "scan-on") {
      Serial2.write(continouesScan, sizeof(continouesScan));
      Serial0.println("[tx] scan-on");
      delay(100);
    }
    else if (command == "scan-off") {
      Serial2.write(stopScan, sizeof(stopScan));
      Serial0.println("[tx] scan-off");
      delay(100);
    }
    else {
      Serial0.println("Unknown command");
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
      Serial0.println("Buffer overflow.");
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
    Serial0.println("WebSocket client connected");
    _ctxSocket = client;
  } else if (type == WS_EVT_DISCONNECT) {
    Serial0.println("WebSocket client disconnected");
  } else if (type == WS_EVT_DATA) {
    Serial0.printf("Data received: %s\n", data);
    // Echo the received message back to the client
    client->text((char*)data, len);
  }
}

void sendMessageToClient(const char* message) {
  if (_ctxSocket != nullptr) {
    _ctxSocket->text(message);
  } else {
    Serial0.println("No client connected");
  }
}

// Filter from raw to string!
void RxDecode(const byte* rx, size_t length) {
  if (length < 5) {
    Serial0.println("Invalid frame length.");
    return;
  }

  // Process based on the command byte (rx[4])
  switch (rx[4]) {
    case 0x03: {
      if (length < 8) {
        Serial0.println("Frame too short for firmware data.");
        return;
      }
      byte firmware[] = { rx[5], rx[6], rx[7] };
      Serial0.print("Firmware: ");
      for (byte b : firmware) {
        Serial0.print(b, 16);
        Serial0.print(".");
      }
      char firmwareStr[9]; // 2 characters per byte + null terminator
      sprintf(firmwareStr, "%02X%02X%02X", firmware[0], firmware[1], firmware[2]);
      sendMessageToClient(firmwareStr);
      Serial0.println();
      break;
    }
    case 0x13: {
      if (length < 9) {
        Serial0.println("Frame too short for power data.");
        return;
      }
      byte power[] = { rx[8], rx[7] };
      uint16_t intValue = (power[1] << 8) | power[0]; // Convert to uint16_t
      Serial0.print("Power dBm: ");
      Serial0.println(intValue / 100);
      break;
    }
    case 0x83: {
      Serial0.print("Tag: ");
      String message;
      for (size_t i = 0; i < length; ++i) {
        if (rx[i] < 0x10) message += "0"; // Add leading zero for single hex digits
        message += String(rx[i], 16);
      }
      Serial0.println(message);
      sendMessageToClient(message.c_str());
      break;
    }
    default:
      Serial0.println("Unknown command.");
      break;
  }
}
