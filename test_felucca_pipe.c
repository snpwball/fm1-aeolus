#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ORGAN_HOST 1
#define NVOICE 8
#define NPART 2
#include "felucca_engine.h"
enum {P_ATK,P_REL,P_E0,P_E1,P_E2,P_E3,P_E4,P_E5,P_E6,P_E7};
typedef struct track {int16_t p[10]; voice_t v[NVOICE];} track_t;
static track_t trk[NPART];
static int32_t voice_amp(int32_t s,const vmod_t *m,uint32_t i) {(void)i;return (s*m->amp1)>>15;}
static const int16_t SINE[1025]={[256]=32767};
#include "eng_pipe.c"
size_t baseline_len(unsigned i);
int baseline_sample(unsigned i,size_t n);
int main(void) {
 vmod_t m={0};m.amp1=32767;organ_init();
 if(organ_velocity(127)!=127 || organ_velocity(64)>=64 || organ_velocity(1)<16) return 16;
 for(unsigned v=1;v<127;v++)if(organ_velocity(v)>organ_velocity(v+1))return 17;

 /* Compare both decoders for every packed block, including the four newer stops. */
 for(unsigned stop=0;stop<ORGAN_STOPS;stop++) {
  if((ORGAN_RAW_MASK>>stop)&1u)continue;
  const uint8_t *src=organ_waves[stop];
  pipe_voice_t *p=&pipe_voices[0][0];
  const organ_info *last=&organ_metas[stop][(organ_last[stop]+organ_gaps[stop]/2)/organ_gaps[stop]];
  uint32_t total=last->offset+last->attack+last->loop;
  for(unsigned k=0;k<2;k++)p->cache[stop][k].block=UINT32_MAX;
  for(uint32_t block=0;block<(total+15u)/16u;block++) {
   uint32_t off=organ_checks[stop][block>>3];
   for(uint32_t n=0;n<(block&7u);n++)off+=2u+2u*src[off+1u];
   int32_t base=(int8_t)src[off];uint32_t bits=src[off+1],acc=0,have=0;off+=2;
   for(uint32_t i=0;i<16u && block*16u+i<total;i++) {
    while(have<bits){acc|=(uint32_t)src[off++]<<have;have+=8u;}
    int32_t expected=base+(int32_t)(bits?acc&((1u<<bits)-1u):0u);
    if(organ_sample(p,stop,block*16u+i)!=expected){printf("decode mismatch stop %u block %u sample %u\n",stop,block,i);return 28;}
    acc>>=bits;have-=bits;
   }
  }
 }
 for(unsigned stop=0;stop<ORGAN_STOPS;stop++) {
  pipe_voice_t *p=&pipe_voices[0][0];
  for(unsigned k=0;k<2;k++)p->cache[stop][k].block=UINT32_MAX;
  for(size_t i=0;i<baseline_len(stop);i++)if(organ_sample(p,stop,(uint32_t)i)!=baseline_sample(stop,i)){printf("decode mismatch stop %u sample %zu\n",stop,i);return 10;}
 }

 for(unsigned stop=0;stop<ORGAN_STOPS;stop++){
  memset((void*)organ_on,0,sizeof organ_on);organ_on[stop]=1;
  track_t*t=&trk[(stop==10||stop==11)?1:0]; unsigned last=(stop==10||stop==11)?60:84;
  for(unsigned note=48;note<=last;note++) {
   voice_t*v=&t->v[0];v->note=note;pipe_note_on(t,v);m.pitch16=(int32_t)note*16;int32_t out[32];long long sum=0;
   for(int k=0;k<1000;k++){memset(out,0,sizeof out);pipe_render(t,v,out,32,&m);for(int i=0;i<32;i++)sum+=out[i]<0?-out[i]:out[i];}
   if(note>=organ_lo[stop]&&note<=organ_hi[stop]&&!sum){printf("silent stop %u note %u\n",stop,note);return 1;}
  }
 }
 for(unsigned i=0;i<16;i++){unsigned r=organ_slot(i/8,i%8);if(r!=i)return 2;}
 organ_init(); { voice_t *a=&trk[0].v[0], *b=&trk[0].v[1]; int32_t out[64]={0};
  a->note=b->note=60;pipe_note_on(&trk[0],a);pipe_note_on(&trk[0],b);
  m.pitch16=60*16;pipe_render(&trk[0],a,out,64,&m);
  m.pitch16=60*16+32;pipe_render(&trk[0],b,out,64,&m);
  if(pipe_voices[0][1].pos[0]<=pipe_voices[0][0].pos[0]) return 5;
  organ_bend14=8191;pipe_note_on(&trk[0],a);memset(out,0,sizeof out);m.pitch16=60*16;pipe_render(&trk[0],a,out,64,&m);
  organ_bend14=0;pipe_note_on(&trk[0],b);int32_t flat[64]={0};pipe_render(&trk[0],b,flat,64,&m);
  if(pipe_voices[0][0].pos[0]<=pipe_voices[0][1].pos[0]) return 7;
  a->vel=b->vel=80;m.amp1=4000;pipe_note_on(&trk[0],a);memset(out,0,sizeof out);pipe_render(&trk[0],a,out,64,&m);
  m.amp1=32767;pipe_note_on(&trk[0],b);memset(flat,0,sizeof flat);pipe_render(&trk[0],b,flat,64,&m);
  long loud=0,soft=0;for(int z=0;z<64;z++){loud+=flat[z]<0?-flat[z]:flat[z];soft+=out[z]<0?-out[z]:out[z];}
  if(!loud || soft*3>loud) return 8;
  a->vel=b->vel=80;organ_trem_phase=0x40000000;organ_trem_depth=127;
  pipe_note_on(&trk[0],a);memset(out,0,sizeof out);m.pitch16=60*16;pipe_render(&trk[0],a,out,64,&m);
  organ_trem_depth=0;pipe_note_on(&trk[0],b);int32_t plain[64]={0};pipe_render(&trk[0],b,plain,64,&m);
  long am=0,bm=0;for(int z=0;z<64;z++){am+=out[z]<0?-out[z]:out[z];bm+=plain[z]<0?-plain[z]:plain[z];}
  if(!am || am>=bm)return 6;
 }
 organ_init(); {
  voice_t *v=&trk[0].v[0];v->note=60;v->vel=100;pipe_note_on(&trk[0],v);
  pipe_voice_t *p=&pipe_voices[0][0];int32_t out[32]={0};m.pitch16=960;
  if(p->fade[1]!=0)return 20;
  organ_on[1]=1;pipe_render(&trk[0],v,out,32,&m);
  if(p->fade[1]!=32 || !p->pos[1])return 21;
  organ_on[1]=0;pipe_render(&trk[0],v,out,32,&m);
  if(p->fade[1]!=0)return 22;
  organ_on[1]=1;pipe_render(&trk[0],v,out,32,&m);
  if(p->fade[1]!=32 || p->pos[1] == 0)return 23;
 }
 organ_init();organ_apply_preset(5);
 if(!organ_on[0]||!organ_on[1]||!organ_on[2]||!organ_on[3]||organ_on[9]||organ_gain[2]!=150||organ_gain[3]!=170) return 3;
 organ_apply_preset(6);if(!organ_on[5]||!organ_on[6]||organ_on[9])return 4;
 organ_apply_preset(9);if(!organ_on[0]||!organ_on[1]||!organ_on[2]||!organ_on[3]||organ_on[9])return 11;
 organ_apply_preset(10);if(!organ_on[12]||organ_on[13]||organ_on[15])return 12;
 organ_apply_preset(11);if(!organ_on[13]||!organ_on[1]||!organ_on[2]||organ_on[12])return 13;
 organ_apply_preset(12);if(!organ_on[5]||!organ_on[14]||organ_on[13])return 14;
 organ_apply_preset(13);if(!organ_on[5]||!organ_on[15]||organ_on[14])return 15;
 organ_apply_preset(15);if(!organ_on[5]||!organ_on[6]||organ_on[15])return 18;
 { voice_t *a=&trk[0].v[0], *b=&trk[0].v[1];
   organ_init(); a->note=b->note=60; a->vel=20; b->vel=120;
   pipe_note_on(&trk[0],a); pipe_note_on(&trk[0],b);
   if(a->vel>=b->vel || pipe_voices[0][0].touch!=20 || pipe_voices[0][1].touch!=120)return 24;
 }
 {static const unsigned masks[]={ (1u<<5)|(1u<<12), (1u<<5)|(1u<<7)|(1u<<14), (1u<<0)|(1u<<5)|(1u<<6)|(1u<<12), (1u<<5)|(1u<<13), (1u<<0)|(1u<<1)|(1u<<2)|(1u<<4), (1u<<0)|(1u<<1)|(1u<<2)|(1u<<4), (1u<<0)|(1u<<1)|(1u<<5)|(1u<<7)};
  for(unsigned n=0;n<7;n++){organ_apply_preset(16+n);for(unsigned j=0;j<ORGAN_STOPS;j++)if(j!=10&&j!=11&&!!organ_on[j]!=!!(masks[n]&(1u<<j)))return 19;}
 }
 {voice_t *v=&trk[0].v[0]; int32_t out[32]; long long lo=0,hi=0;
  v->note=36;pipe_note_on(&trk[0],v);m.pitch16=36*16;for(int k=0;k<200;k++){memset(out,0,sizeof out);pipe_render(&trk[0],v,out,32,&m);for(int i=0;i<32;i++)lo+=out[i]<0?-out[i]:out[i];}
  v->note=96;pipe_note_on(&trk[0],v);m.pitch16=96*16;for(int k=0;k<200;k++){memset(out,0,sizeof out);pipe_render(&trk[0],v,out,32,&m);for(int i=0;i<32;i++)hi+=out[i]<0?-out[i]:out[i];}
  if(!lo||!hi)return 9;}
 organ_apply_preset(5);
 for(int j=0;j<NVOICE;j++){trk[0].v[j].note=60+j;pipe_note_on(&trk[0],&trk[0].v[j]);}
 for(int k=0;k<1000;k++){int32_t out[32]={0};for(int j=0;j<NVOICE;j++)pipe_render(&trk[0],&trk[0].v[j],out,32,&m);}
 puts("16 stops, all playable notes and 8-voice plenum passed");return 0;
}
