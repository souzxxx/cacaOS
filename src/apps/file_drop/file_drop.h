#pragma once

/**
 * "Receber arquivos" screen: starts an HTTP server on the local network and
 * writes uploaded files to the SD card. Server runs only while the screen
 * is open. Requires WiFi connected + SD mounted.
 */
void file_drop_show(void);
