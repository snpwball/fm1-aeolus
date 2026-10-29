// Export generated Aeolus pipe attacks and loops as portable 16-bit data.
// Derived from GPL-3.0-or-later Aeolus; see vendor/aeolus-main/COPYING.
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>
#define private public
#include "rankwave.h"
#undef private
int main(int argc, char **argv) {
 if(argc!=4) { fprintf(stderr,"usage: export STOP-DIR stop.ae0 output.bin\n"); return 1; }
 Addsynth stop; if(strlen(argv[2])>=sizeof stop._filename) return 1;
 strcpy(stop._filename,argv[2]); if(stop.load(argv[1])) return 1;
 float scale[12]; for(int i=0;i<12;i++) scale[i]=powf(2.0f,i/12.0f);
 Rankwave rank(stop._n0,stop._n1); rank.gen_waves(&stop,44100,440,scale);
 float peak=0; for(int n=48;n<=84;n++) { if(n<stop._n0) return 1; Pipewave &p=rank._pipes[std::min(n,stop._n1)-stop._n0]; for(int i=0;i<p._l0+p._l1;i++) peak=std::max(peak,fabsf(p._p0[i])); }
 if(!(peak>0)) return 1; FILE *f=fopen(argv[3],"wb"); if(!f) return 1;
 // Fixed endian and fixed-width format: magic, note count, then per note attack,
 // loop, step, release block count, followed by signed little-endian samples.
 fwrite("AEP1",1,4,f); fputc(48,f); fputc(37,f); fputc(0,f); fputc(0,f);
 auto u32=[&](uint32_t x){for(int j=0;j<4;j++) fputc((x>>(j*8))&255,f);};
 for(int n=48;n<=84;n++) { Pipewave &p=rank._pipes[std::min(n,stop._n1)-stop._n0];
  u32(p._l0); u32(p._l1); u32(p._k_s); u32(p._k_r);
  for(int i=0;i<p._l0+p._l1;i++){ int16_t x=(int16_t)lrintf(p._p0[i]*30000/peak); fputc(x&255,f); fputc((uint16_t)x>>8,f); }
 }
 if(fclose(f)) return 1; printf("%s exported: peak %f\n",argv[2],peak);
}
