// Dry Aeolus reference, using its own generator and Rankwave playback.
// GPL-3.0-or-later: see vendor/aeolus-main/COPYING.
#include "addsynth.h"
#include "rankwave.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
static void u16(FILE *f,uint16_t x){fputc(x&255,f);fputc(x>>8,f);}
static void u32(FILE *f,uint32_t x){u16(f,x);u16(f,x>>16);}
int main(int argc,char**argv){
 if(argc!=4){fprintf(stderr,"usage: %s STOP_DIR OUT.wav [plenum|trumpet]\n",argv[0]);return 1;}
 const char* names[]={"I_principal_8.ae0","I_principal_4.ae0","I_octave_2.ae0","I_mixtur5fach.ae0","I_trumpet.ae0"};
 int nr=strcmp(argv[3],"trumpet")==0?5:4; float scale[12];for(int i=0;i<12;i++)scale[i]=powf(2.0f,i/12.0f);
 std::vector<Addsynth*> stops;std::vector<Rankwave*> ranks;float bus[4*PERIOD]={};
 for(int j=0;j<nr;j++){Addsynth*s=new Addsynth;strcpy(s->_filename,names[j]);if(s->load(argv[1]))return 2;
  Rankwave*r=new Rankwave(s->_n0,s->_n1);r->gen_waves(s,44100,440,scale);r->set_param(bus,0,'C');
  for(int note: {60,64,67})r->note_on(note);stops.push_back(s);ranks.push_back(r);}
 std::vector<float> pcm;pcm.reserve(44100*2);
 while(pcm.size()<44100*2){memset(bus,0,sizeof bus);for(auto*r:ranks)r->play(1);for(int i=0;i<PERIOD&&pcm.size()<44100*2;i++)pcm.push_back(bus[PERIOD+i]+bus[2*PERIOD+i]);}
 float peak=0;for(float x:pcm)peak=std::max(peak,fabsf(x));if(!(peak>0))return 3;
 FILE*f=fopen(argv[2],"wb");if(!f)return 4;u32(f,0x46464952);u32(f,36+pcm.size()*2);fwrite("WAVEfmt ",1,8,f);u32(f,16);u16(f,1);u16(f,1);u32(f,44100);u32(f,88200);u16(f,2);u16(f,16);fwrite("data",1,4,f);u32(f,pcm.size()*2);
 for(float x:pcm)u16(f,(uint16_t)(int16_t)lrintf(x/peak*26000));fclose(f);
 for(auto*r:ranks)delete r;for(auto*s:stops)delete s;return 0;
}
