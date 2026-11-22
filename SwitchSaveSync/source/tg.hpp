// Cliente minimo de la API de bots de Telegram con libcurl.
#pragma once
#include <curl/curl.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "lang.hpp"

namespace tg {

std::string token, chatId, lastError;
bool (*progressFn)(double now, double total) = nullptr;   // false = cancelar
char errBuf[CURL_ERROR_SIZE];
const std::string API = "https://api.telegram.org/";
// Telegram: los bots solo pueden DESCARGAR archivos de hasta 20 MB, asi que se sube en trozos menores.
constexpr uint64_t CHUNK = 18ull * 1024 * 1024;

size_t writeStr(char* p, size_t s, size_t n, void* u) { ((std::string*)u)->append(p, s*n); return s*n; }
size_t writeFile(char* p, size_t s, size_t n, void* u) { return std::fwrite(p, s, n, (FILE*)u); }
int xferCb(void*, curl_off_t dlt, curl_off_t dln, curl_off_t ult, curl_off_t uln) {
    if(!progressFn) return 0;
    double total = ult > 0 ? (double)ult : (double)dlt;
    double now   = ult > 0 ? (double)uln : (double)dln;
    return progressFn(now, total) ? 0 : 1;
}

CURL* handle(const std::string& url) {
    CURL* c = curl_easy_init();
    if(!c) return nullptr;
    errBuf[0] = 0;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);   // la Switch no trae certificados raiz para curl
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 500L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errBuf);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, xferCb);
    return c;
}

std::string jsonStr(const std::string& s, const std::string& key, size_t from = 0) {
    std::string k = "\"" + key + "\":\"";
    size_t p = s.find(k, from);
    if(p == std::string::npos) return "";
    p += k.size();
    std::string out;
    while(p < s.size() && s[p] != '"') {
        if(s[p] == '\\' && p+1 < s.size()) { p++; if(s[p]=='u') { p += 4; continue; } }
        out += s[p]; p++;
    }
    return out;
}
long long jsonNum(const std::string& s, const std::string& key, size_t from = 0) {
    std::string k = "\"" + key + "\":";
    size_t p = s.find(k, from);
    if(p == std::string::npos) return 0;
    return std::atoll(s.c_str() + p + k.size());
}

void setError(CURLcode r, CURL* c, const std::string& body) {
    long code = 0; if(c) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    std::string d = jsonStr(body, "description");
    if(!d.empty()) lastError = d;
    else if(r != CURLE_OK) lastError = errBuf[0] ? std::string(errBuf) : std::string(curl_easy_strerror(r));
    else lastError = "HTTP " + std::to_string(code);
    if(lastError.size() > 70) lastError.resize(70);
}

// Llamada simple a un metodo del bot (GET).
bool call(const std::string& method, const std::string& query, std::string& out) {
    out.clear();
    CURL* c = handle(API + "bot" + token + "/" + method + (query.empty() ? "" : "?" + query));
    if(!c) { lastError = T("CURL NO INICIA","CURL FAILED TO START"); return false; }
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeStr);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L);
    CURLcode r = curl_easy_perform(c);
    bool ok = (r == CURLE_OK) && out.find("\"ok\":true") != std::string::npos;
    if(!ok) setError(r, c, out);
    curl_easy_cleanup(c);
    return ok;
}

// Averigua el chat_id: el usuario debe haber enviado /start (o cualquier mensaje) a su bot.
bool discoverChat() {
    std::string out;
    if(!call("getUpdates", "limit=100", out)) return false;
    size_t p = out.rfind("\"chat\":{\"id\":");
    if(p == std::string::npos) { lastError = T("ENVIA /START A TU BOT DESDE TELEGRAM","SEND /START TO YOUR BOT IN TELEGRAM"); return false; }
    chatId = std::to_string(std::atoll(out.c_str() + p + 13));
    return true;
}

bool getFilePath(const std::string& fileId, std::string& path) {
    std::string out;
    if(!call("getFile", "file_id=" + fileId, out)) return false;
    path = jsonStr(out, "file_path");
    if(path.empty()) { lastError = T("SIN RUTA DE ARCHIVO","NO FILE PATH RETURNED"); return false; }
    return true;
}

bool download(const std::string& fileId, std::string* toStr, FILE* toFile) {
    std::string path;
    if(!getFilePath(fileId, path)) return false;
    CURL* c = handle(API + "file/bot" + token + "/" + path);
    if(!c) { lastError = T("CURL NO INICIA","CURL FAILED TO START"); return false; }
    if(toFile) { curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeFile); curl_easy_setopt(c, CURLOPT_WRITEDATA, toFile); }
    else       { curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeStr);  curl_easy_setopt(c, CURLOPT_WRITEDATA, toStr); }
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 900L);
    CURLcode r = curl_easy_perform(c);
    bool ok = (r == CURLE_OK);
    if(!ok) setError(r, c, "");
    curl_easy_cleanup(c);
    return ok;
}

struct Part { FILE* f; uint64_t base, size, pos; };
size_t partRead(char* buf, size_t sz, size_t n, void* u) {
    Part* p = (Part*)u;
    uint64_t want = (uint64_t)sz * n;
    if(want > p->size - p->pos) want = p->size - p->pos;
    if(!want) return 0;
    size_t r = std::fread(buf, 1, (size_t)want, p->f);
    p->pos += r;
    return r;
}
int partSeek(void* u, curl_off_t off, int origin) {
    Part* p = (Part*)u;
    if(origin != SEEK_SET) return CURL_SEEKFUNC_CANTSEEK;
    p->pos = (uint64_t)off;
    std::fseek(p->f, (long)(p->base + p->pos), SEEK_SET);
    return CURL_SEEKFUNC_OK;
}
void partFree(void*) {}

// Sube [base, base+size) del archivo f como documento. Devuelve file_id y message_id.
bool sendDocument(FILE* f, uint64_t base, uint64_t size, const std::string& filename,
                  const std::string& caption, std::string& fileId, std::string& messageId) {
    std::string out;
    CURL* c = handle(API + "bot" + token + "/sendDocument");
    if(!c) { lastError = T("CURL NO INICIA","CURL FAILED TO START"); return false; }
    Part part{f, base, size, 0};
    std::fseek(f, (long)base, SEEK_SET);
    curl_mime* m = curl_mime_init(c);
    curl_mimepart* p = curl_mime_addpart(m);
    curl_mime_name(p, "chat_id"); curl_mime_data(p, chatId.c_str(), CURL_ZERO_TERMINATED);
    p = curl_mime_addpart(m);
    curl_mime_name(p, "caption"); curl_mime_data(p, caption.c_str(), CURL_ZERO_TERMINATED);
    p = curl_mime_addpart(m);
    curl_mime_name(p, "document"); curl_mime_filename(p, filename.c_str());
    curl_mime_type(p, "application/octet-stream");
    curl_mime_data_cb(p, (curl_off_t)size, partRead, partSeek, partFree, &part);
    curl_easy_setopt(c, CURLOPT_MIMEPOST, m);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeStr);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 1200L);
    CURLcode r = curl_easy_perform(c);
    bool ok = (r == CURLE_OK) && out.find("\"ok\":true") != std::string::npos;
    if(ok) {
        size_t d = out.find("\"document\"");
        fileId = jsonStr(out, "file_id", d == std::string::npos ? 0 : d);
        messageId = std::to_string(jsonNum(out, "message_id"));
        if(fileId.empty()) { ok = false; lastError = T("RESPUESTA SIN FILE_ID","RESPONSE WITHOUT FILE_ID"); }
    } else setError(r, c, out);
    curl_mime_free(m);
    curl_easy_cleanup(c);
    return ok;
}

// ---- Indice compartido: un documento "ssv-index.txt" anclado en el chat ----
const char* INDEX_NAME = "ssv-index.txt";

bool getIndex(std::string& text) {
    text.clear();
    std::string out;
    if(!call("getChat", "chat_id=" + chatId, out)) return false;
    size_t p = out.find("\"pinned_message\"");
    if(p == std::string::npos) return true;                       // aun no hay indice
    if(jsonStr(out, "file_name", p) != INDEX_NAME) return true;   // el anclado no es el nuestro
    std::string id = jsonStr(out, "file_id", p);
    if(id.empty()) return true;
    return download(id, &text, nullptr);
}

bool putIndex(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if(!f) { lastError = T("NO SE PUDO LEER EL INDICE LOCAL","COULD NOT READ THE LOCAL INDEX"); return false; }
    std::fseek(f, 0, SEEK_END); long sz = std::ftell(f);
    std::string fileId, msgId, out;
    bool ok = sendDocument(f, 0, (uint64_t)sz, INDEX_NAME, "SwitchSaveSync index (no borrar)", fileId, msgId);
    std::fclose(f);
    if(!ok) return false;
    return call("pinChatMessage", "chat_id=" + chatId + "&message_id=" + msgId + "&disable_notification=true", out);
}

} // namespace tg
