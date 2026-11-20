// Save Sync: copia las partidas guardadas a Telegram (tu propio bot) y las restaura en otra Switch.
#include "ui.hpp"
#include "tg.hpp"
#include "saves.hpp"
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <map>

namespace {

const std::string APP_DIR = "sdmc:/switch/SwitchSaveSync";
const std::string CFG_PATH = APP_DIR + "/config.ini";
const std::string STATE_PATH = APP_DIR + "/state.txt";
const std::string INDEX_PATH = APP_DIR + "/ssv-index.txt";
const std::string BACKUP_DIR = APP_DIR + "/backups";
constexpr int KEEP_PER_SAVE = 5;   // versiones por juego+usuario+consola que se mantienen en el indice

// ---------------- estado general ----------------
bool quitApp = false, cancelled = false;
std::string consoleName, statusText = "LISTO";
std::vector<sv::LocalSave> saves;

struct Entry {
    u64 tid = 0;
    std::string name, nick, console;
    long long ts = 0;
    u64 size = 0;
    std::vector<std::string> ids;
};
std::vector<Entry> cloud;

struct SyncRec { long long ts = 0; std::string sig; };
std::map<std::string, SyncRec> synced;   // clave: tid_uid

enum Screen { S_MAIN, S_CLOUD, S_CONFIRM };
Screen screen = S_MAIN;
int selMain = 0, scrMain = 0, selCloud = 0, scrCloud = 0;
int confirmEntry = -1, targetSel = 0;
std::vector<int> targets;

// ---------------- utilidades ----------------
std::string keyOf(const sv::LocalSave& s) { return sv::tidHex(s.tid) + "_" + sv::uidHex(s.uid); }
std::string dateStr(long long ts) {
    time_t t = (time_t)ts; struct tm* g = gmtime(&t);
    if(!g) return "?";
    char b[40]; std::snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d", g->tm_year+1900, g->tm_mon+1, g->tm_mday, g->tm_hour, g->tm_min);
    return b;
}
std::string sizeStr(u64 n) { return human(n); }
void makeDirs() {
    mkdir("sdmc:/switch", 0777); mkdir(APP_DIR.c_str(), 0777); mkdir(BACKUP_DIR.c_str(), 0777);
}
std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> v; std::string cur;
    for(char c : s) { if(c == sep) { v.push_back(cur); cur.clear(); } else cur += c; }
    v.push_back(cur);
    return v;
}

// ---------------- configuracion ----------------
void saveConfig() {
    FILE* f = std::fopen(CFG_PATH.c_str(), "wb");
    if(!f) return;
    std::fprintf(f, "# Save Sync - configuracion\n");
    std::fprintf(f, "# token: el que te da @BotFather al crear tu bot\n");
    std::fprintf(f, "token=%s\n", tg::token.c_str());
    std::fprintf(f, "# chat_id: se rellena solo despues de enviar /start a tu bot\n");
    std::fprintf(f, "chat_id=%s\n", tg::chatId.c_str());
    std::fprintf(f, "# nombre de esta consola (distinto en cada Switch)\n");
    std::fprintf(f, "console=%s\n", consoleName.c_str());
    std::fclose(f);
}
void loadConfig() {
    FILE* f = std::fopen(CFG_PATH.c_str(), "rb");
    if(f) {
        char line[512];
        while(std::fgets(line, sizeof(line), f)) {
            std::string l = line;
            while(!l.empty() && (l.back() == '\n' || l.back() == '\r' || l.back() == ' ')) l.pop_back();
            if(l.empty() || l[0] == '#') continue;
            size_t eq = l.find('=');
            if(eq == std::string::npos) continue;
            std::string k = l.substr(0, eq), v = l.substr(eq + 1);
            while(!v.empty() && v[0] == ' ') v.erase(0, 1);
            if(k == "token") tg::token = v;
            else if(k == "chat_id") tg::chatId = v;
            else if(k == "console") consoleName = v;
        }
        std::fclose(f);
    }
    if(consoleName.empty()) {
        char b[32]; std::snprintf(b, sizeof(b), "SWITCH-%04X", (unsigned)(armGetSystemTick() & 0xFFFF));
        consoleName = b;
        saveConfig();
    }
    consoleName = sv::clean(consoleName);
}

void loadState() {
    synced.clear();
    FILE* f = std::fopen(STATE_PATH.c_str(), "rb");
    if(!f) return;
    char line[256];
    while(std::fgets(line, sizeof(line), f)) {
        std::string l = line;
        while(!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        auto p = split(l, '\t');
        if(p.size() >= 3) { SyncRec r; r.ts = std::atoll(p[1].c_str()); r.sig = p[2]; synced[p[0]] = r; }
    }
    std::fclose(f);
}
void saveState() {
    FILE* f = std::fopen(STATE_PATH.c_str(), "wb");
    if(!f) return;
    for(auto& kv : synced) std::fprintf(f, "%s\t%lld\t%s\n", kv.first.c_str(), kv.second.ts, kv.second.sig.c_str());
    std::fclose(f);
}

// ---------------- pantalla de trabajo (progreso) ----------------
std::string busyTitle, busyLine, busyDetail;
double busyFrac = -1, lastBusyDraw = 0;

void drawBusy() {
    fill(0,0,W,H,C.bg);
    fill(0,0,W,78,C.panel);
    text(34,22,"SAVE SYNC",C.text,3);
    fill(28,120,1224,300,C.panel);
    text(60,150,trimName(busyTitle,40),C.accent,3,1150);
    text(60,215,busyLine,C.text,2,1150);
    text(60,250,trimName(busyDetail,80),C.muted,1,1150);
    if(busyFrac >= 0) {
        bar(60,300,1160,24,busyFrac,C.accent);
        text(60,340,fix(busyFrac*100,0)+"%",C.muted,2);
    }
    text(60,380,"B CANCELAR",C.muted,2);
    SDL_RenderPresent(renderer);
}

bool busyTick(bool force = false) {
    if(!appletMainLoop()) { quitApp = true; cancelled = true; }
    SDL_Event e; while(SDL_PollEvent(&e)) if(e.type == SDL_QUIT) { quitApp = true; cancelled = true; }
    u64 d = padDown();
    if(d & HidNpadButton_B) cancelled = true;
    double now = nowSec();
    if(force || now - lastBusyDraw > 0.1) { lastBusyDraw = now; drawBusy(); }
    return !cancelled;
}
bool tgProgress(double now, double total) { busyFrac = total > 0 ? now / total : -1; return busyTick(); }
bool svPump(const std::string& s) { if(!s.empty()) busyDetail = s; return busyTick(); }
void busyStart(const std::string& title, const std::string& line) {
    busyTitle = title; busyLine = line; busyDetail = ""; busyFrac = -1; cancelled = false;
    busyTick(true);
}

// ---------------- indice en la nube ----------------
std::string serialize(const std::vector<Entry>& v) {
    std::string s;
    for(const Entry& e : v) {
        s += "SSV1\t" + sv::tidHex(e.tid) + "\t" + sv::clean(e.name) + "\t" + sv::clean(e.nick) + "\t" + sv::clean(e.console) +
             "\t" + std::to_string(e.ts) + "\t" + std::to_string((unsigned long long)e.size) + "\t";
        for(size_t i = 0; i < e.ids.size(); i++) { if(i) s += ","; s += e.ids[i]; }
        s += "\n";
    }
    return s;
}
std::vector<Entry> parse(const std::string& text) {
    std::vector<Entry> v;
    for(const std::string& line : split(text, '\n')) {
        auto p = split(line, '\t');
        if(p.size() < 8 || p[0] != "SSV1") continue;
        Entry e;
        e.tid = std::strtoull(p[1].c_str(), nullptr, 16);
        e.name = p[2]; e.nick = p[3]; e.console = p[4];
        e.ts = std::atoll(p[5].c_str()); e.size = std::strtoull(p[6].c_str(), nullptr, 10);
        e.ids = split(p[7], ',');
        if(e.ids.empty() || e.ids[0].empty()) continue;
        v.push_back(e);
    }
    return v;
}
void normalize(std::vector<Entry>& v) {
    std::sort(v.begin(), v.end(), [](const Entry& a, const Entry& b) { return a.ts > b.ts; });
    std::vector<Entry> out;
    std::map<std::string, int> count;
    for(const Entry& e : v) {
        bool dup = false;
        for(const Entry& o : out) if(o.tid == e.tid && o.ts == e.ts && o.nick == e.nick && o.console == e.console) { dup = true; break; }
        if(dup) continue;
        std::string k = sv::tidHex(e.tid) + "|" + e.nick + "|" + e.console;
        if(++count[k] > KEEP_PER_SAVE) continue;
        out.push_back(e);
    }
    v = out;
}

bool cloudReady() { return !tg::token.empty() && !tg::chatId.empty(); }

bool refreshIndex() {
    busyStart("SINCRONIZANDO", "DESCARGANDO INDICE DE LA NUBE...");
    std::string text;
    if(!tg::getIndex(text)) { statusText = "ERROR INDICE: " + tg::lastError; return false; }
    cloud = parse(text); normalize(cloud);
    selCloud = scrCloud = 0;
    return true;
}

bool ensureCloud() {
    if(tg::token.empty()) { statusText = "FALTA EL TOKEN: EDITA sdmc:/switch/SwitchSaveSync/config.ini"; return false; }
    if(tg::chatId.empty()) {
        busyStart("CONFIGURANDO", "BUSCANDO TU CHAT DE TELEGRAM...");
        if(!tg::discoverChat()) { statusText = "CHAT: " + tg::lastError; return false; }
        saveConfig();
    }
    return true;
}

// ---------------- estado de cada partida local ----------------
bool unsynced(const sv::LocalSave& s) {
    auto it = synced.find(keyOf(s));
    return it == synced.end() || it->second.sig != s.sig;
}
bool cloudNewer(const sv::LocalSave& s) {
    auto it = synced.find(keyOf(s));
    long long last = it == synced.end() ? 0 : it->second.ts;
    for(const Entry& e : cloud) if(e.tid == s.tid && e.ts > last && e.console != consoleName) return true;
    return false;
}

// ---------------- subir ----------------
bool uploadSave(size_t i, Entry& e) {
    sv::LocalSave& s = saves[i];
    busyStart("SUBIENDO: " + trimName(s.name, 34), "CREANDO COPIA...");
    std::string path = BACKUP_DIR + "/upload.tar.gz", err;
    if(!sv::mount(s)) { statusText = "NO SE PUDO ABRIR LA PARTIDA DE " + trimName(s.name, 30); return false; }
    u64 sz = 0;
    bool ok = sv::makeArchive(path, sz, err);
    sv::unmount();
    if(!ok) { statusText = err; return false; }

    FILE* f = std::fopen(path.c_str(), "rb");
    if(!f) { statusText = "NO SE PUDO LEER LA COPIA"; return false; }
    long long ts = (long long)time(nullptr);
    u64 parts = (sz + tg::CHUNK - 1) / tg::CHUNK;
    if(parts == 0) parts = 1;
    e = Entry(); e.tid = s.tid; e.name = s.name; e.nick = s.nick; e.console = consoleName; e.ts = ts; e.size = sz;
    for(u64 p = 0; p < parts; p++) {
        u64 base = p * tg::CHUNK, len = std::min<u64>(tg::CHUNK, sz - base);
        busyLine = "SUBIENDO PARTE " + std::to_string(p + 1) + " DE " + std::to_string(parts) + " (" + sizeStr(sz) + ")";
        busyDetail = ""; busyFrac = 0;
        std::string fid, mid;
        std::string fname = sv::tidHex(s.tid) + "_" + std::to_string(ts) + "_p" + std::to_string(p + 1) + "of" + std::to_string(parts) + ".bin";
        std::string cap = s.name + " | " + s.nick + " | " + consoleName + " | " + dateStr(ts);
        if(!tg::sendDocument(f, base, len, fname, cap, fid, mid)) {
            std::fclose(f); std::remove(path.c_str());
            statusText = cancelled ? "SUBIDA CANCELADA" : "ERROR AL SUBIR: " + tg::lastError;
            return false;
        }
        e.ids.push_back(fid);
    }
    std::fclose(f);
    std::remove(path.c_str());
    return true;
}

// Fusiona las nuevas entradas con el indice de la nube y lo vuelve a anclar.
bool commitEntries(const std::vector<Entry>& add, const std::vector<size_t>& idx) {
    busyStart("SINCRONIZANDO", "ACTUALIZANDO EL INDICE...");
    std::string text;
    if(!tg::getIndex(text)) { statusText = "ERROR INDICE: " + tg::lastError; return false; }
    std::vector<Entry> all = parse(text);
    for(const Entry& e : add) all.push_back(e);
    normalize(all);
    FILE* f = std::fopen(INDEX_PATH.c_str(), "wb");
    if(!f) { statusText = "NO SE PUDO ESCRIBIR EL INDICE"; return false; }
    std::string out = serialize(all);
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    if(!tg::putIndex(INDEX_PATH)) { statusText = "ERROR AL SUBIR EL INDICE: " + tg::lastError; return false; }
    cloud = all;
    for(size_t k = 0; k < add.size(); k++) {
        SyncRec r; r.ts = add[k].ts; r.sig = saves[idx[k]].sig;
        synced[keyOf(saves[idx[k]])] = r;
    }
    saveState();
    return true;
}

void uploadOne(size_t i) {
    if(!ensureCloud()) return;
    Entry e;
    if(!uploadSave(i, e)) return;
    if(commitEntries({e}, {i})) statusText = "SUBIDO: " + trimName(saves[i].name, 40);
}

void uploadAllPending() {
    if(!ensureCloud()) return;
    std::vector<Entry> add; std::vector<size_t> idx;
    int skipped = 0;
    for(size_t i = 0; i < saves.size(); i++) {
        if(!unsynced(saves[i])) continue;
        if(cloudNewer(saves[i])) { skipped++; continue; }
        Entry e;
        if(!uploadSave(i, e)) break;
        add.push_back(e); idx.push_back(i);
    }
    if(add.empty()) { if(statusText == "LISTO" || skipped) statusText = skipped ? "NADA SUBIDO: HAY CONFLICTOS (VER LA NUBE)" : "NO HAY PARTIDAS PENDIENTES"; return; }
    if(commitEntries(add, idx)) statusText = "SUBIDAS " + std::to_string(add.size()) + " PARTIDAS" + (skipped ? " (" + std::to_string(skipped) + " EN CONFLICTO OMITIDAS)" : "");
}

// ---------------- restaurar ----------------
void restore(const Entry& e, size_t localIdx) {
    sv::LocalSave& s = saves[localIdx];
    std::string dl = BACKUP_DIR + "/restore.tar.gz", err;
    busyStart("RESTAURANDO: " + trimName(e.name, 32), "DESCARGANDO...");
    FILE* f = std::fopen(dl.c_str(), "wb");
    if(!f) { statusText = "NO SE PUDO CREAR EL ARCHIVO TEMPORAL"; return; }
    for(size_t p = 0; p < e.ids.size(); p++) {
        busyLine = "DESCARGANDO PARTE " + std::to_string(p + 1) + " DE " + std::to_string(e.ids.size());
        busyFrac = 0;
        if(!tg::download(e.ids[p], nullptr, f)) {
            std::fclose(f); std::remove(dl.c_str());
            statusText = cancelled ? "RESTAURACION CANCELADA (NO SE HA TOCADO LA PARTIDA)" : "ERROR AL DESCARGAR: " + tg::lastError;
            return;
        }
    }
    std::fclose(f);
    struct stat st; std::memset(&st, 0, sizeof(st)); stat(dl.c_str(), &st);
    if((u64)st.st_size != e.size) {
        std::remove(dl.c_str());
        statusText = "DESCARGA INCOMPLETA (TAMANO DISTINTO). NO SE HA TOCADO LA PARTIDA";
        return;
    }

    // Copia de seguridad de la partida actual antes de sobrescribir.
    busyLine = "COPIA DE SEGURIDAD DE LA PARTIDA ACTUAL..."; busyFrac = -1; busyDetail = "";
    cancelled = false;
    if(!sv::mount(s)) { std::remove(dl.c_str()); statusText = "NO SE PUDO ABRIR LA PARTIDA LOCAL"; return; }
    std::string pre = BACKUP_DIR + "/antes_" + sv::tidHex(s.tid) + "_" + std::to_string((long long)time(nullptr)) + ".tar.gz";
    u64 psz = 0;
    if(!sv::makeArchive(pre, psz, err)) {
        sv::unmount(); std::remove(dl.c_str());
        statusText = "NO SE PUDO HACER LA COPIA PREVIA: " + err + ". NO SE HA TOCADO NADA";
        return;
    }

    // A partir de aqui no se puede cancelar.
    busyLine = "ESCRIBIENDO LA PARTIDA..."; busyDetail = "";
    cancelled = false;
    bool ok = sv::extractArchive(dl, err);
    if(ok) ok = sv::commit();
    if(!ok && err.empty()) err = "NO SE PUDO CONFIRMAR LA ESCRITURA";
    u64 total = 0; std::string sig;
    if(ok) sv::signature(total, sig);
    sv::unmount();
    std::remove(dl.c_str());
    if(!ok) {
        statusText = "ERROR: " + err + ". COPIA PREVIA EN backups/" + trimName(pre.substr(pre.rfind('/') + 1), 40);
        return;
    }
    s.sig = sig; s.size = total;
    SyncRec r; r.ts = e.ts; r.sig = sig;
    synced[keyOf(s)] = r;
    saveState();
    statusText = "RESTAURADO: " + trimName(e.name, 30) + " (" + dateStr(e.ts) + " UTC). COPIA PREVIA GUARDADA EN backups/";
}

// ---------------- pantallas ----------------
void drawHeader(const std::string& right) {
    fill(0,0,W,H,C.bg);
    fill(0,0,W,78,C.panel);
    text(34,22,"SAVE SYNC",C.text,3);
    text(520,29,trimName(consoleName,18),C.cyan,2,300);
    text(900,29,right,C.muted,2,340);
}
void drawStatus(const std::string& hint) {
    fill(28,614,1224,36,C.panel);
    text(48,626,statusText,C.muted,1,1180);
    fill(28,660,1224,36,C.panel);
    text(48,672,hint,C.muted,1,1180);
}

void clampList(int& sel, int& scr, int n) {
    if(n <= 0) { sel = scr = 0; return; }
    sel = std::max(0, std::min(sel, n - 1));
    if(sel < scr) scr = sel;
    if(sel >= scr + 12) scr = sel - 11;
}

void drawMain() {
    drawHeader(cloudReady() ? "NUBE: LISTA" : "NUBE: SIN CONFIGURAR");
    fill(28,98,1224,500,C.panel);
    text(54,112,"JUEGO",C.muted,1); text(420,112,"USUARIO",C.muted,1); text(600,112,"TAMANO",C.muted,1); text(720,112,"ESTADO",C.muted,1);
    if(saves.empty()) text(54,150,"NO SE ENCONTRARON PARTIDAS DE USUARIO",C.muted,2);
    clampList(selMain, scrMain, (int)saves.size());
    for(int i = scrMain; i < (int)saves.size() && i < scrMain + 12; i++) {
        int yy = 136 + (i - scrMain) * 36;
        const sv::LocalSave& s = saves[i];
        bool sel = (i == selMain);
        if(sel) fill(40,yy-6,1200,30,C.panel2);
        text(54,yy,trimName(s.name,34),sel?C.text:C.muted,2,360);
        text(420,yy,trimName(s.nick,12),sel?C.text:C.muted,2,170);
        text(600,yy,sizeStr(s.size),sel?C.text:C.muted,2,110);
        bool u = unsynced(s), n = cloudNewer(s);
        if(u && n) text(720,yy,"CONFLICTO",C.red,2);
        else if(n) text(720,yy,"NUBE MAS NUEVA",C.cyan,2);
        else if(u) text(720,yy,"SIN SUBIR",C.orange,2);
        else text(720,yy,"AL DIA",C.green,2);
    }
    drawStatus("A SUBIR ESTA   Y SUBIR PENDIENTES   X VER NUBE / RESTAURAR   MINUS ACTUALIZAR NUBE   PLUS SALIR");
    SDL_RenderPresent(renderer);
}

void drawCloud() {
    drawHeader(std::to_string(cloud.size()) + " COPIAS EN LA NUBE");
    fill(28,98,1224,500,C.panel);
    text(54,112,"JUEGO",C.muted,1); text(400,112,"USUARIO",C.muted,1); text(540,112,"CONSOLA",C.muted,1);
    text(760,112,"FECHA (UTC)",C.muted,1); text(1000,112,"TAMANO",C.muted,1);
    if(cloud.empty()) text(54,150,"LA NUBE ESTA VACIA. SUBE PARTIDAS DESDE LA PANTALLA PRINCIPAL",C.muted,2);
    clampList(selCloud, scrCloud, (int)cloud.size());
    for(int i = scrCloud; i < (int)cloud.size() && i < scrCloud + 12; i++) {
        int yy = 136 + (i - scrCloud) * 36;
        const Entry& e = cloud[i];
        bool sel = (i == selCloud);
        if(sel) fill(40,yy-6,1200,30,C.panel2);
        SDL_Color c = sel ? C.text : C.muted;
        text(54,yy,trimName(e.name,28),c,2,340);
        text(400,yy,trimName(e.nick,10),c,2,130);
        text(540,yy,trimName(e.console,16),e.console == consoleName ? C.cyan : c,2,210);
        text(760,yy,dateStr(e.ts),c,2,230);
        text(1000,yy,sizeStr(e.size),c,2,200);
    }
    drawStatus("A RESTAURAR   B VOLVER   MINUS ACTUALIZAR NUBE   PLUS SALIR");
    SDL_RenderPresent(renderer);
}

void drawConfirm() {
    drawHeader("CONFIRMAR");
    fill(28,98,1224,500,C.panel);
    if(confirmEntry < 0 || confirmEntry >= (int)cloud.size()) { SDL_RenderPresent(renderer); return; }
    const Entry& e = cloud[confirmEntry];
    text(54,120,"RESTAURAR PARTIDA",C.accent,3);
    text(54,175,trimName(e.name,44),C.text,3,1150);
    text(54,225,"COPIA DE: "+e.nick+" EN "+e.console+" - "+dateStr(e.ts)+" UTC - "+sizeStr(e.size),C.muted,2,1150);
    if(targets.empty()) {
        text(54,300,"ESTE JUEGO NO TIENE PARTIDA EN ESTA CONSOLA.",C.red,2);
        text(54,335,"ABRE EL JUEGO UNA VEZ, CREA UNA PARTIDA NUEVA Y GUARDA. LUEGO VUELVE AQUI.",C.muted,2,1150);
    } else {
        const sv::LocalSave& t = saves[targets[targetSel]];
        text(54,290,"SE ESCRIBIRA EN EL PERFIL:",C.muted,2);
        text(54,325,"< "+t.nick+" >",C.cyan,3,600);
        text(54,385,"SE SOBRESCRIBE LA PARTIDA ACTUAL DE ESE PERFIL.",C.orange,2,1150);
        text(54,420,"ANTES SE HARA UNA COPIA DE SEGURIDAD EN LA SD (carpeta backups).",C.muted,2,1150);
    }
    drawStatus(targets.empty() ? "B VOLVER" : "A CONFIRMAR   B CANCELAR   IZQ/DER CAMBIAR PERFIL");
    SDL_RenderPresent(renderer);
}

void openConfirm() {
    if(cloud.empty()) return;
    confirmEntry = selCloud;
    const Entry& e = cloud[confirmEntry];
    targets.clear(); targetSel = 0;
    for(size_t i = 0; i < saves.size(); i++) if(saves[i].tid == e.tid) targets.push_back((int)i);
    for(size_t k = 0; k < targets.size(); k++) if(saves[targets[k]].nick == e.nick) { targetSel = (int)k; break; }
    screen = S_CONFIRM;
}

void analyzeSaves() {
    for(size_t i = 0; i < saves.size(); i++) {
        busyStart("ANALIZANDO PARTIDAS", trimName(saves[i].name, 50));
        busyFrac = saves.empty() ? -1 : (double)i / (double)saves.size();
        busyDetail = "";
        cancelled = false;
        if(sv::mount(saves[i])) {
            u64 total = 0; std::string sig;
            if(sv::signature(total, sig)) { saves[i].sig = sig; saves[i].size = total; }
            sv::unmount();
        }
        cancelled = false;
    }
}

void rescanLocal() {
    sv::pumpFn = svPump;
    saves = sv::scan();
    analyzeSaves();
}

} // namespace

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    SocketInitConfig cfg = *socketGetDefaultInitConfig();
    cfg.tcp_tx_buf_size = 0x20000; cfg.tcp_rx_buf_size = 0x80000;
    cfg.tcp_tx_buf_max_size = 0x40000; cfg.tcp_rx_buf_max_size = 0x100000;
    bool netOk = R_SUCCEEDED(socketInitialize(&cfg));
    if(netOk) { int probe = socket(AF_INET, SOCK_STREAM, 0); if(probe < 0) { socketExit(); netOk = false; } else close(probe); }
    if(!netOk) netOk = R_SUCCEEDED(socketInitializeDefault());
    if(!initUI("Save Sync")) { if(netOk) socketExit(); return 1; }
    if(netOk) curl_global_init(CURL_GLOBAL_DEFAULT);
    sv::initServices();
    tg::progressFn = tgProgress;
    sv::pumpFn = svPump;

    makeDirs();
    loadConfig();
    loadState();
    if(tg::token.empty()) saveConfig();   // deja la plantilla lista para editar
    rescanLocal();
    if(!netOk) statusText = "NO SE PUDO INICIAR LA RED";
    else if(tg::token.empty()) statusText = "FALTA EL TOKEN: EDITA sdmc:/switch/SwitchSaveSync/config.ini EN EL PC";
    else if(ensureCloud() && refreshIndex()) statusText = "LISTO. " + std::to_string(cloud.size()) + " COPIAS EN LA NUBE";

    while(!quitApp && appletMainLoop()) {
        SDL_Event ev; while(SDL_PollEvent(&ev)) if(ev.type == SDL_QUIT) quitApp = true;
        u64 d = padDown();
        if(d & HidNpadButton_Plus) quitApp = true;

        if(screen == S_MAIN) {
            if(d & HidNpadButton_Down) selMain++;
            if(d & HidNpadButton_Up) selMain--;
            if(d & HidNpadButton_R) selMain += 8;
            if(d & HidNpadButton_L) selMain -= 8;
            if((d & HidNpadButton_A) && netOk && !saves.empty()) uploadOne((size_t)selMain);
            if((d & HidNpadButton_Y) && netOk) uploadAllPending();
            if((d & HidNpadButton_X) && netOk) { if(ensureCloud() && refreshIndex()) screen = S_CLOUD; }
            if((d & HidNpadButton_Minus) && netOk) { if(ensureCloud() && refreshIndex()) statusText = "NUBE ACTUALIZADA: " + std::to_string(cloud.size()) + " COPIAS"; }
            drawMain();
        } else if(screen == S_CLOUD) {
            if(d & HidNpadButton_Down) selCloud++;
            if(d & HidNpadButton_Up) selCloud--;
            if(d & HidNpadButton_R) selCloud += 8;
            if(d & HidNpadButton_L) selCloud -= 8;
            if(d & HidNpadButton_B) screen = S_MAIN;
            if((d & HidNpadButton_Minus) && netOk) refreshIndex();
            if(d & HidNpadButton_A) openConfirm();
            drawCloud();
        } else {
            if(d & HidNpadButton_B) screen = S_CLOUD;
            if(!targets.empty()) {
                int n = (int)targets.size();
                if(d & HidNpadButton_Left) targetSel = (targetSel + n - 1) % n;
                if(d & HidNpadButton_Right) targetSel = (targetSel + 1) % n;
                if(d & HidNpadButton_A) {
                    Entry e = cloud[confirmEntry];
                    restore(e, (size_t)targets[targetSel]);
                    screen = S_MAIN;
                }
            }
            drawConfirm();
        }
        SDL_Delay(16);
    }

    sv::exitServices();
    shutdownUI();
    if(netOk) { curl_global_cleanup(); socketExit(); }
    return 0;
}
