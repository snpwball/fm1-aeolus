#include <stdint.h>
#include <stddef.h>
#include "baseline/organ_quality_16_packed.h"
size_t baseline_len(unsigned i) {
 const organ_info *a=&organ_metas[i][(organ_last[i]+organ_gaps[i]/2)/organ_gaps[i]];
 return a->offset+a->attack+a->loop;
}
int baseline_sample(unsigned i,size_t n) {
 const uint8_t *s=organ_waves[i];uint32_t block=(uint32_t)n>>4,off=organ_checks[i][block>>3];
 for(uint32_t k=0;k<(block&7u);k++)off+=2+2*s[off+1];
 int32_t base=(int8_t)s[off];uint32_t bits=s[off+1],acc=0,have=0;off+=2;
 for(uint32_t k=0;k<=((uint32_t)n&15u);k++) {
  while(have<bits){acc|=(uint32_t)s[off++]<<have;have+=8;}
  int32_t value=base+(int32_t)(acc&((1u<<bits)-1u));
  if(k==((uint32_t)n&15u))return value;
  acc>>=bits;have-=bits;
 }
 return 0;
}
