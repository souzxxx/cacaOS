/**
 * @file sdcard_sim.cpp
 * @brief sdcard module re-implemented for native. The SD-style API
 *        (SD.open / SD.exists) is provided by sd_impl.cpp + SD.h shim;
 *        LVGL paths like "S:/photos/01.jpg" resolve via LV_USE_FS_STDIO
 *        with LV_FS_STDIO_PATH set to "./sd_card" (see lv_conf.h).
 *
 *        So this file only manages the "is the SD root present?" flag
 *        used by sdcard_is_mounted() so apps can degrade gracefully.
 */

#include "../system/sdcard.h"

#include <Arduino.h>

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <string>

static std::string sd_root(void) {
    const char* env = getenv("CACAOS_SD_ROOT");
    if (env && *env) return std::string(env);
    return std::string("./sd_card");
}

static bool s_mounted = false;

bool sdcard_init(void) {
    struct stat st;
    if (::stat(sd_root().c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        Serial.printf("[sdcard:sim] root '%s' not found — degrading\n", sd_root().c_str());
        s_mounted = false;
        return false;
    }
    s_mounted = true;
    Serial.printf("[sdcard:sim] mounted at %s\n", sd_root().c_str());
    return true;
}

bool sdcard_is_mounted(void) { return s_mounted; }

size_t sdcard_total_bytes(void) {
    struct statvfs sv;
    if (::statvfs(sd_root().c_str(), &sv) != 0) return 0;
    return (size_t)(sv.f_blocks * sv.f_frsize);
}

size_t sdcard_free_bytes(void) {
    struct statvfs sv;
    if (::statvfs(sd_root().c_str(), &sv) != 0) return 0;
    return (size_t)(sv.f_bavail * sv.f_frsize);
}

bool sdcard_file_exists(const char* path) {
    if (!path) return false;
    struct stat st;
    std::string p = path;
    if (p.empty() || p[0] != '/') p = "/" + p;
    return ::stat((sd_root() + p).c_str(), &st) == 0;
}
