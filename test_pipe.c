#include "pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
 if(argc!=2) return 1; FILE *f=fopen(argv[1],"rb"); if(!f) return 2;
 fseek(f,0,SEEK_END); long sz=ftell(f); rewind(f); if(sz<=0) return 3;
 unsigned char *buf=malloc((size_t)sz); if(!buf || fread(buf,1,(size_t)sz,f)!=(size_t)sz) return 4; fclose(f);
 organ_bank bank; if(organ_bank_load(&bank,buf,(size_t)sz-1)) return 5;
 if(!organ_bank_load(&bank,buf,(size_t)sz)) return 6;
 organ_voice v={0}; if(organ_note_on(&v,&bank,47) || !organ_note_on(&v,&bank,60)) return 7;
 int32_t out[32768]={0}; organ_render(&v,out,32768); int nonzero=0; for(int i=0;i<32768;i++) if(out[i]) nonzero=1; if(!nonzero) return 8;
 organ_note_off(&v); for(int i=0;i<2000 && v.pipe;i++) organ_render(&v,out,64); if(v.pipe) return 9;
 free(buf); puts("Aeolus pipe bank: bounds, note, sound and release passed"); return 0;
}
