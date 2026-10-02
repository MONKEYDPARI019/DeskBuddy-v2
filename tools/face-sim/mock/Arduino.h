#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
using std::min; using std::max;
extern unsigned long g_ms;
inline unsigned long millis() { return g_ms; }
inline long random(long a, long b) { return a + (rand() % (b - a)); }
inline long random(long b) { return rand() % b; }
#define F(x) x
#ifndef PI
#define PI 3.14159265358979f
#endif
struct SerialMock { template<class... A> void printf(const char* f, A... a) {} void println(const char*) {} void print(const char*) {} };
extern SerialMock Serial;
template<class T> T constrain(T v, T a, T b) { return v < a ? a : (v > b ? b : v); }
