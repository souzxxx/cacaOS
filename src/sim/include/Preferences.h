/**
 * @file Preferences.h (simulator stub)
 *
 * NVS-shaped key/value store backed by a JSON file at ~/.cacaos_sim_prefs.json
 * so state (tamagotchi, mood, scores) survives across sim runs.
 *
 * Thread-unsafe by design — the sim is single-threaded.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>
#include "Arduino.h"

class Preferences {
public:
    bool begin(const char* ns, bool readOnly = false);
    void end();
    bool clear();
    bool isKey(const char* key);
    bool remove(const char* key);

    // Putters
    size_t putBool   (const char* key, bool value);
    size_t putUChar  (const char* key, uint8_t value);
    size_t putChar   (const char* key, int8_t value);
    size_t putUShort (const char* key, uint16_t value);
    size_t putShort  (const char* key, int16_t value);
    size_t putUInt   (const char* key, uint32_t value);
    size_t putInt    (const char* key, int32_t value);
    size_t putULong  (const char* key, uint32_t value);
    size_t putLong   (const char* key, int32_t value);
    size_t putULong64(const char* key, uint64_t value);
    size_t putLong64 (const char* key, int64_t value);
    size_t putFloat  (const char* key, float value);
    size_t putDouble (const char* key, double value);
    size_t putString (const char* key, const char* value);
    size_t putString (const char* key, const String& value);
    size_t putBytes  (const char* key, const void* value, size_t len);

    // Getters
    bool     getBool   (const char* key, bool defaultValue = false);
    uint8_t  getUChar  (const char* key, uint8_t defaultValue = 0);
    int8_t   getChar   (const char* key, int8_t defaultValue = 0);
    uint16_t getUShort (const char* key, uint16_t defaultValue = 0);
    int16_t  getShort  (const char* key, int16_t defaultValue = 0);
    uint32_t getUInt   (const char* key, uint32_t defaultValue = 0);
    int32_t  getInt    (const char* key, int32_t defaultValue = 0);
    uint32_t getULong  (const char* key, uint32_t defaultValue = 0);
    int32_t  getLong   (const char* key, int32_t defaultValue = 0);
    uint64_t getULong64(const char* key, uint64_t defaultValue = 0);
    int64_t  getLong64 (const char* key, int64_t defaultValue = 0);
    float    getFloat  (const char* key, float defaultValue = 0.0f);
    double   getDouble (const char* key, double defaultValue = 0.0);
    size_t   getString (const char* key, char* value, size_t maxLen);
    String   getString (const char* key, const String& defaultValue = String());
    size_t   getBytesLength(const char* key);
    size_t   getBytes  (const char* key, void* buf, size_t maxLen);

private:
    std::string _ns;
    bool        _open = false;
    bool        _readOnly = false;

    std::string _fullKey(const char* key) const;
    bool        _hasKey(const std::string& full) const;
    void        _setRaw(const std::string& full, const std::string& value);
    std::string _getRaw(const std::string& full, const std::string& def) const;
};
