#include <cassert>
#include <cstdio>
#include <vector>
#include "Arduino.h"
#include "WiFi.h"
#include "LittleFS.h"
static unsigned long g_ms = 0; static int g_pin = 1;
unsigned long millis() { return g_ms; } void delay(unsigned long) {} void pinMode(int,int) {} void digitalWrite(int,int) {}
int digitalRead(int) { return g_pin; }
SerialC Serial; WiFiC WiFi; LittleFSC LittleFS;
void configTzTime(const char*,const char*,const char*) {} bool getLocalTime(struct tm*, unsigned long) { return false; }
#include "../board/src/main.cpp"

struct Step { int pin; int ms; };
static std::vector<Gesture> run(std::vector<Step> steps, int tail = 800) {
  std::vector<Gesture> out; g_ms = 100000;
  for (auto &s : steps) for (int i = 0; i < s.ms; i++) { g_pin = s.pin; g_ms++; Gesture g = pollButton(); if (g != G_NONE) out.push_back(g); }
  for (int i = 0; i < tail; i++) { g_pin = 1; g_ms++; Gesture g = pollButton(); if (g != G_NONE) out.push_back(g); }
  return out;
}
int main() {
  auto t = run({{1,500},{0,90},{1,10}});                       assert(t.size()==1 && t[0]==G_TAP);
  auto d = run({{1,500},{0,80},{1,120},{0,80},{1,10}});        assert(d.size()==1 && d[0]==G_DOUBLE);
  auto h = run({{1,500},{0,1500},{1,10}});                     assert(h.size()==1 && h[0]==G_HOLD);       // exactly one HOLD, no tap after release
  auto b = run({{1,500},{0,10},{1,10}});                       assert(b.empty());                          // 10 ms bounce ignored
  auto s = run({{1,500},{0,90},{1,500},{0,90},{1,10}});        assert(s.size()==2 && s[0]==G_TAP && s[1]==G_TAP);   // two slow taps = two taps
  auto tr = run({{1,500},{0,90},{1,100},{0,90},{1,100},{0,90},{1,10}}); assert(tr.size()>=1);              // triple: no crash
  printf("gesture tests passed (tap, double, hold, bounce, two slow taps)\n");
}
