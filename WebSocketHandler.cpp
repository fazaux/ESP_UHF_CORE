#include "HardwareSerial.h"
#include "WebSocketHandler.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Declare _ctxSocket as an external variable
extern AsyncWebSocketClient *_ctxSocket;

#define BUZZZER_PIN  18


// function prototype
void WssResponseJson(const char* event, int statuscode, const char* message);
void WssResponseRfidEvent(const char* event, int statuscode, const char* epc, const char* tid, const char* rssi, const char* ant);
void TX_StartScan();
void TX_StopScan();



void _WssListenHandle(char* data, size_t length) {
  StaticJsonDocument<256> json;
  DeserializationError error = deserializeJson(json, data);

  if (!error) {

    const char* event = json["event"];

    Serial.print("Event: ");
    Serial.println(event);


    if(strcmp(event, "get-version") == 0){
      const byte getFirmwareVersion[] = { 0xC8, 0x8C, 0x00, 0x08, 0x02, 0x0A, 0x0D, 0x0A };
      WssResponseJson("get-version", 1, "success");
      Serial2.write(getFirmwareVersion, sizeof(getFirmwareVersion));
    }
    else if(strcmp(event, "scan-rfid-on") == 0){
      static const byte continouesScan[] = { 0xC8, 0x8C, 0x00, 0x0A, 0x82, 0x27, 0x10, 0xBF, 0x0D, 0x0A };
      Serial2.write(continouesScan, sizeof(continouesScan));
      WssResponseJson("scan-rfid-on", 1, "success");
    }
    else if(strcmp(event, "scan-rfid-off") == 0){
      static const byte stopScan[] = { 0xC8, 0x8C, 0x00, 0x08, 0x8C, 0x84, 0x0D, 0x0A };
      Serial2.write(stopScan, sizeof(stopScan));
      WssResponseJson("scan-rfid-off", 1, "success");
    }
    else if(strcmp(event, "get-rfid-power") == 0){
      const byte getTransmitPower[] = { 0xC8, 0x8C, 0x00, 0x08, 0x12, 0x1A, 0x0D, 0x0A };
      Serial2.write(getTransmitPower, sizeof(getTransmitPower));
    }
    else if(strcmp(event, "set-rfid-power") == 0) {
      JsonVariant value = json["value"];
      if(value.isNull()) {
        WssResponseJson(event, 0, "Value is null");
      } else if(value.is<int>()) {
        int intValue = value.as<int>();
        int decimalValue = intValue * 100;
        Serial.println("[tx] set-power:");
        Serial.print(decimalValue);
        if (decimalValue > 0xFFFF) {
          Serial.println("Error: Payload too large.");
        } else {
          byte setA, setB;
          Serial.println("[set-power]");
          setA = (decimalValue >> 8) & 0xFF; // High byte
          setB = decimalValue & 0xFF;        // Low byte
          byte setPower[] = { 0xC8, 0x8C, 0x00, 0x0E, 0x10, 0x02, 0x01, setA, setB, setA, setB, 0x1D, 0x0D, 0x0A };
          Serial2.write(setPower, sizeof(setPower));
          WssResponseJson(event, 1, "success");
        }
      } else {
        WssResponseJson(event, 0, "Value is not an integer");
      }
    }
    else if(strcmp(event, "set-tone-on") == 0){
      int value = json["value"];
      tone(BUZZZER_PIN,value);
      Serial.print(value);
      WssResponseJson("set-tone-on", 1, "success");
    }else if(strcmp(event, "set-tone-off") == 0){
      noTone(BUZZZER_PIN);
      WssResponseJson("set-tone-off", 1, "success");
    }
    else{
      Serial.println("event not valid!");
      WssResponseJson("error", 0, "event not valid");
    }
  delay(50);
  } else {
    WssResponseJson("error", 0, "Failed Json Format");
  }
}

// TX Command
void TX_StopScan(){
  Serial.print("[TX] Stop Scan");
  static const byte stopScan[] = { 0xC8, 0x8C, 0x00, 0x08, 0x8C, 0x84, 0x0D, 0x0A };
  Serial2.write(stopScan, sizeof(stopScan));
  for (int i = 0; i <= 2; i++) {
      static const byte stopScan[] = { 0xC8, 0x8C, 0x00, 0x08, 0x8C, 0x84, 0x0D, 0x0A };
      Serial2.write(stopScan, sizeof(stopScan));
      delay(50);
  }
}
void TX_StartScan(){
  Serial.print("[TX] Start Scan");
  static const byte continouesScan[] = { 0xC8, 0x8C, 0x00, 0x0A, 0x82, 0x27, 0x10, 0xBF, 0x0D, 0x0A };
  Serial2.write(continouesScan, sizeof(continouesScan));
}


void WssResponseJson(const char* event, int statuscode, const char* message) {
  StaticJsonDocument<200> doc;
  doc["event"] = event;
  doc["statusCode"] = statuscode;
  doc["message"] = message;
  String jsonString;
  serializeJson(doc, jsonString);
  if (_ctxSocket != nullptr) {
    _ctxSocket->text(jsonString.c_str());
  } else {
    Serial.println("No client connected");
  }
}

void WssResponseRfidEvent(const char* event, int statuscode, const char* epc, const char* tid, const char* rssi, const char* ant){
  delay(10);
  StaticJsonDocument<200> doc;
  doc["event"] = event;
  doc["statusCode"] = statuscode;
  doc["epc"] = epc;
  doc["tid"] = tid;
  doc["rssi"] = rssi;
  doc["ant"] = ant;
  String jsonString;
  serializeJson(doc, jsonString);
  if (_ctxSocket != nullptr) {
    _ctxSocket->text(jsonString.c_str());
  } else {
    Serial.println("No client connected");
  }
}
