#pragma once
// Firmware update from the GitHub releases (built by
// .github/workflows/firmware.yml): checked once after each start.

// Firmware build number = number of commits on main when it was built
// (tools/version.py). GitHub's release "build-<N>" is installed if N is higher.
int firmwareBuild();

// Asks GitHub for the newest release and, if it is newer, installs it and
// restarts. Returns if there is none or anything failed (logged). Needs WiFi
// and a valid time (TLS). Blocks up to a minute while downloading.
void updateCheckAndInstall();

// true while updateCheckAndInstall() downloads (the board shows a notice).
bool updateRunning();
