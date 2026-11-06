#pragma once
#include <switch.h>
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr int W = 1280, H = 720;
struct Palette { SDL_Color bg, panel, panel2, text, muted, accent, cyan, green, orange, red, border; };
const Palette C = {
    {12,14,24,255},{22,26,42,255},{30,36,56,255},{239,242,250,255},
    {145,155,178,255},{151,110,255,255},{63,190,235,255},{65,205,150,255},
    {255,174,82,255},{244,92,112,255},{53,61,86,255}
};
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
void color(SDL_Color c) { SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a); }
void fill(int x,int y,int w,int h, SDL_Color c) {
    color(c); SDL_Rect r{x,y,w,h}; SDL_RenderFillRect(renderer,&r);
}
void line(int x1,int y1,int x2,int y2, SDL_Color c) {
    color(c); SDL_RenderDrawLine(renderer,x1,y1,x2,y2);
}
void rect(int x,int y,int w,int h, SDL_Color c) {
    color(c); SDL_Rect r{x,y,w,h}; SDL_RenderDrawRect(renderer,&r);
}
const uint8_t* glyph(char ch) {
    static const uint8_t blank[7]={0,0,0,0,0,0,0};
    // 5x7 bitmap font, uppercase ASCII.
    #define G(a,b,c,d,e,f,g) { static const uint8_t v[7]={a,b,c,d,e,f,g}; return v; }
    switch(ch) {
    case 'A': G(14,17,17,31,17,17,17); case 'B': G(30,17,17,30,17,17,30);
    case 'C': G(14,17,16,16,16,17,14); case 'D': G(30,17,17,17,17,17,30);
    case 'E': G(31,16,16,30,16,16,31); case 'F': G(31,16,16,30,16,16,16);
    case 'G': G(14,17,16,23,17,17,15); case 'H': G(17,17,17,31,17,17,17);
    case 'I': G(14,4,4,4,4,4,14); case 'J': G(7,2,2,2,18,18,12);
    case 'K': G(17,18,20,24,20,18,17); case 'L': G(16,16,16,16,16,16,31);
    case 'M': G(17,27,21,21,17,17,17); case 'N': G(17,25,21,19,17,17,17);
    case 'O': G(14,17,17,17,17,17,14); case 'P': G(30,17,17,30,16,16,16);
    case 'Q': G(14,17,17,17,21,18,13); case 'R': G(30,17,17,30,20,18,17);
    case 'S': G(15,16,16,14,1,1,30); case 'T': G(31,4,4,4,4,4,4);
    case 'U': G(17,17,17,17,17,17,14); case 'V': G(17,17,17,17,17,10,4);
    case 'W': G(17,17,17,21,21,21,10); case 'X': G(17,17,10,4,10,17,17);
    case 'Y': G(17,17,10,4,4,4,4); case 'Z': G(31,1,2,4,8,16,31);
    case '0': G(14,17,19,21,25,17,14); case '1': G(4,12,4,4,4,4,14);
    case '2': G(14,17,1,2,4,8,31); case '3': G(30,1,1,14,1,1,30);
    case '4': G(2,6,10,18,31,2,2); case '5': G(31,16,16,30,1,1,30);
    case '6': G(14,16,16,30,17,17,14); case '7': G(31,1,2,4,8,8,8);
    case '8': G(14,17,17,14,17,17,14); case '9': G(14,17,17,15,1,1,14);
    case ':': G(0,4,4,0,4,4,0); case '.': G(0,0,0,0,0,12,12);
    case ',': G(0,0,0,0,4,4,8); case '-': G(0,0,0,31,0,0,0);
    case '_': G(0,0,0,0,0,0,31); case '/': G(1,2,2,4,8,8,16);
    case '%': G(17,2,4,8,17,0,0); case '+': G(0,4,4,31,4,4,0);
    case '(': G(2,4,8,8,8,4,2); case ')': G(8,4,2,2,2,4,8);
    case '[': G(14,8,8,8,8,8,14); case ']': G(14,2,2,2,2,2,14);
    case '?': G(14,17,1,2,4,0,4); case '!': G(4,4,4,4,4,0,4);
    case ' ': return blank; case '>': G(16,8,4,2,4,8,16);
    case '<': G(1,2,4,8,4,2,1); case '=': G(0,31,0,31,0,0,0);
    case '*': G(0,21,14,31,14,21,0); case '#': G(10,31,10,10,31,10,0);
    case '|': G(4,4,4,4,4,4,4); case '\'': G(4,4,8,0,0,0,0);
    case '"': G(10,10,0,0,0,0,0); case '~': G(0,0,9,22,0,0,0);
    default: return blank;
    }
    #undef G
}
void text(int x,int y,const std::string& s, SDL_Color c, int scale=2, int maxWidth=1000) {
    color(c); int ox=x;
    for(unsigned char raw : s) {
        char ch = raw;
        if(ch >= 'a' && ch <= 'z') ch = char(ch - 32);
        if(ch=='\n') { y += 9*scale; x=ox; continue; }
        if(ch < 32 || ch > 126) ch='?';
        if(x-ox+6*scale > maxWidth) break;
        const uint8_t* bits=glyph(ch);
        for(int row=0;row<7;row++) for(int col=0;col<5;col++)
            if(bits[row] & (1 << (4-col))) {
                SDL_Rect p{x+col*scale,y+row*scale,scale,scale};
                SDL_RenderFillRect(renderer,&p);
            }
        x += 6*scale;
    }
}
std::string human(uint64_t n) {
    const char* units[]={"B","KB","MB","GB","TB"};
    double v=(double)n; int u=0;
    while(v>=1024.0 && u<4){v/=1024.0;u++;}
    char b[48]; std::snprintf(b,sizeof(b),u==0?"%.0f %s":"%.2f %s",v,units[u]);
    return b;
}
std::string trimName(const std::string& s, size_t n=26) {
    if(s.size()<=n) return s;
    return s.substr(0,n>3?n-3:0)+"...";
}

PadState pad;
bool initUI(const char* title) {
    if(SDL_Init(SDL_INIT_VIDEO)!=0) return false;
    window=SDL_CreateWindow(title,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_SHOWN);
    if(!window) return false;
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer) return false;
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    return true;
}
void shutdownUI() {
    if(renderer) SDL_DestroyRenderer(renderer);
    if(window) SDL_DestroyWindow(window);
    SDL_Quit();
}
u64 padDown() { padUpdate(&pad); return padGetButtonsDown(&pad); }
double nowSec() { return (double)armTicksToNs(armGetSystemTick())/1e9; }
void bar(int x,int y,int w,int h,double frac,SDL_Color c) {
    if(frac<0) frac=0; if(frac>1) frac=1;
    fill(x,y,w,h,C.panel2); fill(x,y,(int)(w*frac),h,c);
}
void graph(int x,int y,int w,int h,const std::vector<double>& v,SDL_Color c) {
    fill(x,y,w,h,C.panel2);
    if(v.size()<2) return;
    size_t n=std::min(v.size(),(size_t)(w/4));
    size_t first=v.size()-n;
    double mx=1e-9; for(size_t i=first;i<v.size();i++) mx=std::max(mx,v[i]);
    for(size_t i=1;i<n;i++) {
        int x1=x+(int)((i-1)*(w-4)/(n-1)), x2=x+(int)(i*(w-4)/(n-1));
        int y1=y+h-4-(int)((h-8)*v[first+i-1]/mx), y2=y+h-4-(int)((h-8)*v[first+i]/mx);
        line(x1,y1,x2,y2,c); line(x1,y1+1,x2,y2+1,c);
    }
}
std::string fix(double v,int dec=1) {
    char b[32]; std::snprintf(b,sizeof(b),"%.*f",dec,v); return b;
}
void card(int x,int y,int w,int h,const std::string& label,const std::string& value,const std::string& unit,SDL_Color c) {
    fill(x,y,w,h,C.panel);
    text(x+20,y+16,label,C.muted,2);
    text(x+20,y+52,value,c,5,w-30);
    text(x+20,y+h-30,unit,C.muted,2);
}
}
