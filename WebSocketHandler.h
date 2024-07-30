#ifndef WEBSOCKETHANDLER_H
#define WEBSOCKETHANDLER_H

#include <ArduinoJson.h>

void _WssListenHandle(char* data, size_t length);
void WssResponseJson(const char* event, int statuscode, const char* message);
void WssResponseRfidEvent(const char* event, int statuscode, const char* epc, const char* tid, const char* rssi, const char* ant);
void TX_StopScan();

#endif // WEBSOCKETHANDLER_H
