// Inspect the actual generated Aeolus wave sizes; GPL-3.0-or-later.
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#define private public
#include "rankwave.h"
#undef private
int main(int argc, char **argv) {
 if(argc != 3) return 1;
 Addsynth stop; if(strlen(argv[2]) >= sizeof stop._filename) return 1;
 strcpy(stop._filename,argv[2]); if(stop.load(argv[1])) return 1;
 float scale[12]; for(int i=0;i<12;i++) scale[i]=powf(2.0f,i/12.0f);
 Rankwave rank(stop._n0,stop._n1); rank.gen_waves(&stop,44100,440,scale);
 size_t total=0, attacks=0, loops=0;
 for(int n=48;n<=84;n++) { if(n<stop._n0 || n>stop._n1) continue;
  Pipewave &p=rank._pipes[n-stop._n0]; attacks+=p._l0; loops+=p._l1; total+=p._l0+p._l1;
 }
 printf("%s: 37 notes, %zu samples (attack %zu, loop %zu), 16-bit %zu bytes\n",argv[2],total,attacks,loops,total*2);
}
