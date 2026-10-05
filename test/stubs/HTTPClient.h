#pragma once
#include "Arduino.h"
#define HTTPC_STRICT_FOLLOW_REDIRECTS 1
struct HTTPClient { void setTimeout(int){} void setFollowRedirects(int){} bool begin(WiFiClientSecure&,const char*){return false;} int GET(){return 0;} String getString(){return String();} void end(){} };
