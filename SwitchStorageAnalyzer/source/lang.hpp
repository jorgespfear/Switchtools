// Idioma ES/EN compartido por todas las apps. ZL cambia el idioma; se recuerda en la SD.
#pragma once
#include <switch.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

int gLang = 0;   // 0 = espanol, 1 = ingles
const char* LANG_DIR  = "sdmc:/config/SwitchTools";
const char* LANG_FILE = "sdmc:/config/SwitchTools/lang.txt";

inline std::string T(const char* es, const char* en) { return std::string(gLang == 0 ? es : en); }

void langSave() {
    mkdir("sdmc:/config", 0777); mkdir(LANG_DIR, 0777);
    FILE* f = std::fopen(LANG_FILE, "wb");
    if(!f) return;
    std::fputs(gLang == 0 ? "es\n" : "en\n", f);
    std::fclose(f);
}

// Usa el idioma guardado; si no hay, el idioma del sistema (espanol si es ES/ES-419; si no, ingles).
void langInit() {
    char buf[16] = {0};
    FILE* f = std::fopen(LANG_FILE, "rb");
    if(f) {
        size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = 0;
        std::fclose(f);
        if(std::strncmp(buf, "es", 2) == 0) { gLang = 0; return; }
        if(std::strncmp(buf, "en", 2) == 0) { gLang = 1; return; }
    }
    gLang = 1;
    if(R_SUCCEEDED(setInitialize())) {
        u64 code = 0; SetLanguage lang = SetLanguage_ENUS;
        if(R_SUCCEEDED(setGetSystemLanguage(&code)) && R_SUCCEEDED(setMakeLanguage(code, &lang))) {
            if(lang == SetLanguage_ES || lang == SetLanguage_ES419) gLang = 0;
        }
        setExit();
    }
}

void langToggle() { gLang = gLang == 0 ? 1 : 0; langSave(); }

} // namespace
