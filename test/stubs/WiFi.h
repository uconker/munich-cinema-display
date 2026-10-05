#pragma once
#include "Arduino.h"
#define WIFI_STA 1
#define WIFI_OFF 0
#define WL_CONNECTED 3
struct WiFiC { void mode(int){} void disconnect(bool){} int status(){return 0;} }; extern WiFiC WiFi;
