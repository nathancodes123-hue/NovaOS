#include "../include/nova/types.h"
#define WIDTH 320
#define HEIGHT 200
#define MAX_WINDOWS 16
typedef struct{int x,y,w,h;u8 visible,active;u32 color;const char*title;} Window;
static u8*fb=(u8*)0xA0000;static Window windows[MAX_WINDOWS];static u32 count;
static void pixel(int x,int y,u8 c){if(x>=0&&x<WIDTH&&y>=0&&y<HEIGHT)fb[y*WIDTH+x]=c;}
static void fill(int x,int y,int w,int h,u8 c){for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)pixel(x+xx,y+yy,c);}
static void border(int x,int y,int w,int h,u8 c){for(int i=0;i<w;i++){pixel(x+i,y,c);pixel(x+i,y+h-1,c);}for(int i=0;i<h;i++){pixel(x,y+i,c);pixel(x+w-1,y+i,c);}}
static void glyph(int x,int y,char c,u8 col){static const u8 font[96][5]={{0}};if(c<32||c>127)return;for(int i=0;i<5;i++)for(int j=0;j<7;j++)if(font[(int)c-32][i]&(1<<j))pixel(x+i,y+j,col);}
static void label(int x,int y,const char*s,u8 c){while(*s){glyph(x,y,*s++,c);x+=6;}}
static void window(Window*w){fill(w->x,w->y,w->w,w->h,7);fill(w->x,w->y,w->w,16,1);border(w->x,w->y,w->w,w->h,15);label(w->x+6,w->y+5,w->title,15);}
void gui_open(int x,int y,int w,int h,const char*t){if(count<MAX_WINDOWS)windows[count++]=(Window){x,y,w,h,1,1,7,t};}
void gui_init(void){count=0;fill(0,0,WIDTH,HEIGHT,3);fill(0,180,WIDTH,20,1);gui_open(30,25,260,145,"Nova Desktop");}
void gui_render(void){fill(0,0,WIDTH,180,3);for(u32 i=0;i<count;i++)if(windows[i].visible)window(&windows[i]);fill(0,180,WIDTH,20,1);}
