/**
 * @file file_drop.cpp
 * @brief Receive files over WiFi (HTTP) and write them to the SD card.
 *        Device-only (WebServer + SD + WiFi); excluded from the sim build.
 *
 * Endpoints:
 *   GET  /                 upload page (browser, multi-file)
 *   POST /upload?path=/x   multipart upload written to SD at `path`
 *   GET  /list?dir=/x      recursive text listing (verification)
 */

#include "file_drop.h"

#include <Arduino.h>
#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>
#include <lvgl.h>
#include <string.h>

#include "../../system/sdcard.h"
#include "../../system/wifi_mgr.h"
#include "../../ui/nav.h"
#include "../../ui/theme.h"

static WebServer*  s_server     = nullptr;
static lv_timer_t* s_pump_timer = nullptr;
static lv_obj_t*   s_status     = nullptr;
static File        s_upload_file;
static char        s_upload_path[128] = {0};
static bool        s_upload_ok  = false;
static uint16_t    s_received   = 0;

// ---------------------------------------------------------------------------
// Upload page (PT-BR, user-facing)
// ---------------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"html(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CacaOS - mandar arquivos</title>
<style>
 body{font-family:sans-serif;background:#ffeef5;color:#5a3d4d;max-width:420px;
      margin:24px auto;padding:0 16px}
 h1{font-size:1.3em;color:#e8739e}
 select,input,button{width:100%;margin:6px 0;padding:10px;border-radius:10px;
      border:1px solid #f3b8cf;background:#fff;font-size:1em;box-sizing:border-box}
 button{background:#e8739e;color:#fff;border:0;cursor:pointer}
 pre{background:#fff;border-radius:10px;padding:10px;min-height:3em;
      white-space:pre-wrap;word-break:break-all}
</style></head><body>
<h1>mandar arquivos &#128140;</h1>
<label for="dir">pasta de destino</label>
<select id="dir">
 <option value="/photos">fotos (galeria)</option>
 <option value="/open_when">cartinhas (abra quando...)</option>
 <option value="/memory_pairs">jogo da memoria</option>
 <option value="/">raiz do cartao</option>
</select>
<input id="custom" placeholder="ou digita uma pasta (ex: /photos)">
<input id="files" type="file" multiple>
<button onclick="send()">enviar</button>
<pre id="log"></pre>
<script>
async function send(){
  const log=document.getElementById('log');
  let dir=document.getElementById('custom').value.trim()
        ||document.getElementById('dir').value;
  if(!dir.startsWith('/'))dir='/'+dir;
  if(dir.endsWith('/'))dir=dir.slice(0,-1);
  for(const f of document.getElementById('files').files){
    const p=dir+'/'+f.name;
    log.textContent+='enviando '+p+'... ';
    const d=new FormData();d.append('file',f,f.name);
    try{
      const r=await fetch('/upload?path='+encodeURIComponent(p),
                          {method:'POST',body:d});
      log.textContent+=(r.ok?'ok':'erro')+'\n';
    }catch(e){log.textContent+='erro\n';}
  }
  log.textContent+='pronto!\n';
}
</script></body></html>)html";

// ---------------------------------------------------------------------------
// SD helpers
// ---------------------------------------------------------------------------
static bool path_ok(const char* p) {
    return p[0] == '/' && !strstr(p, "..") &&
           strlen(p) < sizeof(s_upload_path);
}

static void mkdirs_for(const char* path) {
    char tmp[sizeof(s_upload_path)];
    strlcpy(tmp, path, sizeof(tmp));
    for (char* p = tmp + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (!SD.exists(tmp)) SD.mkdir(tmp);
        *p = '/';
    }
}

static void list_dir(const char* dirname, String& out, int depth) {
    if (depth > 4) return;
    File dir = SD.open(dirname);
    if (!dir || !dir.isDirectory()) return;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        if (f.isDirectory()) {
            out += String(f.path()) + "/\n";
            list_dir(f.path(), out, depth + 1);
        } else {
            out += String(f.path()) + "\t" + String((uint32_t)f.size()) + "\n";
        }
        f.close();
    }
    dir.close();
}

// ---------------------------------------------------------------------------
// Status label
// ---------------------------------------------------------------------------
static void update_status(void) {
    if (!s_status) return;
    char buf[128];
    snprintf(buf, sizeof(buf),
             "manda do navegador:\nhttp://%s\n\nrecebidos: %u",
             WiFi.localIP().toString().c_str(), s_received);
    lv_label_set_text(s_status, buf);
}

// ---------------------------------------------------------------------------
// HTTP handlers (run on the LVGL task via the pump timer; a large upload
// blocks the UI for its duration, which is fine for this screen)
// ---------------------------------------------------------------------------
static void handle_upload(void) {
    HTTPUpload& up = s_server->upload();
    switch (up.status) {
        case UPLOAD_FILE_START: {
            String path = s_server->arg("path");
            if (path.isEmpty()) path = "/" + up.filename;
            s_upload_ok = false;
            if (!path_ok(path.c_str())) {
                s_upload_path[0] = '\0';
                Serial.printf("[file_drop] rejected path '%s'\n", path.c_str());
                return;
            }
            strlcpy(s_upload_path, path.c_str(), sizeof(s_upload_path));
            mkdirs_for(s_upload_path);
            if (SD.exists(s_upload_path)) SD.remove(s_upload_path);
            s_upload_file = SD.open(s_upload_path, FILE_WRITE);
            Serial.printf("[file_drop] receiving %s\n", s_upload_path);
            break;
        }
        case UPLOAD_FILE_WRITE:
            if (s_upload_file) s_upload_file.write(up.buf, up.currentSize);
            break;
        case UPLOAD_FILE_END:
            if (s_upload_file) {
                s_upload_file.close();
                s_upload_ok = true;
                ++s_received;
                Serial.printf("[file_drop] saved %s (%u bytes)\n",
                              s_upload_path, up.totalSize);
                update_status();
            }
            break;
        case UPLOAD_FILE_ABORTED:
            if (s_upload_file) {
                s_upload_file.close();
                SD.remove(s_upload_path);
            }
            Serial.println(F("[file_drop] upload aborted"));
            break;
    }
}

static void server_begin(void) {
    s_server = new WebServer(80);
    s_server->on("/", HTTP_GET, []() {
        s_server->send_P(200, "text/html", INDEX_HTML);
    });
    s_server->on("/upload", HTTP_POST, []() {
        if (s_upload_ok) s_server->send(200, "text/plain", "ok\n");
        else             s_server->send(500, "text/plain", "erro\n");
    }, handle_upload);
    s_server->on("/list", HTTP_GET, []() {
        String dir = s_server->arg("dir");
        if (dir.isEmpty()) dir = "/";
        String out;
        out.reserve(1024);
        list_dir(dir.c_str(), out, 0);
        s_server->send(200, "text/plain", out);
    });
    s_server->onNotFound([]() {
        s_server->send(404, "text/plain", "404\n");
    });
    s_server->begin();
    Serial.printf("[file_drop] server on http://%s\n",
                  WiFi.localIP().toString().c_str());
}

static void pump_cb(lv_timer_t* /*t*/) {
    if (s_server) s_server->handleClient();
}

// ---------------------------------------------------------------------------
// Screen
// ---------------------------------------------------------------------------
static void screen_delete_cb(lv_event_t* /*e*/) {
    if (s_pump_timer) {
        lv_timer_delete(s_pump_timer);
        s_pump_timer = nullptr;
    }
    if (s_server) {
        s_server->stop();
        delete s_server;
        s_server = nullptr;
        Serial.println(F("[file_drop] server stopped"));
    }
    s_status = nullptr;
}

static void back_event_cb(lv_event_t* /*e*/) {
    nav_pop(NAV_ANIM_SLIDE_RIGHT);
}

void file_drop_show(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, theme_color_bg(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, screen_delete_cb, LV_EVENT_DELETE, NULL);

    // Header
    lv_obj_t* header = lv_obj_create(scr);
    lv_obj_set_size(header, 240, 40);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, theme_color_primary(), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* back_btn = lv_button_create(header);
    lv_obj_set_size(back_btn, 36, 28);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(back_btn, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_radius(back_btn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(back_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(back_btn, back_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, theme_color_accent(), LV_PART_MAIN);
    lv_obj_center(back_lbl);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Receber arquivos");
    lv_obj_set_style_text_color(title, theme_color_card(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    s_status = lv_label_create(scr);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status, 210);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_status, theme_color_text(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_status, LV_ALIGN_CENTER, 0, 0);

    // Preconditions: WiFi + SD (card may have been inserted after boot)
    if (!wifi_mgr_is_connected()) {
        lv_label_set_text(s_status, "conecta no wifi primeiro\n(Ajustes > configurar wifi)");
    } else if (!sdcard_is_mounted() && !sdcard_init()) {
        lv_label_set_text(s_status, "cartao SD nao encontrado :(\ncoloca o cartao e tenta de novo");
    } else {
        s_received = 0;
        server_begin();
        update_status();
        s_pump_timer = lv_timer_create(pump_cb, 20, NULL);
    }

    nav_push(scr, NAV_ANIM_SLIDE_LEFT);
}
