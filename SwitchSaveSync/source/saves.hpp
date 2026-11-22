// Acceso a los datos de guardado (usuarios), copia en tar.gz y restauracion.
#pragma once
#include <switch.h>
#include <zlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <vector>
#include "lang.hpp"

namespace sv {

bool (*pumpFn)(const std::string&) = nullptr;   // devuelve false si hay que cancelar
bool pump(const std::string& s) { return pumpFn ? pumpFn(s) : true; }

struct LocalSave {
    u64 tid = 0;
    AccountUid uid{};
    std::string name, nick, sig;
    u64 size = 0;
};

const char* MOUNT = "save";
const std::string ROOT = "save:/";

std::string tidHex(u64 t) { char b[24]; std::snprintf(b, sizeof(b), "%016llX", (unsigned long long)t); return b; }
std::string uidHex(const AccountUid& u) {
    char b[40]; std::snprintf(b, sizeof(b), "%016llX%016llX", (unsigned long long)u.uid[0], (unsigned long long)u.uid[1]);
    return b;
}
bool sameUid(const AccountUid& a, const AccountUid& b) { return a.uid[0] == b.uid[0] && a.uid[1] == b.uid[1]; }
std::string clean(std::string s) {
    for(char& c : s) if(c == '\t' || c == '\n' || c == '\r' || c == '|') c = ' ';
    return s;
}

void initServices() { nsInitialize(); accountInitialize(AccountServiceType_Application); }
void exitServices() { accountExit(); nsExit(); }

std::string titleName(u64 tid) {
    std::string name;
    NsApplicationControlData* d = (NsApplicationControlData*)std::malloc(sizeof(NsApplicationControlData));
    if(d) {
        u64 out = 0;
        if(R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage, tid, d, sizeof(NsApplicationControlData), &out))) {
            NacpLanguageEntry* le = nullptr;
            if(R_SUCCEEDED(nacpGetLanguageEntry(&d->nacp, &le)) && le && le->name[0]) name = le->name;
        }
        std::free(d);
    }
    return name.empty() ? tidHex(tid) : clean(name);
}

std::string nickOf(const AccountUid& uid) {
    std::string nick;
    AccountProfile prof; AccountProfileBase base; AccountUserData ud;
    std::memset(&base, 0, sizeof(base));
    if(R_SUCCEEDED(accountGetProfile(&prof, uid))) {
        if(R_SUCCEEDED(accountProfileGet(&prof, &ud, &base))) nick = base.nickname;
        accountProfileClose(&prof);
    }
    return nick.empty() ? T("USUARIO","USER") : clean(nick);
}

// Lista los datos de guardado de usuario instalados en la consola.
std::vector<LocalSave> scan() {
    std::vector<LocalSave> v;
    FsSaveDataInfoReader rd;
    if(R_FAILED(fsOpenSaveDataInfoReader(&rd, FsSaveDataSpaceId_User))) return v;
    FsSaveDataInfo info; s64 total = 0;
    std::map<u64, std::string> names;
    std::map<std::string, std::string> nicks;
    while(R_SUCCEEDED(fsSaveDataInfoReaderRead(&rd, &info, 1, &total)) && total > 0) {
        if(info.save_data_type != FsSaveDataType_Account) continue;
        LocalSave s;
        s.tid = info.application_id;
        s.uid = info.uid;
        if(s.tid == 0 || (s.uid.uid[0] == 0 && s.uid.uid[1] == 0)) continue;
        if(!names.count(s.tid)) names[s.tid] = titleName(s.tid);
        std::string uk = uidHex(s.uid);
        if(!nicks.count(uk)) nicks[uk] = nickOf(s.uid);
        s.name = names[s.tid]; s.nick = nicks[uk];
        v.push_back(s);
    }
    fsSaveDataInfoReaderClose(&rd);
    std::sort(v.begin(), v.end(), [](const LocalSave& a, const LocalSave& b) {
        if(a.name != b.name) return a.name < b.name;
        return a.nick < b.nick;
    });
    return v;
}

bool mount(const LocalSave& s) {
    FsSaveDataAttribute attr;
    std::memset(&attr, 0, sizeof(attr));
    attr.application_id = s.tid;
    attr.uid = s.uid;
    attr.save_data_type = FsSaveDataType_Account;
    FsFileSystem fs;
    if(R_FAILED(fsOpenSaveDataFileSystem(&fs, FsSaveDataSpaceId_User, &attr))) return false;
    if(fsdevMountDevice(MOUNT, fs) < 0) { fsFsClose(&fs); return false; }
    return true;
}
void unmount() { fsdevUnmountDevice(MOUNT); }
bool commit() { return fsdevCommitDevice(MOUNT) >= 0; }

// ---------- utilidades de directorios ----------
struct Item { std::string name; bool dir; u64 size; long long mtime; };

bool listDir(const std::string& dir, std::vector<Item>& out) {
    out.clear();
    DIR* d = opendir(dir.c_str());
    if(!d) return false;
    dirent* e;
    while((e = readdir(d)) != nullptr) {
        if(!std::strcmp(e->d_name, ".") || !std::strcmp(e->d_name, "..")) continue;
        struct stat st; std::memset(&st, 0, sizeof(st));
        if(stat((dir + e->d_name).c_str(), &st) != 0) continue;
        Item it; it.name = e->d_name; it.dir = S_ISDIR(st.st_mode);
        it.size = it.dir ? 0 : (u64)st.st_size; it.mtime = (long long)st.st_mtime;
        out.push_back(it);
    }
    closedir(d);
    std::sort(out.begin(), out.end(), [](const Item& a, const Item& b) { return a.name < b.name; });
    return true;
}

unsigned char ioBuf[65536];

// CRC de todo el contenido (nombres + datos) para saber si la partida cambio.
bool crcWalk(const std::string& rel, uint32_t& crc, u64& total) {
    std::vector<Item> items;
    if(!listDir(ROOT + rel, items)) return true;
    for(const Item& it : items) {
        std::string r = rel + it.name;
        crc = (uint32_t)crc32(crc, (const Bytef*)r.data(), (uInt)r.size());
        if(it.dir) { if(!crcWalk(r + "/", crc, total)) return false; }
        else {
            FILE* f = std::fopen((ROOT + r).c_str(), "rb");
            if(f) {
                size_t n;
                while((n = std::fread(ioBuf, 1, sizeof(ioBuf), f)) > 0) {
                    crc = (uint32_t)crc32(crc, ioBuf, (uInt)n); total += n;
                }
                std::fclose(f);
            }
        }
        if(!pump("")) return false;
    }
    return true;
}

// Con la partida montada: calcula tamano total y firma.
bool signature(u64& total, std::string& sig) {
    uint32_t crc = (uint32_t)crc32(0L, Z_NULL, 0);
    total = 0;
    if(!crcWalk("", crc, total)) return false;
    char b[32]; std::snprintf(b, sizeof(b), "%08x-%llu", crc, (unsigned long long)total);
    sig = b;
    return true;
}

// ---------- tar ----------
void putOct(char* dst, size_t len, unsigned long long v) { std::snprintf(dst, len, "%0*llo", (int)(len - 1), v); }

bool tarHeader(char* h, std::string name, u64 size, bool dir, long long mtime) {
    std::memset(h, 0, 512);
    if(dir) name += "/";
    std::string prefix;
    if(name.size() > 100) {
        size_t p = std::min<size_t>(155, name.size() - 1);
        bool found = false;
        for(; p > 0; p--) {
            if(name[p] == '/' && name.size() - p - 1 <= 100 && name.size() - p - 1 > 0) { found = true; break; }
        }
        if(!found) return false;
        prefix = name.substr(0, p); name = name.substr(p + 1);
    }
    std::memcpy(h, name.data(), name.size());
    putOct(h + 100, 8, dir ? 0755 : 0644);
    putOct(h + 108, 8, 0); putOct(h + 116, 8, 0);
    putOct(h + 124, 12, size);
    putOct(h + 136, 12, mtime < 0 ? 0 : (unsigned long long)mtime);
    std::memset(h + 148, ' ', 8);
    h[156] = dir ? '5' : '0';
    std::memcpy(h + 257, "ustar", 6);
    std::memcpy(h + 263, "00", 2);
    if(!prefix.empty()) std::memcpy(h + 345, prefix.data(), prefix.size());
    unsigned sum = 0;
    for(int i = 0; i < 512; i++) sum += (unsigned char)h[i];
    std::snprintf(h + 148, 7, "%06o", sum);
    h[154] = 0; h[155] = ' ';
    return true;
}

// Escribe la cabecera de una entrada; si la ruta no cabe en ustar usa el estandar GNU ('L').
bool writeEntryHeader(gzFile gz, const std::string& r, u64 size, bool dir, long long mtime) {
    char h[512];
    if(tarHeader(h, r, size, dir, mtime)) return gzwrite(gz, h, 512) == 512;
    std::string full = dir ? r + "/" : r;
    char lh[512];
    tarHeader(lh, "././@LongLink", full.size() + 1, false, 0);
    lh[156] = 'L';
    unsigned sum = 0; std::memset(lh + 148, ' ', 8);
    for(int i = 0; i < 512; i++) sum += (unsigned char)lh[i];
    std::snprintf(lh + 148, 7, "%06o", sum); lh[154] = 0; lh[155] = ' ';
    if(gzwrite(gz, lh, 512) != 512) return false;
    std::string data = full; data.push_back('\0');
    size_t pad = (512 - (data.size() % 512)) % 512;
    data.append(pad, '\0');
    if(gzwrite(gz, data.data(), (unsigned)data.size()) != (int)data.size()) return false;
    std::string shortName = full.substr(0, 99);
    tarHeader(h, shortName, size, dir, mtime);
    return gzwrite(gz, h, 512) == 512;
}

bool addTree(gzFile gz, const std::string& rel, std::string& err, u64& done) {
    std::vector<Item> items;
    if(!listDir(ROOT + rel, items)) return true;
    for(const Item& it : items) {
        std::string r = rel + it.name;
        if(!writeEntryHeader(gz, r, it.size, it.dir, it.mtime)) { err = T("ERROR AL ESCRIBIR EL ARCHIVO (SD LLENA?)","ERROR WRITING THE FILE (SD FULL?)"); return false; }
        if(it.dir) { if(!addTree(gz, r + "/", err, done)) return false; continue; }
        FILE* f = std::fopen((ROOT + r).c_str(), "rb");
        if(!f) { err = T("NO SE PUDO LEER ","COULD NOT READ ") + r.substr(0, 40); return false; }
        u64 left = it.size;
        while(left > 0) {
            size_t want = left < sizeof(ioBuf) ? (size_t)left : sizeof(ioBuf);
            size_t n = std::fread(ioBuf, 1, want, f);
            if(n == 0) { std::fclose(f); err = T("LECTURA INCOMPLETA DE ","INCOMPLETE READ OF ") + r.substr(0, 40); return false; }
            if(gzwrite(gz, ioBuf, (unsigned)n) != (int)n) { std::fclose(f); err = T("ERROR AL ESCRIBIR EL ARCHIVO (SD LLENA?)","ERROR WRITING THE FILE (SD FULL?)"); return false; }
            left -= n; done += n;
        }
        std::fclose(f);
        size_t pad = (512 - (it.size % 512)) % 512;
        if(pad) { char z[512]; std::memset(z, 0, sizeof(z)); gzwrite(gz, z, (unsigned)pad); }
        if(!pump(r)) { err = T("CANCELADO","CANCELLED"); return false; }
    }
    return true;
}

// Con la partida montada: crea outPath (.tar.gz).
bool makeArchive(const std::string& outPath, u64& outSize, std::string& err) {
    gzFile gz = gzopen(outPath.c_str(), "wb6");
    if(!gz) { err = T("NO SE PUDO CREAR EL ARCHIVO EN LA SD","COULD NOT CREATE THE FILE ON THE SD"); return false; }
    u64 done = 0;
    bool ok = addTree(gz, "", err, done);
    if(ok) {
        char z[1024]; std::memset(z, 0, sizeof(z));
        gzwrite(gz, z, sizeof(z));
    }
    int rc = gzclose(gz);
    if(ok && rc != Z_OK) { ok = false; err = T("ERROR AL CERRAR EL ARCHIVO","ERROR CLOSING THE FILE"); }
    if(!ok) { std::remove(outPath.c_str()); return false; }
    struct stat st; std::memset(&st, 0, sizeof(st));
    stat(outPath.c_str(), &st);
    outSize = (u64)st.st_size;
    return true;
}

// ---------- restaurar ----------
bool wipeDir(const std::string& dir, bool removeSelf) {
    std::vector<Item> items;
    listDir(dir, items);
    for(const Item& it : items) {
        std::string p = dir + it.name;
        if(it.dir) wipeDir(p + "/", true);
        else std::remove(p.c_str());
    }
    if(removeSelf) { std::string d = dir; if(!d.empty() && d.back() == '/') d.pop_back(); rmdir(d.c_str()); }
    return true;
}

void mkdirs(const std::string& path) {   // crea todos los directorios de "save:/a/b/c"
    for(size_t p = ROOT.size(); p < path.size(); p++) {
        if(path[p] == '/') mkdir(path.substr(0, p).c_str(), 0777);
    }
}

bool readFull(gzFile gz, void* buf, size_t n) {
    size_t got = 0;
    while(got < n) {
        int r = gzread(gz, (char*)buf + got, (unsigned)(n - got));
        if(r <= 0) return false;
        got += (size_t)r;
    }
    return true;
}

bool safeName(const std::string& n) {
    if(n.empty() || n[0] == '/') return false;
    if(n.find("..") != std::string::npos) return false;
    return true;
}

// Con la partida montada: borra el contenido actual y extrae el .tar.gz.
bool extractArchive(const std::string& path, std::string& err) {
    gzFile gz = gzopen(path.c_str(), "rb");
    if(!gz) { err = T("NO SE PUDO ABRIR LA COPIA DESCARGADA","COULD NOT OPEN THE DOWNLOADED BACKUP"); return false; }
    wipeDir(ROOT, false);
    char h[512];
    bool ok = true;
    std::string longName;
    while(readFull(gz, h, 512)) {
        bool zero = true;
        for(int i = 0; i < 512; i++) if(h[i]) { zero = false; break; }
        if(zero) break;
        std::string name(h, strnlen(h, 100));
        std::string prefix(h + 345, strnlen(h + 345, 155));
        if(!prefix.empty()) name = prefix + "/" + name;
        u64 size = std::strtoull(std::string(h + 124, 12).c_str(), nullptr, 8);
        char type = h[156];
        size_t pad = (512 - (size % 512)) % 512;
        if(type == 'L') {   // nombre largo GNU para la siguiente entrada
            if(size == 0 || size > 4096) { err = T("COPIA CON NOMBRE NO VALIDO","BACKUP WITH AN INVALID NAME"); ok = false; break; }
            std::string data(size + pad, '\0');
            if(!readFull(gz, &data[0], size + pad)) { err = T("COPIA INCOMPLETA","INCOMPLETE BACKUP"); ok = false; break; }
            longName = data.substr(0, strnlen(data.c_str(), size));
            continue;
        }
        if(!longName.empty()) { name = longName; longName.clear(); }
        bool isDir = (type == '5');
        bool isFile = (type == '0' || type == 0);
        if((isDir || isFile) && !safeName(name)) { err = T("COPIA CON RUTAS NO VALIDAS","BACKUP WITH INVALID PATHS"); ok = false; break; }
        if(isDir) {
            while(!name.empty() && name.back() == '/') name.pop_back();
            mkdirs(ROOT + name + "/x");
            mkdir((ROOT + name).c_str(), 0777);
            continue;
        }
        FILE* f = nullptr;
        if(isFile) {
            mkdirs(ROOT + name);
            f = std::fopen((ROOT + name).c_str(), "wb");
            if(!f) { err = T("NO SE PUDO CREAR ","COULD NOT CREATE ") + name.substr(0, 40); ok = false; break; }
        }
        u64 left = size;
        while(left > 0) {
            size_t want = left < sizeof(ioBuf) ? (size_t)left : sizeof(ioBuf);
            int r = gzread(gz, ioBuf, (unsigned)want);
            if(r <= 0) { err = T("COPIA INCOMPLETA O DANADA","INCOMPLETE OR DAMAGED BACKUP"); ok = false; break; }
            if(f && std::fwrite(ioBuf, 1, (size_t)r, f) != (size_t)r) { err = T("ESCRITURA FALLIDA (SAVE LLENO?)","WRITE FAILED (SAVE FULL?)"); ok = false; break; }
            left -= (u64)r;
        }
        if(f) std::fclose(f);
        if(!ok) break;
        if(pad) { char z[512]; if(!readFull(gz, z, pad)) { err = T("COPIA INCOMPLETA","INCOMPLETE BACKUP"); ok = false; break; } }
        pump(name);
    }
    gzclose(gz);
    return ok;
}

} // namespace sv
