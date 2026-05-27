/**
 * @file sd_impl.cpp
 * @brief Implementation of the Arduino SD stub mapped onto the local
 *        filesystem at <repo>/sd_card/.
 */

#include <SD.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

SDClass SD;

// ---------------------------------------------------------------- helpers
static std::string sd_root(void) {
    // Allow override via env var; default is ./sd_card relative to cwd
    // (PlatformIO runs from the project root).
    const char* env = getenv("CACAOS_SD_ROOT");
    if (env && *env) return std::string(env);
    return std::string("./sd_card");
}

static std::string resolve_path(const char* sd_path) {
    if (!sd_path) return sd_root();
    std::string p = sd_path;
    // Strip optional driver letter prefix used by LVGL ("S:")
    if (p.size() >= 2 && p[1] == ':') p = p.substr(2);
    // Ensure leading slash so we get sd_root() + "/" + relative-or-absolute
    if (p.empty() || p[0] != '/') p = "/" + p;
    return sd_root() + p;
}

static bool is_dir(const std::string& full) {
    struct stat st;
    if (stat(full.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static std::string basename_of(const std::string& full) {
    size_t pos = full.find_last_of('/');
    return pos == std::string::npos ? full : full.substr(pos + 1);
}

// ---------------------------------------------------------------- File
File::File(const std::string& path, const char* mode) : _path(path) {
    _basename = basename_of(path);
    if (is_dir(path)) {
        _is_dir = true;
        _dir = (void*)opendir(path.c_str());
    } else {
        _fp = ::fopen(path.c_str(), mode);
    }
}

File::File(File&& other) noexcept
    : _path(std::move(other._path)),
      _basename(std::move(other._basename)),
      _fp(other._fp),
      _dir(other._dir),
      _is_dir(other._is_dir) {
    other._fp = nullptr;
    other._dir = nullptr;
    other._is_dir = false;
}

File& File::operator=(File&& other) noexcept {
    if (this != &other) {
        close();
        _path     = std::move(other._path);
        _basename = std::move(other._basename);
        _fp       = other._fp;       other._fp = nullptr;
        _dir      = other._dir;      other._dir = nullptr;
        _is_dir   = other._is_dir;   other._is_dir = false;
    }
    return *this;
}

File::~File() { close(); }

void File::close() {
    if (_fp)  { ::fclose(_fp); _fp = nullptr; }
    if (_dir) { ::closedir((DIR*)_dir); _dir = nullptr; }
    _is_dir = false;
}

size_t File::size() {
    if (!_fp) return 0;
    long cur = ::ftell(_fp);
    ::fseek(_fp, 0, SEEK_END);
    long sz = ::ftell(_fp);
    ::fseek(_fp, cur, SEEK_SET);
    return sz < 0 ? 0 : (size_t)sz;
}

int File::available() {
    if (!_fp) return 0;
    long cur = ::ftell(_fp);
    ::fseek(_fp, 0, SEEK_END);
    long end = ::ftell(_fp);
    ::fseek(_fp, cur, SEEK_SET);
    return (int)(end - cur);
}

int File::read() {
    if (!_fp) return -1;
    int c = ::fgetc(_fp);
    return c == EOF ? -1 : c;
}

int File::read(uint8_t* buf, size_t len) {
    if (!_fp || !buf) return 0;
    return (int)::fread(buf, 1, len, _fp);
}

size_t File::write(const uint8_t* buf, size_t len) {
    if (!_fp || !buf) return 0;
    return ::fwrite(buf, 1, len, _fp);
}

String File::readStringUntil(char terminator) {
    String out;
    if (!_fp) return out;
    int c;
    while ((c = ::fgetc(_fp)) != EOF) {
        if ((char)c == terminator) break;
        out._raw().push_back((char)c);
    }
    return out;
}

File File::openNextFile(const char* mode) {
    if (!_dir) return File();
    DIR* d = (DIR*)_dir;
    while (true) {
        struct dirent* de = ::readdir(d);
        if (!de) return File();
        // Skip '.' and '..'
        if (de->d_name[0] == '.' && (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0'))) continue;
        std::string child = _path + "/" + de->d_name;
        return File(child, mode);
    }
}

// ---------------------------------------------------------------- SDClass
File SDClass::open(const char* path, const char* mode) {
    return File(resolve_path(path), mode);
}

bool SDClass::exists(const char* path) {
    struct stat st;
    return ::stat(resolve_path(path).c_str(), &st) == 0;
}
