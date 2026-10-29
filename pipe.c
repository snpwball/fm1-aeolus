/* Portable playback of pre-generated Aeolus pipes. GPL-3.0-or-later. */
#include "pipe.h"
#include <string.h>
static uint32_t rd32(const uint8_t *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int organ_bank_load(organ_bank *b,const void *data,size_t size) {
 const uint8_t *p=(const uint8_t *)data; size_t off=8;
 if(!b || !p || size<8 || memcmp(p,"AEP1",4) || p[4]!=48 || p[5]!=37) return 0;
 organ_bank tmp;
 for(unsigned i=0;i<37;i++) {
  if(off>size || size-off<16) return 0;
  organ_pipe *v=&tmp.pipe[i]; v->attack=rd32(p+off); v->loop=rd32(p+off+4); v->step=rd32(p+off+8); v->release=rd32(p+off+12); off+=16;
  uint64_t count=(uint64_t)v->attack+v->loop;
  if(!v->loop || v->step<1 || v->step>3 || !v->release || v->release>32767 || count>(size-off)/2) return 0;
  v->samples=p+off; off+=(size_t)count*2;
 }
 *b=tmp; return 1;
}
int organ_note_on(organ_voice *v,const organ_bank *b,unsigned note) {
 if(!v || !b || note<48 || note>84) return 0;
 v->pipe=&b->pipe[note-48]; v->pos=0; v->on=1; v->remaining=0; v->release_total=0; return 1;
}
void organ_note_off(organ_voice *v) { if(v && v->on) { v->on=0; v->remaining=v->pipe->release*64; v->release_total=v->remaining; } }
void organ_render(organ_voice *v,int32_t *out,unsigned frames) {
 if(!v || !out || !v->pipe) return; const organ_pipe *p=v->pipe;
 for(unsigned i=0;i<frames;i++) {
  if(!v->on && !v->remaining) {v->pipe=0; break;}
  uint32_t at=v->pos; int16_t s=(int16_t)((uint16_t)p->samples[at*2]|((uint16_t)p->samples[at*2+1]<<8));
  if(!v->on) {s=(int16_t)((int32_t)s*(int32_t)v->remaining/(int32_t)v->release_total); --v->remaining;}
  out[i]+=s;
  v->pos+=at<p->attack?1:p->step;
  if(v->pos>=p->attack+p->loop) v->pos=p->attack+(v->pos-p->attack)%p->loop;
 }
}
