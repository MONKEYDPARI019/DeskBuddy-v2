#ifndef NET_H
#define NET_H

// Networking: WiFi (WiFiManager portal on first boot), NTP, mDNS, OTA,
// the HTTP /notify endpoint, weather, and — Phase 7 — the WebSocket server
// the Android app (or tools/deskbuddy_client.py) connects to.
// Protocol: docs/PROTOCOL.md. Everything runs in the net task on core 0.

#include <Arduino.h>

enum NetStatus : uint8_t {
    NET_OFFLINE = 0,
    NET_PORTAL,        // config access point "DeskBuddy_Setup" is up
    NET_CONNECTED
};

void      netInit();                 // starts the net task
NetStatus netStatus();
bool      netWifiConnected();
int       netAppClients();           // connected WebSocket clients
void      netRequestWeather();       // fetch as soon as possible
void      netResetWifi();            // forget credentials and reboot into the portal

// Device -> app messages (safe to call from any task)
void netSendJson(const char* json);
void netSendButton(uint8_t btn, const char* action);
void netSendTouch();
void netSendPttStart();
void netSendPttEnd();
void netSendState();                 // face / screen / silent / unread snapshot
void netSendAudio(const uint8_t* pcm, size_t len);   // mic frames (binary)

#endif // NET_H
