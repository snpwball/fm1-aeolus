/* Offline audition of the actual compact engine; no reverb. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NVOICE 8
#define NPART 2
#include "felucca_engine.h"
enum {P_ATK,P_REL,P_E0,P_E1,P_E2,P_E3,P_E4,P_E5,P_E6,P_E7};
typedef struct track {int16_t p[10]; voice_t v[NVOICE];} track_t;
static track_t trk[NPART];
static int32_t voice_amp(int32_t s,const vmod_t *m,uint32_t i){(void)i;return (s*m->amp1)>>15;}
static const int16_t SINE[1025]={0};
#include "eng_pipe.c"
static void u16(FILE *f,uint16_t x){fputc(x&255,f);fputc(x>>8,f);}
static void u32(FILE *f,uint32_t x){u16(f,x);u16(f,x>>16);}
int main(int argc,char**argv){
 if(argc!=3)return 1;int stop=atoi(argv[1]);if(stop<0||stop>=ORGAN_STOPS+2)return 2;
 organ_init();if(stop>=ORGAN_STOPS) organ_apply_preset(stop==ORGAN_STOPS?5:6); else {memset((void*)organ_on,0,sizeof organ_on);organ_on[stop]=1;} track_t*t=&trk[stop<10||stop>=ORGAN_STOPS?0:1];vmod_t m={0};m.amp1=32767;
 for(int j=0;j<3;j++){t->v[j].note=(uint8_t)(j==0?60:j==1?64:67);pipe_note_on(t,&t->v[j]);}
 enum{FRAMES=44100*2};int32_t *buf=calloc(FRAMES,sizeof *buf);if(!buf)return 3;
 for(int pos=0;pos<FRAMES;pos+=32){unsigned n=FRAMES-pos<32?FRAMES-pos:32;for(int j=0;j<3;j++)pipe_render(t,&t->v[j],buf+pos,n,&m);}
 int32_t peak=1;for(int i=0;i<FRAMES;i++){int32_t z=buf[i]<0?-buf[i]:buf[i];if(z>peak)peak=z;}
 FILE*f=fopen(argv[2],"wb");if(!f)return 4;fwrite("RIFF",1,4,f);u32(f,36+FRAMES*2);fwrite("WAVEfmt ",1,8,f);u32(f,16);u16(f,1);u16(f,1);u32(f,44100);u32(f,88200);u16(f,2);u16(f,16);fwrite("data",1,4,f);u32(f,FRAMES*2);
 for(int i=0;i<FRAMES;i++){int16_t y=(int16_t)((int64_t)buf[i]*26000/peak);u16(f,(uint16_t)y);}fclose(f);free(buf);return 0;
}
