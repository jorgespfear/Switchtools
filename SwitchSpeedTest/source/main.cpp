Claude


Switchspeedtest main · CPP
// Switch Speed Test: ping, descarga y subida contra el MISMO servidor (HTTP/HTTPS con libcurl).
#include "ui.hpp"
#include <curl/curl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
 
namespace {
 
struct Server { const char* name; const char* dlUrl; const char* ulUrl; };
// Solo los servidores con ulUrl pueden medir la subida; el resto muestran "--" en subida.
const Server SERVERS[] = {
    {"CLOUDFLARE",        "https://speed.cloudflare.com/__down?bytes=200000000", "https://speed.cloudflare.com/__up"},
    {"TELE2 (EUROPA)",    "https://speedtest.tele2.net/100MB.zip",               "https://speedtest.tele2.net/upload.php"},
    {"VULTR (MADRID)",    "https://mad-es-ping.vultr.com/vultr.com.100MB.bin",   nullptr},
    {"OVH (FRANCIA)",     "https://proof.ovh.net/files/100Mb.dat",               nullptr},
    {"HETZNER (ALEMANIA)","https://speed.hetzner.de/100MB.bin",                  nullptr},
};
constexpr int NUM_SERVERS = (int)(sizeof(SERVERS)/sizeof(SERVERS[0]));
constexpr double TEST_SECONDS = 10.0;
int sel = 0; // 0 = AUTO, 1..N = servidor concreto
 
bool quitApp = false, cancelled = false, running = false;
std::string phase = "LISTO", status = "PULSA A PARA EMPEZAR", serverName = "-";
double pingMs = -1, jitterMs = -1, downMbps = -1, upMbps = -1, liveMbps = 0, progress = 0;
std::vector<double> hist;
std::vector<std::string> logLines;
SDL_Color phaseColor = {145,155,178,255};
 
void logAdd(const std::string& l) { logLines.push_back(l); if(logLines.size()>9) logLines.erase(logLines.begin()); }
std::string val(double v, int dec = 1) { return v < 0 ? std::string("--") : fix(v, dec); }
std::string selName() { return sel==0 ? std::string("AUTO") : std::string(SERVERS[sel-1].name); }
 
void draw() {
    fill(0,0,W,H,C.bg);
    fill(0,0,W,78,C.panel);
    text(34,22,"SWITCH SPEED TEST",C.text,3);
    text(930,27,"TEST POR HTTPS",C.muted,2);
 
    card(28,98,390,150,"PING",val(pingMs,0),"MS",C.cyan);
    card(445,98,390,150,"DESCARGA",val(downMbps),"MBPS",C.green);
    card(862,98,390,150,"SUBIDA",val(upMbps),"MBPS",C.orange);
 
    fill(28,268,1224,330,C.panel);
    text(52,288,phase,phaseColor,3);
    text(52,330,"VELOCIDAD ACTUAL",C.muted,2);
    text(52,360,running ? fix(liveMbps) : std::string("--"),C.text,6,500);
    text(52,415,"MBPS",C.muted,2);
    text(52,448,"< "+selName()+" >",running?C.muted:C.accent,2,500);
    text(52,478,"SERVIDOR: "+serverName,C.muted,1,500);
    text(52,505,"JITTER: "+(jitterMs<0?std::string("--"):fix(jitterMs)+" MS"),C.muted,2,500);
    bar(52,545,500,18,progress,phaseColor);
    if(!running && !logLines.empty()) {
        fill(580,300,640,270,C.panel2);
        text(595,310,"DETALLE DEL ERROR:",C.red,2);
        for(size_t i=0;i<logLines.size();i++) text(595,340+(int)i*24,logLines[i],C.text,1,610);
    } else graph(580,300,640,270,hist,phaseColor);
 
    fill(28,614,1224,36,C.panel);
    text(48,626,status,C.muted,1,1160);
    fill(28,660,1224,36,C.panel);
    text(48,672,"A EMPEZAR   B CANCELAR   IZQ/DER SERVIDOR   PLUS SALIR",C.muted,1);
    SDL_RenderPresent(renderer);
}
 
// Refresca pantalla y lee mandos. Devuelve false si hay que parar.
bool tick() {
    if(!appletMainLoop()) { quitApp = true; cancelled = true; }
    SDL_Event e; while(SDL_PollEvent(&e)) if(e.type==SDL_QUIT) { quitApp = true; cancelled = true; }
    u64 d = padDown();
    if(d & HidNpadButton_B) cancelled = true;
    if(d & HidNpadButton_Plus) { quitApp = true; cancelled = true; }
    draw();
    return !cancelled;
}
 
// ---- Estado de una transferencia ----
struct Xfer {
    bool started = false, stop = false;
    double t0 = 0, lastDraw = 0, lastSample = 0;
    uint64_t total = 0, lastBytes = 0, chunkLeft = 0;
};
Xfer X;
 
void xferReset() { X = Xfer(); }
 
// Actualiza muestras, grafica y pantalla. Devuelve false si hay que cortar.
bool xferUpdate() {
    double now = nowSec();
    if(!X.started) { X.started = true; X.t0 = now; X.lastSample = now; }
    if(now-X.lastSample >= 0.25) {
        liveMbps = (double)(X.total-X.lastBytes)*8.0/1e6/(now-X.lastSample);
        hist.push_back(liveMbps); X.lastBytes = X.total; X.lastSample = now;
    }
    progress = std::min(1.0,(now-X.t0)/TEST_SECONDS);
    if(now-X.lastDraw > 0.1) { X.lastDraw = now; if(!tick()) { X.stop = true; return false; } }
    if(now-X.t0 >= TEST_SECONDS) { X.stop = true; return false; }
    return true;
}
 
size_t dlWrite(char*, size_t sz, size_t n, void*) {
    size_t len = sz*n;
    X.total += len;
    return xferUpdate() ? len : 0;
}
size_t sinkWrite(char*, size_t sz, size_t n, void*) { return sz*n; }
size_t ulRead(char* buf, size_t sz, size_t n, void*) {
    size_t len = sz*n;
    if(!xferUpdate()) return CURL_READFUNC_ABORT;
    size_t give = (uint64_t)len < X.chunkLeft ? len : (size_t)X.chunkLeft;
    std::memset(buf,0,give);
    X.chunkLeft -= give; X.total += give;
    return give;
}
int progressCb(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    double now = nowSec();
    if(now-X.lastDraw > 0.1) { X.lastDraw = now; if(!tick()) return 1; }
    return cancelled ? 1 : 0;
}
 
char errBuf[CURL_ERROR_SIZE];
CURL* newHandle(const char* url) {
    CURL* c = curl_easy_init();
    if(!c) return nullptr;
    errBuf[0] = 0;
    curl_easy_setopt(c,CURLOPT_ERRORBUFFER,errBuf);
    curl_easy_setopt(c,CURLOPT_URL,url);
    curl_easy_setopt(c,CURLOPT_USERAGENT,"SwitchSpeedTest/0.2");
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,0L);   // la Switch no trae certificados raiz para curl
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,0L);
    curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(c,CURLOPT_MAXREDIRS,3L);
    curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,8L);
    curl_easy_setopt(c,CURLOPT_LOW_SPEED_LIMIT,1000L);
    curl_easy_setopt(c,CURLOPT_LOW_SPEED_TIME,8L);
    curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(c,CURLOPT_NOPROGRESS,0L);
    curl_easy_setopt(c,CURLOPT_XFERINFOFUNCTION,progressCb);
    return c;
}
 
std::string codeStr(CURL* c) {
    long code = 0; curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);
    return code ? " (HTTP "+std::to_string(code)+")" : "";
}
 
bool runPing(const Server& s) {
    phase = "PING"; phaseColor = C.cyan; progress = 0; hist.clear(); liveMbps = 0;
    status = std::string("MIDIENDO LATENCIA CON ")+s.name+"...";
    std::vector<double> t; std::string lastErr;
    for(int i=0;i<6;i++) {
        if(!tick()) return false;
        CURL* c = newHandle(s.dlUrl);
        if(!c) { lastErr = "CURL NO INICIA"; break; }
        curl_easy_setopt(c,CURLOPT_NOBODY,1L);
        curl_easy_setopt(c,CURLOPT_FRESH_CONNECT,1L);
        curl_easy_setopt(c,CURLOPT_FORBID_REUSE,1L);
        curl_easy_setopt(c,CURLOPT_TIMEOUT,12L);
        CURLcode r = curl_easy_perform(c);
        double ct = 0; curl_easy_getinfo(c,CURLINFO_CONNECT_TIME,&ct);
        if(ct > 0) { if(i>0) t.push_back(ct*1000.0); }   // la primera incluye DNS/arranque
        else lastErr = std::string(curl_easy_strerror(r))+codeStr(c);
        curl_easy_cleanup(c);
        progress = (i+1)/6.0;
    }
    if(t.empty()) { logAdd(std::string(s.name)+": "+(lastErr.empty()?"SIN CONEXION":lastErr)); return false; }
    double sum = 0; for(double v: t) sum += v;
    pingMs = sum/t.size();
    double jit = 0; for(size_t i=1;i<t.size();i++) jit += std::fabs(t[i]-t[i-1]);
    jitterMs = t.size()>1 ? jit/(t.size()-1) : 0;
    return true;
}
 
bool runDownload(const Server& s) {
    phase = "DESCARGA"; phaseColor = C.green; progress = 0; hist.clear(); liveMbps = 0;
    status = "DESCARGANDO...";
    xferReset();
    CURL* c = newHandle(s.dlUrl);
    if(!c) { logAdd("CURL NO INICIA"); return false; }
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,dlWrite);
    curl_easy_setopt(c,CURLOPT_TIMEOUT,40L);
    CURLcode r = curl_easy_perform(c);
    std::string code = codeStr(c);
    curl_easy_cleanup(c);
    if(cancelled) return false;
    if(X.total > 100000 && (r==CURLE_OK || X.stop)) {
        double el = nowSec()-X.t0; if(el < 0.1) el = 0.1;
        downMbps = (double)X.total*8.0/1e6/el; liveMbps = downMbps; progress = 1;
        return true;
    }
    logAdd(std::string(s.name)+" BAJADA: "+(errBuf[0]?std::string(errBuf).substr(0,60):std::string(curl_easy_strerror(r)))+code);
    return false;
}
 
bool runUpload(const Server& s) {
    phase = "SUBIDA"; phaseColor = C.orange; progress = 0; hist.clear(); liveMbps = 0;
    status = "SUBIENDO...";
    xferReset();
    constexpr uint64_t CHUNK = 8ull*1024*1024;
    CURL* c = newHandle(s.ulUrl);
    if(!c) { logAdd("CURL NO INICIA"); return false; }
    curl_slist* hdr = nullptr;
    hdr = curl_slist_append(hdr,"Content-Type: application/octet-stream");
    hdr = curl_slist_append(hdr,"Expect:");
    if(std::strstr(s.ulUrl,"cloudflare")) {
        hdr = curl_slist_append(hdr,"Origin: https://speed.cloudflare.com");
        hdr = curl_slist_append(hdr,"Referer: https://speed.cloudflare.com/");
    }
    curl_easy_setopt(c,CURLOPT_HTTPHEADER,hdr);
    curl_easy_setopt(c,CURLOPT_POST,1L);
    curl_easy_setopt(c,CURLOPT_READFUNCTION,ulRead);
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,sinkWrite);
    curl_easy_setopt(c,CURLOPT_TIMEOUT,40L);
    CURLcode r = CURLE_OK; std::string code; long httpCode = 0;
    while(!X.stop && !cancelled) {
        X.chunkLeft = CHUNK;
        curl_easy_setopt(c,CURLOPT_POSTFIELDSIZE_LARGE,(curl_off_t)CHUNK);
        r = curl_easy_perform(c);
        code = codeStr(c);
        curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&httpCode);
        if(r != CURLE_OK || httpCode >= 400) break;
    }
    std::string detail = errBuf[0] ? std::string(errBuf) : std::string(curl_easy_strerror(r));
    curl_easy_cleanup(c);
    curl_slist_free_all(hdr);
    if(cancelled) return false;
    if(httpCode >= 400) {
        logAdd(std::string(s.name)+" SUBIDA RECHAZADA:"+code);
        logAdd("BYTES ENVIADOS ANTES: "+std::to_string(X.total));
        return false;
    }
    if(X.total > 100000) {
        double el = nowSec()-X.t0; if(el < 0.1) el = 0.1;
        upMbps = (double)X.total*8.0/1e6/el; liveMbps = upMbps; progress = 1;
        return true;
    }
    logAdd(std::string(s.name)+" SUBIDA: "+detail.substr(0,60)+code);
    logAdd("BYTES ENVIADOS: "+std::to_string(X.total));
    return false;
}
 
void runAll() {
    running = true; cancelled = false; logLines.clear();
    std::vector<const Server*> list;
    if(sel==0) { for(int i=0;i<NUM_SERVERS;i++) list.push_back(&SERVERS[i]); }
    else list.push_back(&SERVERS[sel-1]);
 
    bool done = false; std::string note;
    for(const Server* sp : list) {
        const Server& s = *sp;
        pingMs = jitterMs = downMbps = upMbps = -1;
        serverName = s.name;
        if(!runPing(s)) { if(cancelled) break; continue; }
        if(!runDownload(s)) { if(cancelled) break; continue; }
        done = true;
        if(s.ulUrl) { if(!runUpload(s) && !cancelled) note = "LA SUBIDA FALLO EN ESTE SERVIDOR (VER DETALLE)"; }
        else note = "ESTE SERVIDOR SOLO MIDE BAJADA. ELIGE CLOUDFLARE O TELE2 PARA SUBIDA";
        break;
    }
    running = false;
    if(cancelled) { phase = "CANCELADO"; status = "TEST CANCELADO. PULSA A PARA REPETIR"; phaseColor = C.red; }
    else if(!done) { phase = "ERROR"; phaseColor = C.red; status = "NO SE PUDO MEDIR CON "+std::string(sel==0?"NINGUN SERVIDOR":"ESE SERVIDOR"); }
    else {
        phase = "COMPLETADO"; phaseColor = C.accent;
        status = note.empty() ? std::string("TEST TERMINADO CON ")+serverName+". PULSA A PARA REPETIR" : note;
    }
    cancelled = false;
}
 
} // namespace
 
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    // Buffers moderados; si la Switch no puede crear sockets con ellos (ENOBUFS), configuracion estandar.
    SocketInitConfig cfg = *socketGetDefaultInitConfig();
    cfg.tcp_tx_buf_size = 0x20000; cfg.tcp_rx_buf_size = 0x80000;
    cfg.tcp_tx_buf_max_size = 0x40000; cfg.tcp_rx_buf_max_size = 0x100000;
    bool netOk = R_SUCCEEDED(socketInitialize(&cfg));
    std::string netMode = "BUFFER AMPLIADO";
    if(netOk) {
        int probe = socket(AF_INET,SOCK_STREAM,0);
        if(probe < 0) { socketExit(); netOk = false; }
        else close(probe);
    }
    if(!netOk) {
        netOk = R_SUCCEEDED(socketInitializeDefault());
        netMode = "BUFFER ESTANDAR";
    }
    if(!initUI("Switch Speed Test")) { if(netOk) socketExit(); return 1; }
    if(netOk) curl_global_init(CURL_GLOBAL_DEFAULT);
    if(!netOk) status = "NO SE PUDO INICIAR LA RED";
    else status = "PULSA A PARA EMPEZAR  ("+netMode+")";
    while(!quitApp && appletMainLoop()) {
        SDL_Event e; while(SDL_PollEvent(&e)) if(e.type==SDL_QUIT) quitApp = true;
        u64 d = padDown();
        if(d & HidNpadButton_Plus) quitApp = true;
        if(!running) {
            if(d & HidNpadButton_Left)  sel = (sel+NUM_SERVERS)%(NUM_SERVERS+1);
            if(d & HidNpadButton_Right) sel = (sel+1)%(NUM_SERVERS+1);
        }
        if((d & HidNpadButton_A) && !running && netOk) runAll();
        draw();
        SDL_Delay(16);
    }
    shutdownUI();
    if(netOk) { curl_global_cleanup(); socketExit(); }
    return 0;
}
 
