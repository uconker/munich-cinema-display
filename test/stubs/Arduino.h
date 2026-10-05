#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <string>
unsigned long millis(); void delay(unsigned long); void pinMode(int,int); void digitalWrite(int,int); int digitalRead(int);
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1
struct SerialC{void begin(int){}}; extern SerialC Serial;
typedef std::string String_;
#include "WString_stub.h"
void configTzTime(const char*,const char*,const char* = nullptr);
bool getLocalTime(struct tm*, unsigned long = 5000);
