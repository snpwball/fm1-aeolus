#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
static uint8_t waves[16][65536];
static size_t lengths[16];
size_t baseline_len(unsigned i) {
 if(!lengths[i]) {
  char name[64];snprintf(name,sizeof name,"stops/%02u.bin",i);
  FILE *f=fopen(name,"rb");if(!f)return 0;
  lengths[i]=fread(waves[i],1,sizeof waves[i],f);fclose(f);
 }
 return lengths[i];
}
int baseline_sample(unsigned i,size_t n) {(void)baseline_len(i);return (int8_t)waves[i][n];}
