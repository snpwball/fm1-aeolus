#define FELUCCA_SLICE 0
#define EDITOR_TEST_MAIN original_editor_test_main
#define ORGAN_BANK_PTR(k) (host_samples[k])
#include "vendor/Felucca-smk37/tests/editor_test.c"

static uint8_t banks[2][0x14000];
static int upload(unsigned slot, unsigned from, unsigned to) {
    uint8_t args[600];
    for(unsigned off=from;off<to;off+=256) {
        args[0]=slot;args[1]=off&127;args[2]=(off>>7)&127;args[3]=(off>>14)&127;
        unsigned n=pack7(banks[slot]+off,256,args+4);
        if(request(ED_SMP_WRITE,args,n+4)!=11u || host_wire[9])return 1;
    }
    return 0;
}
static int finish(unsigned slot) {
    uint8_t args[600];args[0]=slot;
    unsigned n=pack7(banks[slot],480,args+1);
    if(request(ED_SMP_END,args,n+1)!=8u)return 255;
    return host_wire[6];
}
int main(void) {
    reset();memset(host_samples,255,sizeof host_samples);
    for(unsigned k=0;k<2;k++) {
        char name[64];snprintf(name,sizeof name,"build/organ-bank-%u.bin",k);
        FILE *f=fopen(name,"rb");if(!f)return 1;
        size_t n=fread(banks[k],1,sizeof banks[k],f);fclose(f);
        if(n!=sizeof banks[k])return 2;
    }
    organ_bank_check();if(organ_bank_ready || organ_bank_valid)return 3;
    if(getenv("ORGAN_BANK_WIRE")) {
        char line[4096];setvbuf(stdout,0,_IONBF,0);
        while(fgets(line,sizeof line,stdin)) {
            uint8_t frame[800];unsigned n=0;
            for(char *v=strtok(line," \r\n");v && n<sizeof frame;v=strtok(0," \r\n"))frame[n++]=(uint8_t)strtoul(v,0,16);
            if(n<6 || frame[0]!=240 || frame[n-1]!=247)return 20;
            request(frame[4],frame+5,n-6);
            for(unsigned i=0;i<host_wire_n;i++)printf("%02x ",host_wire[i]);
            printf("\n");
        }
        return !organ_bank_ready;
    }

    for(unsigned k=0;k<2;k++) {
        uint8_t a=k;
        if(request(ED_SMP_BEGIN,&a,1)!=8u || host_wire[6])return 4;
        if(upload(k,512,4096))return 5;
        organ_bank_check();if(organ_bank_ready)return 6;
        if(!finish(k))return 7;
        if(upload(k,4096,0x14000))return 8;
        if(finish(k))return 9;
        if(finish(k))return 10;
    }
    if(request(ED_ORGAN_BANK,0,0)!=42u || !organ_bank_ready || organ_bank_valid!=3)return 11;
    for(unsigned k=0;k<2;k++)if(memcmp(host_samples[k],banks[k],sizeof banks[k]))return 12;
    host_samples[1][4096]^=1;
    organ_bank_check();if(organ_bank_ready || organ_bank_valid!=1)return 13;
    uint8_t a=1;request(ED_SMP_BEGIN,&a,1);
    if(upload(1,512,0x14000) || finish(1))return 14;
    if(!organ_bank_ready)return 15;
    puts("Stop bank upload, partial upload, retry, corruption and readback passed");
    return 0;
}
