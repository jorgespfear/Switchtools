// SD Benchmark: velocidad secuencial y aleatoria (4K) de lectura/escritura en la microSD.
#include "ui.hpp"
#include "lang.hpp"
#include "update.hpp"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <cstdlib>

namespace {

const char* TMP_PATH = "sdmc:/switch/SDBenchmark.tmp";
const int SIZES_MB[] = {64, 128, 256, 512};
int sizeIdx = 1;

bool quitApp = false, cancelled = false, running = false;
std::string phase, status;
double seqWrite = -1, seqRead = -1, rndRead = -1, rndWrite = -1;      // MB/s
double rndReadIops = -1, rndWriteIops = -1;
double liveMBs = 0, progress = 0;
std::vector<double> hist;
SDL_Color phaseColor = {145,155,178,255};
uint64_t sdTotal = 0, sdFree = 0;

std::string val(double v, int dec = 1) { return v < 0 ? std::string("--") : fix(v, dec); }

void readCapacity() {
    struct statvfs vfs; std::memset(&vfs,0,sizeof(vfs));
    if(statvfs("sdmc:/",&vfs)==0) {
        sdTotal = (uint64_t)vfs.f_blocks*vfs.f_frsize;
        sdFree  = (uint64_t)vfs.f_bavail*vfs.f_frsize;
    }
}

void draw() {
    fill(0,0,W,H,C.bg);
    fill(0,0,W,78,C.panel);
    text(34,22,"SD BENCHMARK",C.text,3);
    text(900,27,T("LECTURA / ESCRITURA","READ / WRITE SPEED"),C.muted,2);

    card(28,98,300,150,T("ESCRITURA SEQ","SEQ WRITE"),val(seqWrite),"MB/S",C.orange);
    card(344,98,300,150,T("LECTURA SEQ","SEQ READ"),val(seqRead),"MB/S",C.green);
    card(660,98,300,150,T("LECTURA 4K","READ 4K"),val(rndReadIops,0),"IOPS",C.cyan);
    card(976,98,276,150,T("ESCRITURA 4K","WRITE 4K"),val(rndWriteIops,0),"IOPS",C.accent);

    fill(28,268,1224,330,C.panel);
    text(52,288,phase,phaseColor,3);
    text(52,330,T("VELOCIDAD ACTUAL","CURRENT SPEED"),C.muted,2);
    text(52,360,running ? fix(liveMBs) : std::string("--"),C.text,6,500);
    text(52,415,"MB/S",C.muted,2);
    text(52,455,T("TAMANO DE PRUEBA: ","TEST SIZE: ")+std::to_string(SIZES_MB[sizeIdx])+T(" MB  (IZQ / DER)"," MB  (LEFT / RIGHT)"),C.muted,2,520);
    text(52,485,"MICROSD: "+human(sdTotal)+T("  LIBRE: ","  FREE: ")+human(sdFree),C.muted,2,520);
    text(52,515,T("REF. UHS-I: ~80-95 MB/S LECTURA","UHS-I REF: ~80-95 MB/S READ"),C.muted,2,520);
    bar(52,545,500,18,progress,phaseColor);
    graph(580,300,640,270,hist,phaseColor);

    fill(28,614,1224,36,C.panel);
    text(48,626,status,C.muted,1,1160);
    fill(28,660,1224,36,C.panel);
    text(48,672,T("A EMPEZAR   B CANCELAR   IZQ/DER TAMANO   PLUS SALIR   ZL IDIOMA: ES   ZR ACTUALIZAR","A START   B CANCEL   LEFT/RIGHT SIZE   PLUS EXIT   ZL LANGUAGE: EN   ZR UPDATE"),C.muted,1);
    SDL_RenderPresent(renderer);
}

bool tick() {
    if(!appletMainLoop()) { quitApp = true; cancelled = true; }
    SDL_Event e; while(SDL_PollEvent(&e)) if(e.type==SDL_QUIT) { quitApp = true; cancelled = true; }
    u64 d = padDown();
    if(d & HidNpadButton_B) cancelled = true;
    if(d & HidNpadButton_Plus) { quitApp = true; cancelled = true; }
    draw();
    return !cancelled;
}

uint32_t rngState = 0x12345678u;
uint32_t rnd() { rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5; return rngState; }

constexpr size_t BLOCK = 1u << 20;
unsigned char* buf = nullptr;

// Prueba secuencial con bloques de 1 MB. Devuelve MB/s o -1 si falla.
double seqTest(bool write) {
    phase = write ? T("ESCRITURA SECUENCIAL","SEQUENTIAL WRITE") : T("LECTURA SECUENCIAL","SEQUENTIAL READ");
    phaseColor = write ? C.orange : C.green;
    progress = 0; hist.clear(); liveMBs = 0;
    status = write ? T("ESCRIBIENDO ARCHIVO TEMPORAL...","WRITING TEMPORARY FILE...") : T("LEYENDO ARCHIVO TEMPORAL...","READING TEMPORARY FILE...");
    FILE* f = std::fopen(TMP_PATH, write ? "wb" : "rb");
    if(!f) { status = T("NO SE PUDO ABRIR EL ARCHIVO EN sdmc:/switch/","COULD NOT OPEN THE FILE IN sdmc:/switch/"); return -1; }
    std::setvbuf(f,nullptr,_IONBF,0);
    int blocks = SIZES_MB[sizeIdx];
    double t0 = nowSec(), lastDraw = t0, lastSample = t0; int lastBlock = 0;
    for(int i=0;i<blocks;i++) {
        size_t r = write ? std::fwrite(buf,1,BLOCK,f) : std::fread(buf,1,BLOCK,f);
        if(r != BLOCK) { std::fclose(f); status = write ? T("ERROR DE ESCRITURA (DISCO LLENO?)","WRITE ERROR (DISK FULL?)") : T("ERROR DE LECTURA","READ ERROR"); return -1; }
        double now = nowSec();
        if(now-lastSample >= 0.2) {
            liveMBs = (double)(i+1-lastBlock)/(now-lastSample);
            hist.push_back(liveMBs); lastBlock = i+1; lastSample = now;
        }
        progress = (double)(i+1)/blocks;
        if(now-lastDraw > 0.1) { lastDraw = now; if(!tick()) { std::fclose(f); return -1; } }
    }
    if(write) std::fflush(f);
    std::fclose(f);
    double el = nowSec()-t0; if(el < 0.01) el = 0.01;
    liveMBs = blocks/el; progress = 1;
    return blocks/el;
}

// Prueba aleatoria de 4 KB durante unos segundos. Devuelve IOPS o -1.
double randTest(bool write) {
    phase = write ? T("ESCRITURA ALEATORIA 4K","RANDOM WRITE 4K") : T("LECTURA ALEATORIA 4K","RANDOM READ 4K");
    phaseColor = write ? C.accent : C.cyan;
    progress = 0; hist.clear(); liveMBs = 0;
    status = T("ACCESOS ALEATORIOS DE 4 KB...","RANDOM 4 KB ACCESSES...");
    FILE* f = std::fopen(TMP_PATH, "r+b");
    if(!f) { status = T("NO SE PUDO ABRIR EL ARCHIVO TEMPORAL","COULD NOT OPEN THE TEMPORARY FILE"); return -1; }
    std::setvbuf(f,nullptr,_IONBF,0);
    const long slots = (long)SIZES_MB[sizeIdx]*256;   // bloques de 4 KB
    const double DURATION = 5.0; const int MAX_OPS = 20000;
    int ops = 0; double t0 = nowSec(), lastDraw = t0, lastSample = t0; int lastOps = 0;
    while(ops < MAX_OPS) {
        double now = nowSec();
        if(now-t0 >= DURATION) break;
        long off = (long)(rnd() % (uint32_t)slots) * 4096L;
        if(std::fseek(f,off,SEEK_SET)!=0) break;
        size_t r = write ? std::fwrite(buf,1,4096,f) : std::fread(buf,1,4096,f);
        if(r != 4096) break;
        if(write) std::fflush(f);
        ops++;
        now = nowSec();
        if(now-lastSample >= 0.25) {
            double iops = (double)(ops-lastOps)/(now-lastSample);
            liveMBs = iops*4096.0/1048576.0; hist.push_back(liveMBs);
            lastOps = ops; lastSample = now;
        }
        progress = std::min(1.0,(now-t0)/DURATION);
        if(now-lastDraw > 0.1) { lastDraw = now; if(!tick()) { std::fclose(f); return -1; } }
    }
    std::fclose(f);
    double el = nowSec()-t0; if(el < 0.01) el = 0.01;
    if(ops == 0) return -1;
    progress = 1;
    return ops/el;
}

void runAll() {
    running = true; cancelled = false;
    seqWrite = seqRead = rndRead = rndWrite = rndReadIops = rndWriteIops = -1;
    readCapacity();
    uint64_t need = (uint64_t)SIZES_MB[sizeIdx]*1048576ull + 32ull*1048576ull;
    if(sdFree < need) {
        status = T("NO HAY ESPACIO LIBRE SUFICIENTE PARA ESTA PRUEBA","NOT ENOUGH FREE SPACE FOR THIS TEST");
        phase = "ERROR"; phaseColor = C.red; running = false; return;
    }
    mkdir("sdmc:/switch", 0777);
    buf = (unsigned char*)std::malloc(BLOCK);
    if(!buf) { status = T("SIN MEMORIA","OUT OF MEMORY"); phase = "ERROR"; phaseColor = C.red; running = false; return; }
    for(size_t i=0;i<BLOCK;i+=4) { uint32_t r = rnd(); std::memcpy(buf+i,&r,4); }

    bool ok = true;
    double v;
    v = seqTest(true);  if(v < 0) ok = false; else seqWrite = v;
    if(ok) { v = seqTest(false); if(v < 0) ok = false; else seqRead = v; }
    if(ok) { v = randTest(false); if(v < 0) ok = false; else { rndReadIops = v; rndRead = v*4096.0/1048576.0; } }
    if(ok) { v = randTest(true);  if(v < 0) ok = false; else { rndWriteIops = v; rndWrite = v*4096.0/1048576.0; } }

    std::free(buf); buf = nullptr;
    std::remove(TMP_PATH);
    readCapacity();
    running = false;
    if(cancelled) { phase = T("CANCELADO","CANCELLED"); status = T("TEST CANCELADO. ARCHIVO TEMPORAL BORRADO","TEST CANCELLED. TEMPORARY FILE DELETED"); phaseColor = C.red; }
    else if(!ok) { phase = "ERROR"; phaseColor = C.red; }
    else { phase = T("COMPLETADO","COMPLETED"); phaseColor = C.accent; status = T("TEST TERMINADO. ARCHIVO TEMPORAL BORRADO. PULSA A PARA REPETIR","TEST FINISHED. TEMPORARY FILE DELETED. PRESS A TO RETRY"); }
    cancelled = false;
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    langInit();
    phase = T("LISTO","READY"); status = T("PULSA A PARA EMPEZAR","PRESS A TO START");
    if(!initUI("SD Benchmark")) return 1;
    readCapacity();
    while(!quitApp && appletMainLoop()) {
        SDL_Event e; while(SDL_PollEvent(&e)) if(e.type==SDL_QUIT) quitApp = true;
        u64 d = padDown();
        if(d & HidNpadButton_Plus) quitApp = true;
        if(!running && (d & HidNpadButton_ZL)) {
            langToggle();
            phase = T("LISTO","READY"); status = T("PULSA A PARA EMPEZAR","PRESS A TO START");
        }
        if(!running && (d & HidNpadButton_ZR)) { if(upd::run(argv[0], false)) quitApp = true; }
        if(!running) {
            if(d & HidNpadButton_Left)  sizeIdx = std::max(0,sizeIdx-1);
            if(d & HidNpadButton_Right) sizeIdx = std::min(3,sizeIdx+1);
            if(d & HidNpadButton_A) runAll();
        }
        draw();
        SDL_Delay(16);
    }
    shutdownUI();
    return 0;
}
