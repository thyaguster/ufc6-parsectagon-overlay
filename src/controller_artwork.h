#ifndef UFC6_CONTROLLER_ARTWORK_H
#define UFC6_CONTROLLER_ARTWORK_H
/* Exact authored vector geometry; embedded so installation needs no loose image files. */
typedef struct {unsigned char op;int xy[6];} ControllerArtCommand;
typedef struct {const ControllerArtCommand *commands;unsigned count;unsigned rgb;} ControllerArtPath;
typedef struct {const ControllerArtPath *paths;unsigned count;int left,top,right,bottom;} ControllerArtAsset;
#include "controller_artwork_data.h"
static int controller_keyboard_keycap(HDC dc,int x,int y,int width,int height,uint32_t bit){
    const wchar_t *label=bit==0?L"WASD":bit==0x400?L"Q":bit==0x800?L"E":bit==8?L"F10":
        bit==0x10?L"↑":bit==0x40?L"↓":bit==0x80?L"←":bit==0x20?L"→":
        bit==0x2000?L"Esc":bit==0x4000?L"Ent":bit==2?L"LS":bit==4?L"RS":
        bit==0x8000?L"X":bit==0x1000?L"Y":NULL;
    if(!label)return 0;
    int saved=SaveDC(dc);if(!saved)return 0;
    HPEN pen=CreatePen(PS_SOLID,1,RGB(157,174,195));
    HBRUSH brush=CreateSolidBrush(RGB(30,38,50));
    HFONT font=CreateFontW(-(bit==0?9:height<23?10:12),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    if(!pen||!brush||!font){if(pen)DeleteObject(pen);if(brush)DeleteObject(brush);if(font)DeleteObject(font);RestoreDC(dc,saved);return 0;}
    SelectObject(dc,pen);SelectObject(dc,brush);SelectObject(dc,font);
    RoundRect(dc,x+1,y+1,x+width-1,y+height-1,5,5);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(235,241,249));
    RECT box={x+2,y+1,x+width-2,y+height-1};DrawTextW(dc,label,-1,&box,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    RestoreDC(dc,saved);DeleteObject(font);DeleteObject(brush);DeleteObject(pen);return 1;
}
static int controller_artwork_draw(HDC dc,int x,int y,int width,int height,uint32_t bit,int ps){
    if(ps==2)return controller_keyboard_keycap(dc,x,y,width,height,bit);
    static const uint32_t bits[]={2,4,0x400,0x800,8,0x10,0x40,0x80,0x20,0x1000,0x2000,0x4000,0x8000,0};
    unsigned index=0;for(;index<14&&bits[index]!=bit;++index){}if(index==14)return 0;
    const ControllerArtAsset *asset=controller_art_assets[ps!=0][index];
    FLOAT sx=(FLOAT)(width-2)/(asset->right-asset->left),sy=(FLOAT)(height-2)/(asset->bottom-asset->top),scale=sx<sy?sx:sy;
    XFORM xf={scale,0,0,scale,x+(width-(asset->right-asset->left)*scale)/2-asset->left*scale,y+(height-(asset->bottom-asset->top)*scale)/2-asset->top*scale};
    int saved=SaveDC(dc);if(!saved)return 0;
    if(!SetGraphicsMode(dc,GM_ADVANCED)||!SetWorldTransform(dc,&xf)){RestoreDC(dc,saved);return 0;}
    SetPolyFillMode(dc,WINDING);int ok=1;
    for(unsigned p=0;p<asset->count&&ok;++p){const ControllerArtPath *path=&asset->paths[p];ok=BeginPath(dc)!=0;
        for(unsigned i=0;i<path->count&&ok;++i){const ControllerArtCommand *c=&path->commands[i];
            if(c->op==0)ok=MoveToEx(dc,c->xy[0],c->xy[1],NULL)!=0;
            else if(c->op==1)ok=LineTo(dc,c->xy[0],c->xy[1])!=0;
            else if(c->op==2){POINT points[]={{c->xy[0],c->xy[1]},{c->xy[2],c->xy[3]},{c->xy[4],c->xy[5]}};ok=PolyBezierTo(dc,points,3)!=0;}
            else ok=CloseFigure(dc)!=0;
        }
        if(!ok){AbortPath(dc);break;}ok=EndPath(dc)!=0;
        COLORREF color=RGB((path->rgb>>16)&255,(path->rgb>>8)&255,path->rgb&255);
        if(path->rgb==0xffffffu){
            color=RGB(232,238,247);
            if(bit==0x4000)color=ps?RGB(134,192,255):RGB(139,218,100);
            else if(bit==0x2000)color=ps?RGB(244,135,148):RGB(242,105,103);
            else if(bit==0x8000)color=ps?RGB(225,155,227):RGB(111,180,255);
            else if(bit==0x1000)color=ps?RGB(137,221,180):RGB(242,214,104);
        }
        HBRUSH brush=CreateSolidBrush(color);if(!brush){AbortPath(dc);ok=0;break;}HGDIOBJ previous=SelectObject(dc,brush);if(ok)ok=FillPath(dc)!=0;SelectObject(dc,previous);DeleteObject(brush);
    }
    RestoreDC(dc,saved);return ok;
}
#endif
