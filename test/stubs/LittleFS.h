#pragma once
#include "Arduino.h"
struct File { operator bool() const {return false;} size_t size(){return 0;} void close(){} size_t readBytes(char*,size_t){return 0;} void print(const String&){} };
struct LittleFSC { bool begin(bool){return false;} File open(const char*,const char*){return File();} }; extern LittleFSC LittleFS;
