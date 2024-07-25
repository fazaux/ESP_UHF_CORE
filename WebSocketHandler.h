#ifndef WEBSOCKETHANDLER_H
#define WEBSOCKETHANDLER_H

#include <ArduinoJson.h>

void _WssListenHandle(char* data, size_t length);
void WssResponseJson(const char* event, int statuscode, const char* message);

#endif // WEBSOCKETHANDLER_H
