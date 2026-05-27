/**
 * @file SD.h (simulator stub)
 *
 * Maps the Arduino SD API onto the local filesystem rooted at
 * <repo>/sd_card/, so the sim sees the same content layout as the
 * physical SD card.
 *
 * Subset implemented: enough for daily_card, open_when, gallery,
 * memory_game, tamagotchi (sprite loads via SD.exists / SD.open).
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include "Arduino.h"

#ifndef FILE_READ
  #define FILE_READ "r"
#endif
#ifndef FILE_WRITE
  #define FILE_WRITE "w"
#endif
#ifndef FILE_APPEND
  #define FILE_APPEND "a"
#endif

class File {
public:
    File() = default;
    explicit File(const std::string& path, const char* mode);
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;
    ~File();

    explicit operator bool() const { return _fp != nullptr || _is_dir; }
    bool   isDirectory() const     { return _is_dir; }
    const  char* name() const      { return _basename.c_str(); }
    const  char* path() const      { return _path.c_str(); }
    size_t size();
    int    available();
    int    read();
    int    read(uint8_t* buf, size_t len);
    size_t write(const uint8_t* buf, size_t len);
    String readStringUntil(char terminator);
    File   openNextFile(const char* mode = FILE_READ);
    void   close();

private:
    std::string _path;
    std::string _basename;
    FILE*       _fp     = nullptr;
    void*       _dir    = nullptr;  // DIR* — opaque in header
    bool        _is_dir = false;
};

class SDClass {
public:
    bool   begin(uint8_t /*cs*/ = 0) { return true; }
    void   end() {}
    File   open(const char* path, const char* mode = FILE_READ);
    File   open(const String& path, const char* mode = FILE_READ) { return open(path.c_str(), mode); }
    bool   exists(const char* path);
    bool   exists(const String& path) { return exists(path.c_str()); }
    uint64_t totalBytes() { return 64ULL * 1024 * 1024 * 1024; }  // pretend 64GB
    uint64_t usedBytes()  { return 32ULL * 1024 * 1024; }         // pretend 32MB used
    uint8_t  cardType()   { return 1; }
};

extern SDClass SD;
