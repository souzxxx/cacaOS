/**
 * @file preferences_impl.cpp
 * @brief Implementation of the Preferences stub.
 *        Persists to ~/.cacaos_sim_prefs.json so state survives runs.
 */

#include <Preferences.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <fstream>
#include <sstream>

// ---------- shared in-process backing store ----------
namespace {

std::map<std::string, std::string>& store() {
    static std::map<std::string, std::string> s;
    return s;
}

std::string prefs_path() {
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.cacaos_sim_prefs.json";
}

bool g_loaded = false;
bool g_dirty  = false;

void load_from_disk() {
    if (g_loaded) return;
    g_loaded = true;
    std::ifstream f(prefs_path());
    if (!f.is_open()) return;
    // Trivial line-based format: "key=value\n" with backslash-escaped newlines in value.
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        // unescape
        std::string out;
        out.reserve(v.size());
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == '\\' && i + 1 < v.size()) {
                if (v[i+1] == 'n') { out.push_back('\n'); i++; }
                else if (v[i+1] == '\\') { out.push_back('\\'); i++; }
                else out.push_back(v[i]);
            } else out.push_back(v[i]);
        }
        store()[k] = out;
    }
}

void save_to_disk() {
    if (!g_dirty) return;
    std::ofstream f(prefs_path(), std::ios::trunc);
    if (!f.is_open()) return;
    for (auto& kv : store()) {
        std::string v;
        v.reserve(kv.second.size());
        for (char c : kv.second) {
            if (c == '\n') { v += "\\n"; }
            else if (c == '\\') { v += "\\\\"; }
            else v.push_back(c);
        }
        f << kv.first << "=" << v << "\n";
    }
    g_dirty = false;
}

} // namespace

// ---------- Preferences ----------
std::string Preferences::_fullKey(const char* key) const {
    return _ns + "." + (key ? key : "");
}

bool Preferences::_hasKey(const std::string& full) const {
    return store().find(full) != store().end();
}

void Preferences::_setRaw(const std::string& full, const std::string& value) {
    store()[full] = value;
    g_dirty = true;
}

std::string Preferences::_getRaw(const std::string& full, const std::string& def) const {
    auto it = store().find(full);
    return it == store().end() ? def : it->second;
}

bool Preferences::begin(const char* ns, bool readOnly) {
    load_from_disk();
    _ns = ns ? ns : "";
    _open = true;
    _readOnly = readOnly;
    return true;
}

void Preferences::end() {
    save_to_disk();
    _open = false;
}

bool Preferences::clear() {
    if (!_open || _readOnly) return false;
    std::string prefix = _ns + ".";
    for (auto it = store().begin(); it != store().end(); ) {
        if (it->first.compare(0, prefix.size(), prefix) == 0) it = store().erase(it);
        else ++it;
    }
    g_dirty = true;
    return true;
}

bool Preferences::isKey(const char* key) { return _hasKey(_fullKey(key)); }

bool Preferences::remove(const char* key) {
    if (!_open || _readOnly) return false;
    auto it = store().find(_fullKey(key));
    if (it == store().end()) return false;
    store().erase(it);
    g_dirty = true;
    return true;
}

// ---- putters ----
#define PUT_NUM(method, T, fmt)                                        \
    size_t Preferences::method(const char* key, T value) {             \
        if (!_open || _readOnly) return 0;                             \
        char buf[64];                                                  \
        ::snprintf(buf, sizeof(buf), fmt, value);                      \
        _setRaw(_fullKey(key), buf);                                   \
        return strlen(buf);                                            \
    }

PUT_NUM(putBool,    bool,     "%d")
PUT_NUM(putUChar,   uint8_t,  "%u")
PUT_NUM(putChar,    int8_t,   "%d")
PUT_NUM(putUShort,  uint16_t, "%u")
PUT_NUM(putShort,   int16_t,  "%d")
PUT_NUM(putUInt,    uint32_t, "%u")
PUT_NUM(putInt,     int32_t,  "%d")
PUT_NUM(putULong,   uint32_t, "%u")
PUT_NUM(putLong,    int32_t,  "%d")
PUT_NUM(putULong64, uint64_t, "%llu")
PUT_NUM(putLong64,  int64_t,  "%lld")
PUT_NUM(putFloat,   float,    "%.9g")
PUT_NUM(putDouble,  double,   "%.17g")

#undef PUT_NUM

size_t Preferences::putString(const char* key, const char* value) {
    if (!_open || _readOnly) return 0;
    _setRaw(_fullKey(key), value ? value : "");
    return value ? strlen(value) : 0;
}
size_t Preferences::putString(const char* key, const String& value) {
    return putString(key, value.c_str());
}

size_t Preferences::putBytes(const char* key, const void* value, size_t len) {
    if (!_open || _readOnly) return 0;
    std::string raw((const char*)value, len);
    _setRaw(_fullKey(key), raw);
    return len;
}

// ---- getters ----
#define GET_NUM(method, T, parser)                                     \
    T Preferences::method(const char* key, T defaultValue) {           \
        if (!_open) return defaultValue;                               \
        std::string full = _fullKey(key);                              \
        if (!_hasKey(full)) return defaultValue;                       \
        std::string s = _getRaw(full, "");                             \
        if (s.empty()) return defaultValue;                            \
        return (T)parser(s.c_str(), nullptr, 10);                      \
    }

GET_NUM(getUChar,  uint8_t,  strtoul)
GET_NUM(getChar,   int8_t,   strtol)
GET_NUM(getUShort, uint16_t, strtoul)
GET_NUM(getShort,  int16_t,  strtol)
GET_NUM(getUInt,   uint32_t, strtoul)
GET_NUM(getInt,    int32_t,  strtol)
GET_NUM(getULong,  uint32_t, strtoul)
GET_NUM(getLong,   int32_t,  strtol)

#undef GET_NUM

bool Preferences::getBool(const char* key, bool defaultValue) {
    return getUChar(key, defaultValue ? 1 : 0) != 0;
}

uint64_t Preferences::getULong64(const char* key, uint64_t defaultValue) {
    if (!_open) return defaultValue;
    std::string full = _fullKey(key);
    if (!_hasKey(full)) return defaultValue;
    return strtoull(_getRaw(full, "").c_str(), nullptr, 10);
}
int64_t Preferences::getLong64(const char* key, int64_t defaultValue) {
    if (!_open) return defaultValue;
    std::string full = _fullKey(key);
    if (!_hasKey(full)) return defaultValue;
    return strtoll(_getRaw(full, "").c_str(), nullptr, 10);
}
float Preferences::getFloat(const char* key, float defaultValue) {
    if (!_open) return defaultValue;
    std::string full = _fullKey(key);
    if (!_hasKey(full)) return defaultValue;
    return strtof(_getRaw(full, "").c_str(), nullptr);
}
double Preferences::getDouble(const char* key, double defaultValue) {
    if (!_open) return defaultValue;
    std::string full = _fullKey(key);
    if (!_hasKey(full)) return defaultValue;
    return strtod(_getRaw(full, "").c_str(), nullptr);
}

size_t Preferences::getString(const char* key, char* out, size_t maxLen) {
    if (!_open || !out || maxLen == 0) return 0;
    std::string s = _getRaw(_fullKey(key), "");
    size_t n = s.size();
    if (n >= maxLen) n = maxLen - 1;
    memcpy(out, s.data(), n);
    out[n] = '\0';
    return n;
}
String Preferences::getString(const char* key, const String& defaultValue) {
    if (!_open) return defaultValue;
    std::string full = _fullKey(key);
    if (!_hasKey(full)) return defaultValue;
    return String(_getRaw(full, ""));
}
size_t Preferences::getBytesLength(const char* key) {
    return _getRaw(_fullKey(key), "").size();
}
size_t Preferences::getBytes(const char* key, void* buf, size_t maxLen) {
    if (!_open || !buf) return 0;
    std::string s = _getRaw(_fullKey(key), "");
    size_t n = s.size();
    if (n > maxLen) n = maxLen;
    memcpy(buf, s.data(), n);
    return n;
}
