// Switch Speed Test: ping, descarga y subida usando HTTP simple (sin TLS).
#include "ui.hpp"
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>

namespace {

struct Server { const char* host; const char* path; };
const Server DOWNLOADS[] = {
    {"speedtest.tele2.net", "/100MB.zip"},
    {"ipv4.download.thinkbroadband.com", "/100MB.zip"},
    {"proof.ovh.net", "/files/100Mb.dat"},
};
const Server UPLOADS[] = {
    {"speedtest.tele2.net", "/upload.php"},
};
constexpr double TEST_SECONDS = 10.0;

bool quitApp = false, cancelled = false, running = false;
std::string phase = "LISTO", status = "PULSA A PARA EMPEZAR", serverName = "-";
double pingMs = -1, jitterMs = -1, downMbps = -1, upMbps = -1, liveMbps = 0, progress = 0;
std::vector<double> hist;
SDL_Color phaseColor = {145,155,178,255};

std::string val(double v, int dec = 1) { return v < 0 ? std::string("--") : fix(v, dec); }

void draw() {
    fill(0,0,W,H,C.bg);
    fill(0,0,W,78,C.panel);
    text(34,22,"SWITCH SPEED TEST",C.text,3);
    text(930,27,"TEST POR HTTP",C.muted,2);

    card(28,98,390,150,"PING",val(pingMs,0),"MS",C.cyan);
    card(445,98,390,150,"DESCARGA",val(downMbps),"MBPS",C.green);
    card(862,98,390,150,"SUBIDA",val(upMbps),"MBPS",C.orange);

    fill(28,268,1224,330,C.panel);
    text(52,288,phase,phaseColor,3);
    text(52,330,"VELOCIDAD ACTUAL",C.muted,2);
    text(52,360,running ? fix(liveMbps) : std::string("--"),C.text,6,500);
    text(52,415,"MBPS",C.muted,2);
    text(52,470,"SERVIDOR: "+serverName,C.muted,2,500);
    text(52,505,"JITTER: "+(jitterMs<0?std::string("--"):fix(jitterMs)+" MS"),C.muted,2,500);
    bar(52,545,500,18,progress,phaseColor);
    graph(580,300,640,270,hist,phaseColor);

    fill(28,614,1224,36,C.panel);
    text(48,626,status,C.muted,1,1160);
    fill(28,660,1224,36,C.panel);
    text(48,672,"A EMPEZAR   B CANCELAR   PLUS SALIR",C.muted,1);
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

bool waitFd(int fd, bool forWrite, int ms) {
    fd_set s; FD_ZERO(&s); FD_SET(fd,&s);
    timeval tv; tv.tv_sec = ms/1000; tv.tv_usec = (ms%1000)*1000;
    int r = forWrite ? select(fd+1,nullptr,&s,nullptr,&tv) : select(fd+1,&s,nullptr,nullptr,&tv);
    return r > 0;
}

int connectTo(const char* host, int port, double* connectMs) {
    addrinfo hints; std::memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    char ps[16]; std::snprintf(ps,sizeof(ps),"%d",port);
    if(getaddrinfo(host,ps,&hints,&res)!=0 || !res) return -1;
    int fd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if(fd < 0) { freeaddrinfo(res); return -1; }
    int fl = fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,fl|O_NONBLOCK);
    double t0 = nowSec();
    int r = connect(fd,res->ai_addr,res->ai_addrlen);
    freeaddrinfo(res);
    if(r < 0) {
        if(errno!=EINPROGRESS && errno!=EWOULDBLOCK && errno!=EAGAIN) { close(fd); return -1; }
        if(!waitFd(fd,true,5000)) { close(fd); return -1; }
        int so = 0; socklen_t sl = sizeof(so);
        if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&so,&sl)!=0 || so!=0) { close(fd); return -1; }
    }
    if(connectMs) *connectMs = (nowSec()-t0)*1000.0;
    return fd;
}

bool sendAll(int fd, const char* data, size_t len) {
    size_t sent = 0; double t0 = nowSec();
    while(sent < len) {
        if(nowSec()-t0 > 8.0) return false;
        if(!waitFd(fd,true,100)) continue;
        ssize_t n = send(fd,data+sent,len-sent,0);
        if(n < 0) { if(errno==EAGAIN||errno==EWOULDBLOCK) continue; return false; }
        sent += (size_t)n;
    }
    return true;
}

bool runPing() {
    phase = "PING"; phaseColor = C.cyan; progress = 0; hist.clear(); liveMbps = 0;
    const char* host = DOWNLOADS[0].host; serverName = host;
    status = "MIDIENDO LATENCIA...";
    std::vector<double> t;
    for(int i=0;i<6;i++) {
        if(!tick()) return false;
        double ms = 0; int fd = connectTo(host,80,&ms);
        if(fd >= 0) { close(fd); if(i>0) t.push_back(ms); } // el primero incluye DNS
        progress = (i+1)/6.0;
    }
    if(t.empty()) { status = "SIN CONEXION A INTERNET (COMPRUEBA EL WIFI)"; return false; }
    double sum = 0; for(double v: t) sum += v;
    pingMs = sum/t.size();
    double jit = 0; for(size_t i=1;i<t.size();i++) jit += std::fabs(t[i]-t[i-1]);
    jitterMs = t.size()>1 ? jit/(t.size()-1) : 0;
    return true;
}

bool runDownload() {
    phase = "DESCARGA"; phaseColor = C.green; progress = 0; hist.clear(); liveMbps = 0;
    static char buf[131072];
    for(const Server& s : DOWNLOADS) {
        if(cancelled) return false;
        serverName = s.host; status = std::string("CONECTANDO A ")+s.host+"...";
        if(!tick()) return false;
        int fd = connectTo(s.host,80,nullptr);
        if(fd < 0) continue;
        std::string req = std::string("GET ")+s.path+" HTTP/1.1\r\nHost: "+s.host+
            "\r\nUser-Agent: SwitchSpeedTest\r\nAccept: */*\r\nConnection: close\r\n\r\n";
        if(!sendAll(fd,req.data(),req.size())) { close(fd); continue; }
        status = "DESCARGANDO...";
        std::string head; bool body = false; uint64_t total = 0, lastBytes = 0;
        double start = nowSec(), t0 = 0, lastDraw = 0, lastSample = 0;
        while(true) {
            double now = nowSec();
            if(now-lastDraw > 0.1) { if(!tick()) { close(fd); return false; } lastDraw = now; }
            if(!body && now-start > 15.0) break;
            if(body && now-t0 >= TEST_SECONDS) break;
            if(!waitFd(fd,false,100)) continue;
            ssize_t n = recv(fd,buf,sizeof(buf),0);
            if(n == 0) break;
            if(n < 0) { if(errno==EAGAIN||errno==EWOULDBLOCK) continue; break; }
            if(!body) {
                head.append(buf,(size_t)n);
                size_t p = head.find("\r\n\r\n");
                if(p == std::string::npos) { if(head.size()>8192) break; continue; }
                if(head.size()<12 || std::atoi(head.c_str()+9)!=200) break;
                body = true; t0 = nowSec(); lastSample = t0;
                total = head.size()-(p+4); lastBytes = total;
            } else total += (uint64_t)n;
            now = nowSec();
            if(body && now-lastSample >= 0.25) {
                liveMbps = (double)(total-lastBytes)*8.0/1e6/(now-lastSample);
                hist.push_back(liveMbps); lastBytes = total; lastSample = now;
            }
            if(body) progress = std::min(1.0,(now-t0)/TEST_SECONDS);
        }
        close(fd);
        if(body && total > 100000) {
            double el = nowSec()-t0; if(el < 0.1) el = 0.1;
            downMbps = (double)total*8.0/1e6/el; liveMbps = downMbps; progress = 1;
            return true;
        }
    }
    status = "NO SE PUDO DESCARGAR DE NINGUN SERVIDOR";
    return false;
}

bool runUpload() {
    phase = "SUBIDA"; phaseColor = C.orange; progress = 0; hist.clear(); liveMbps = 0;
    static char zeros[65536];
    constexpr uint64_t CONTENT = 100ull*1024*1024;
    for(const Server& s : UPLOADS) {
        if(cancelled) return false;
        serverName = s.host; status = std::string("CONECTANDO A ")+s.host+"...";
        if(!tick()) return false;
        int fd = connectTo(s.host,80,nullptr);
        if(fd < 0) continue;
        std::string req = std::string("POST ")+s.path+" HTTP/1.1\r\nHost: "+s.host+
            "\r\nUser-Agent: SwitchSpeedTest\r\nContent-Type: application/octet-stream\r\nContent-Length: "+
            std::to_string(CONTENT)+"\r\nConnection: close\r\n\r\n";
        if(!sendAll(fd,req.data(),req.size())) { close(fd); continue; }
        status = "SUBIENDO...";
        uint64_t total = 0, lastBytes = 0;
        double t0 = nowSec(), lastDraw = 0, lastSample = t0;
        while(total < CONTENT) {
            double now = nowSec();
            if(now-lastDraw > 0.1) { if(!tick()) { close(fd); return false; } lastDraw = now; }
            if(now-t0 >= TEST_SECONDS) break;
            if(!waitFd(fd,true,100)) continue;
            ssize_t n = send(fd,zeros,sizeof(zeros),0);
            if(n < 0) { if(errno==EAGAIN||errno==EWOULDBLOCK) continue; break; }
            total += (uint64_t)n;
            now = nowSec();
            if(now-lastSample >= 0.25) {
                liveMbps = (double)(total-lastBytes)*8.0/1e6/(now-lastSample);
                hist.push_back(liveMbps); lastBytes = total; lastSample = now;
            }
            progress = std::min(1.0,(now-t0)/TEST_SECONDS);
        }
        close(fd);
        if(total > 100000) {
            double el = nowSec()-t0; if(el < 0.1) el = 0.1;
            upMbps = (double)total*8.0/1e6/el; liveMbps = upMbps; progress = 1;
            return true;
        }
    }
    status = "LA SUBIDA NO ESTA DISPONIBLE (SERVIDOR NO RESPONDE)";
    return false;
}

void runAll() {
    running = true; cancelled = false;
    pingMs = jitterMs = downMbps = upMbps = -1;
    bool ok = runPing();
    bool dlOk = false;
    if(ok) dlOk = runDownload();
    if(ok && !cancelled) runUpload();
    running = false;
    if(cancelled) { phase = "CANCELADO"; status = "TEST CANCELADO. PULSA A PARA REPETIR"; phaseColor = C.red; }
    else if(!ok) { phase = "ERROR"; phaseColor = C.red; }
    else {
        phase = "COMPLETADO"; phaseColor = C.accent;
        if(dlOk) status = "TEST TERMINADO. PULSA A PARA REPETIR";
    }
    cancelled = false;
}

} // namespace

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    SocketInitConfig cfg = *socketGetDefaultInitConfig();
    cfg.tcp_tx_buf_size = 0x100000; cfg.tcp_rx_buf_size = 0x200000;
    cfg.tcp_tx_buf_max_size = 0x200000; cfg.tcp_rx_buf_max_size = 0x400000;
    cfg.sb_efficiency = 4;
    bool netOk = R_SUCCEEDED(socketInitialize(&cfg));
    if(!initUI("Switch Speed Test")) { if(netOk) socketExit(); return 1; }
    if(!netOk) status = "NO SE PUDO INICIAR LA RED";
    while(!quitApp && appletMainLoop()) {
        SDL_Event e; while(SDL_PollEvent(&e)) if(e.type==SDL_QUIT) quitApp = true;
        u64 d = padDown();
        if(d & HidNpadButton_Plus) quitApp = true;
        if((d & HidNpadButton_A) && !running && netOk) runAll();
        draw();
        SDL_Delay(16);
    }
    shutdownUI();
    if(netOk) socketExit();
    return 0;
}
