#pragma once

/**
 * WiFi configuration screen. Scans nearby networks, lets the user pick one and
 * enter its password, then applies + persists the credentials via wifi_mgr.
 *
 * Launched from the settings app. Pushes its own screen via nav_push() and
 * returns with nav_pop().
 */
void wifi_config_show(void);
