#pragma once

// Starts WiFi (station mode) and SNTP time sync.
void netBegin();

// Call frequently from the network task. Kicks the WiFi stack if it has been
// down for WIFI_RECONNECT_S, reboots after WIFI_REBOOT_S offline, and serves
// OTA updates and the settings page. Returns true while WiFi is connected.
bool netMaintain();

// true once SNTP has delivered a plausible time.
bool timeValid();
