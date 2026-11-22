// Actualizador desde GitHub Releases / GitHub Releases self-updater (compartido por las 4 apps).
// Se abre con ZR. Compara la version de la app con la ultima release del repositorio,
// descarga el .nro de esa release y sustituye el fichero de la app en la SD.
#pragma once
#include <switch.h>
#include <SDL.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif
#ifndef APP_TARGET
#define APP_TARGET "SwitchTools"
#endif

namespace {
namespace upd {

const char* REPO_API = "https://api.github.com/repos/jorgespfear/Switchtools/releases/latest";
const char* REPO_URL = "github.com/jorgespfear/Switchtools";

enum Phase { P_CHECK, P_NEW, P_SAME, P_ERR, P_DL, P_DONE };

struct St {
    Phase phase = P_CHECK;
    std::string latest, url, err, dest;
    double total = 0, done = 0, speed = 0, spT = 0, spB = 0, drawT = 0;
    bool cancel = false, quit = false;
};
St* cur = nullptr;
PadState pad;
bool padReady = false;

double now() { return (double)armTicksToNs(armGetSystemTick()) / 1e9; }

u64 down() {
    if(!padReady) { padConfigureInput(1, HidNpadStyleSet_NpadStandard); padInitializeDefault(&pad); padReady = true; }
    padUpdate(&pad);
    return padGetButtonsDown(&pad);
}

void waitRelease() {
    double t0 = now();
    while(now() - t0 < 1.0) { padUpdate(&pad); if(padGetButtons(&pad) == 0) break; svcSleepThread(20000000ULL); }
    down();
}

std::vector<int> parseVer(const std::string& s) {
    std::vector<int> v; size_t i = 0;
    while(i < s.size() && !std::isdigit((unsigned char)s[i])) i++;
    int n = 0; bool any = false;
    for(; i < s.size(); i++) {
        char c = s[i];
        if(std::isdigit((unsigned char)c)) { n = n * 10 + (c - '0'); any = true; }
        else if(c == '.') { v.push_back(n); n = 0; any = false; }
        else break;
    }
    if(any) v.push_back(n);
    return v;
}
// >0 si a es mas nueva que b
int cmpVer(const std::string& a, const std::string& b) {
    std::vector<int> x = parseVer(a), y = parseVer(b);
    size_t n = std::max(x.size(), y.size());
    for(size_t i = 0; i < n; i++) {
        int p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if(p != q) return p > q ? 1 : -1;
    }
    return 0;
}

// Lee el valor de "key": "valor" a partir de la posicion from.
bool jstr(const std::string& j, size_t from, const char* key, std::string& out, size_t* endPos = nullptr) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k, from);
    if(p == std::string::npos) return false;
    p += k.size();
    while(p < j.size() && (j[p] == ' ' || j[p] == ':' || j[p] == '\n' || j[p] == '\t' || j[p] == '\r')) p++;
    if(p >= j.size() || j[p] != '"') return false;
    p++; out.clear();
    while(p < j.size() && j[p] != '"') {
        if(j[p] == '\\' && p + 1 < j.size()) p++;
        out += j[p++];
    }
    if(endPos) *endPos = p;
    return true;
}

std::string lower(std::string s) { for(auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }

// ---------- pantalla ----------
void drawScreen(St& s) {
    fill(0, 0, W, H, C.bg);
    fill(0, 0, W, 78, C.panel);
    text(34, 22, T("ACTUALIZADOR", "UPDATER"), C.text, 3);
    text(700, 27, std::string(APP_TARGET), C.muted, 2, 540);

    fill(28, 98, 1224, 500, C.panel);
    text(52, 120, T("VERSION INSTALADA: ", "INSTALLED VERSION: ") + APP_VERSION, C.text, 2, 1100);
    text(52, 154, T("ULTIMA VERSION:    ", "LATEST VERSION:    ") + (s.latest.empty() ? std::string("--") : s.latest), C.text, 2, 1100);
    text(52, 188, T("REPOSITORIO: ", "REPOSITORY: ") + REPO_URL, C.muted, 2, 1100);

    std::string msg; SDL_Color mc = C.muted;
    switch(s.phase) {
    case P_CHECK: msg = T("BUSCANDO ACTUALIZACIONES...", "CHECKING FOR UPDATES..."); mc = C.cyan; break;
    case P_NEW:   msg = T("HAY UNA VERSION NUEVA. PULSA A PARA ACTUALIZAR", "A NEW VERSION IS AVAILABLE. PRESS A TO UPDATE"); mc = C.green; break;
    case P_SAME:  msg = T("YA TIENES LA ULTIMA VERSION", "YOU ALREADY HAVE THE LATEST VERSION"); mc = C.green; break;
    case P_ERR:   msg = s.err; mc = C.red; break;
    case P_DL:    msg = T("DESCARGANDO...", "DOWNLOADING..."); mc = C.orange; break;
    case P_DONE:  msg = T("ACTUALIZADO. VUELVE A ABRIR LA APP DESDE EL HOMEBREW MENU", "UPDATED. REOPEN THE APP FROM THE HOMEBREW MENU"); mc = C.green; break;
    }
    text(52, 250, msg, mc, 2, 1150);

    if(s.phase == P_DL) {
        double f = s.total > 0 ? std::min(1.0, s.done / s.total) : 0;
        fill(52, 320, 1100, 26, C.panel2);
        fill(52, 320, (int)(1100 * f), 26, C.accent);
        rect(52, 320, 1100, 26, C.border);
        text(52, 362, std::to_string((int)(f * 100)) + "%   " + human((uint64_t)s.done) + (s.total > 0 ? " / " + human((uint64_t)s.total) : std::string()), C.text, 2, 1100);
        char b[64]; std::snprintf(b, sizeof(b), "%.2f", s.speed / 1048576.0);
        text(52, 410, std::string(b), C.text, 6, 500);
        text(52, 465, "MB/S", C.muted, 2);
        std::snprintf(b, sizeof(b), "%.1f MBPS", s.speed * 8.0 / 1e6);
        text(200, 465, std::string(b), C.muted, 2, 400);
    }
    if(s.phase == P_DONE || s.phase == P_NEW || s.phase == P_SAME)
        text(52, 520, T("ARCHIVO: ", "FILE: ") + trimName(s.dest, 90), C.muted, 1, 1150);

    fill(28, 660, 1224, 36, C.panel);
    std::string foot;
    switch(s.phase) {
    case P_CHECK: foot = T("B CANCELAR", "B CANCEL"); break;
    case P_NEW:   foot = T("A ACTUALIZAR   B VOLVER", "A UPDATE   B BACK"); break;
    case P_SAME:  foot = T("A REINSTALAR   B VOLVER", "A REINSTALL   B BACK"); break;
    case P_ERR:   foot = T("A REINTENTAR   B VOLVER", "A RETRY   B BACK"); break;
    case P_DL:    foot = T("B CANCELAR", "B CANCEL"); break;
    case P_DONE:  foot = T("A / B SALIR DE LA APP", "A / B EXIT THE APP"); break;
    }
    text(48, 672, foot, C.muted, 1, 1160);
    SDL_RenderPresent(renderer);
}

// Callback de progreso de curl: dibuja, lee B y mide la velocidad.
int xfer(void*, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    St& s = *cur;
    if(!appletMainLoop()) { s.quit = true; return 1; }
    SDL_Event e; while(SDL_PollEvent(&e)) if(e.type == SDL_QUIT) { s.quit = true; return 1; }
    if(down() & HidNpadButton_B) { s.cancel = true; return 1; }
    double t = now();
    if(s.phase == P_DL) {
        if(dltotal > 0) s.total = (double)dltotal;
        s.done = (double)dlnow;
        if(s.spT == 0 || dlnow < s.spB) { s.spT = t; s.spB = (double)dlnow; }
        else if(t - s.spT >= 0.5) {
            double v = ((double)dlnow - s.spB) / (t - s.spT);
            s.speed = s.speed > 0 ? s.speed * 0.4 + v * 0.6 : v;
            s.spB = (double)dlnow; s.spT = t;
        }
    }
    if(t - s.drawT > 0.1) { s.drawT = t; drawScreen(s); }
    return 0;
}

size_t wrStr(char* p, size_t sz, size_t n, void* u) { ((std::string*)u)->append(p, sz * n); return sz * n; }
size_t wrFile(char* p, size_t sz, size_t n, void* u) { return std::fwrite(p, sz, n, (FILE*)u); }

std::string curlErr(CURLcode rc, long http) {
    if(rc == CURLE_ABORTED_BY_CALLBACK) return T("CANCELADO", "CANCELLED");
    if(rc != CURLE_OK) return std::string("ERROR: ") + curl_easy_strerror(rc);
    return std::string("HTTP ") + std::to_string(http);
}

CURL* newCurl(const std::string& url) {
    CURL* c = curl_easy_init();
    if(!c) return nullptr;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_USERAGENT, "SwitchTools-Updater");
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 8L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, xfer);
    return c;
}

// Busca la ultima release y el .nro de esta app.
void check(St& s) {
    s.phase = P_CHECK; s.err.clear(); s.latest.clear(); s.url.clear(); s.cancel = false;
    drawScreen(s);
    CURL* c = newCurl(REPO_API);
    if(!c) { s.phase = P_ERR; s.err = "CURL"; return; }
    std::string body;
    struct curl_slist* h = nullptr;
    h = curl_slist_append(h, "Accept: application/vnd.github+json");
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, wrStr);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    CURLcode rc = curl_easy_perform(c);
    long http = 0; curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_slist_free_all(h); curl_easy_cleanup(c);
    if(rc != CURLE_OK || http != 200) {
        s.phase = P_ERR;
        if(rc == CURLE_OK && http == 404) s.err = T("NO HAY NINGUNA RELEASE PUBLICADA EN EL REPOSITORIO", "NO RELEASE PUBLISHED IN THE REPOSITORY");
        else s.err = curlErr(rc, http);
        return;
    }
    if(!jstr(body, 0, "tag_name", s.latest)) { s.phase = P_ERR; s.err = T("RESPUESTA DE GITHUB NO VALIDA", "INVALID GITHUB RESPONSE"); return; }
    // buscar el asset NOMBRE.nro
    std::string want = lower(std::string(APP_TARGET) + ".nro");
    size_t pos = 0; std::string u;
    while(true) {
        size_t endp = 0;
        if(!jstr(body, pos, "browser_download_url", u, &endp)) break;
        size_t sl = u.find_last_of('/');
        std::string fn = lower(sl == std::string::npos ? u : u.substr(sl + 1));
        if(fn == want) { s.url = u; break; }
        pos = endp;
    }
    if(cmpVer(s.latest, APP_VERSION) > 0) {
        if(s.url.empty()) { s.phase = P_ERR; s.err = T("LA RELEASE ", "RELEASE ") + s.latest + T(" NO TIENE EL ARCHIVO ", " HAS NO FILE ") + APP_TARGET + ".nro"; }
        else s.phase = P_NEW;
    } else {
        s.phase = P_SAME;
    }
}

bool checkNro(const std::string& path, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if(!f) { err = T("NO SE PUDO ABRIR LO DESCARGADO", "COULD NOT OPEN THE DOWNLOAD"); return false; }
    unsigned char hd[0x14] = {0};
    size_t n = std::fread(hd, 1, sizeof(hd), f);
    std::fseek(f, 0, SEEK_END); long sz = std::ftell(f);
    std::fclose(f);
    if(n < sizeof(hd) || std::memcmp(hd + 0x10, "NRO0", 4) != 0 || sz < 65536) {
        err = T("EL ARCHIVO DESCARGADO NO ES UN .NRO VALIDO", "THE DOWNLOADED FILE IS NOT A VALID .NRO");
        return false;
    }
    return true;
}

void download(St& s) {
    if(s.url.empty()) { s.phase = P_ERR; s.err = T("NO HAY ARCHIVO PARA DESCARGAR EN LA RELEASE", "NO FILE TO DOWNLOAD IN THE RELEASE"); return; }
    std::string tmp = s.dest + ".new";
    s.phase = P_DL; s.err.clear(); s.total = s.done = s.speed = 0; s.spT = s.spB = 0; s.cancel = false;
    drawScreen(s);
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if(!f) { s.phase = P_ERR; s.err = T("NO SE PUDO CREAR ", "COULD NOT CREATE ") + tmp; return; }
    CURL* c = newCurl(s.url);
    if(!c) { std::fclose(f); s.phase = P_ERR; s.err = "CURL"; return; }
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, wrFile);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 30L);
    CURLcode rc = curl_easy_perform(c);
    long http = 0; curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(c);
    std::fclose(f);
    if(rc != CURLE_OK) {
        std::remove(tmp.c_str());
        s.phase = P_ERR; s.err = curlErr(rc, http);
        return;
    }
    std::string err;
    if(!checkNro(tmp, err)) { std::remove(tmp.c_str()); s.phase = P_ERR; s.err = err; return; }
    // sustituir: dest -> dest.bak, tmp -> dest
    std::string bak = s.dest + ".bak";
    std::remove(bak.c_str());
    bool had = std::rename(s.dest.c_str(), bak.c_str()) == 0;
    if(std::rename(tmp.c_str(), s.dest.c_str()) != 0) {
        if(had) std::rename(bak.c_str(), s.dest.c_str());
        s.phase = P_ERR; s.err = T("NO SE PUDO SUSTITUIR EL ARCHIVO. DESCARGA EN: ", "COULD NOT REPLACE THE FILE. DOWNLOAD AT: ") + tmp;
        return;
    }
    if(had) std::remove(bak.c_str());
    s.phase = P_DONE;
}

std::string destPath(const char* a0) {
    std::string p = a0 ? a0 : "";
    if(!p.empty() && p[0] == '/') p = "sdmc:" + p;
    struct stat st;
    bool ok = p.size() > 4 && lower(p.substr(p.size() - 4)) == ".nro" && stat(p.c_str(), &st) == 0;
    if(!ok) p = std::string("sdmc:/switch/") + APP_TARGET + ".nro";
    return p;
}

// Devuelve true si la app debe cerrarse (tras actualizar o al salir de la consola).
// netOk: la app ya tiene la red iniciada (SpeedTest y SaveSync). Si no, se inicia aqui.
bool run(const char* argv0, bool netOk = false) {
    St s; cur = &s;
    s.dest = destPath(argv0);
    bool ownNet = false;
    if(!netOk) {
        if(R_SUCCEEDED(socketInitializeDefault())) { ownNet = true; curl_global_init(CURL_GLOBAL_DEFAULT); }
        else { s.phase = P_ERR; s.err = T("NO SE PUDO INICIAR LA RED", "COULD NOT START NETWORKING"); }
    }
    down();
    if(s.phase != P_ERR) check(s);
    bool exitApp = false;
    while(!s.quit && !exitApp && appletMainLoop()) {
        SDL_Event e; while(SDL_PollEvent(&e)) if(e.type == SDL_QUIT) s.quit = true;
        u64 d = down();
        if(s.cancel) { s.cancel = false; if(s.phase == P_CHECK || s.phase == P_DL) s.phase = P_ERR; if(s.err.empty()) s.err = T("CANCELADO", "CANCELLED"); }
        if(d & HidNpadButton_B) { if(s.phase == P_DONE) exitApp = true; else break; }
        if(d & HidNpadButton_A) {
            if(s.phase == P_NEW || s.phase == P_SAME) { if(s.url.empty()) { s.phase = P_ERR; s.err = T("LA RELEASE NO TIENE EL .NRO DE ESTA APP", "THE RELEASE HAS NO .NRO FOR THIS APP"); } else download(s); }
            else if(s.phase == P_ERR) check(s);
            else if(s.phase == P_DONE) exitApp = true;
        }
        drawScreen(s);
        SDL_Delay(16);
    }
    if(ownNet) { curl_global_cleanup(); socketExit(); }
    waitRelease();
    bool done = (s.phase == P_DONE) || s.quit;
    cur = nullptr;
    return done;
}

} // namespace upd
} // namespace
