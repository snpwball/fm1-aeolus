/* Aeolus-derived 16-stop pipe organ; GPL-3.0-or-later. */
#include "organ_quality.h"
static volatile uint8_t organ_on[ORGAN_STOPS];
static volatile uint8_t organ_gain[ORGAN_STOPS];
static uint8_t organ_page, organ_preset, organ_native;
static uint32_t organ_fader_until;
static volatile uint8_t organ_trem_depth;
static volatile int16_t organ_bend14;          /* pitch wheel, -8192..8191 (main.c smk_wheels) */
static uint32_t organ_trem_phase;
static uint8_t organ_card_dirty, organ_fader_dirty, organ_chrome_dirty;
static const char *const organ_registration_names[] = {"PRINCIPAL 8", "FLUTE 8", "PRINCIPAL 8+4", "GENTLE", "TRUMPET", "PLENUM", "FLUTE CHORUS", "MEDITATION", "OBOE SOLO", "BACH PLENUM", "QUINTADENA", "GEM CHORUS", "DOLCE FLUTE", "CROMORNE", "CINEMA", "INTERSTELLAR", "MAD RUSH", "GLASS OPENING", "KOYAANISQATSI", "THE HOURS", "HOGWARTS XMAS", "CHERNOBYL", "QUEEN FOREVER"};
static const char *const organ_names[ORGAN_STOPS] = {
 "PRIN 8","PRIN 4","OCT 2","MIXTUR","QUINT","FLUTE 8","FLUTE 4","SUABILE",
 "OBOE","TRUMPET","P SUB16","P PRIN8", "QUINTAD", "GEMSH 8", "DOLCE 4", "CROMORN"
};
static uint32_t organ_slot(uint32_t page, uint32_t slot) { uint32_t n=page*8u+slot; return n<ORGAN_STOPS?n:255u; }
static void organ_apply_preset(uint32_t n) {
    static const uint8_t masks[][8]={{0,255},{5,255},{0,1,255},{0,5,255},{9,255},{0,1,2,3,255},{5,6,255},{5,7,255},{5,8,255},{0,1,2,3,255},{12,255},{13,1,2,255},{5,14,255},{5,15,255},{0,5,7,255},{5,6,255},
      {5,12,255}, {5,7,14,255}, {0,5,12,6,255}, {5,13,255}, {0,1,2,4,255}, {0,1,2,4,255}, {0,5,7,1,255}};
    uint32_t selected=n%NELEM(organ_registration_names);
    for(uint32_t i=0;i<ORGAN_STOPS;i++) if(i!=10u && i!=11u) {organ_on[i]=0; organ_gain[i]=100;}
    for(uint32_t k=0;k<8u && masks[selected][k]!=255u;k++) organ_on[masks[selected][k]]=1;
    if(selected==5u) {organ_gain[1]=110; organ_gain[2]=150; organ_gain[3]=170;}
    /* Starting balances for the film-inspired registrations. */
    if(selected==16u) {organ_gain[12]=65;}
    if(selected==17u) {organ_gain[7]=60; organ_gain[14]=55;}
    if(selected==18u) {organ_gain[0]=85; organ_gain[12]=70; organ_gain[6]=75;}
    if(selected==19u) {organ_gain[13]=60;}
    if(selected==20u) {organ_gain[1]=90; organ_gain[2]=70; organ_gain[4]=40;}
    if(selected==21u) {organ_gain[1]=80; organ_gain[2]=60; organ_gain[4]=65;}
    if(selected==22u) {organ_gain[5]=85; organ_gain[7]=65; organ_gain[1]=70;}
    organ_preset=(uint8_t)selected;
}
static void organ_init(void) {
    for(uint32_t i=0;i<ORGAN_STOPS;i++){organ_gain[i]=100u;organ_on[i]=0;}
    organ_on[10]=1; organ_page=0; organ_apply_preset(0);
}
typedef struct { uint32_t pos[ORGAN_STOPS]; uint16_t fade[ORGAN_STOPS]; uint8_t valid, touch; } pipe_voice_t;
#ifndef ORGAN_BANK_PTR
#ifdef ORGAN_HOST
static uint8_t organ_bank_image[2][0x14000];
#define ORGAN_BANK_PTR(k) (organ_bank_image[k])
#else
#define ORGAN_BANK_PTR(k) fm1_xip_ptr(0xC2000u+(k)*0x14000u)
#endif
#endif
static uint8_t organ_bank_ready, organ_bank_valid;
static uint32_t organ_le32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void organ_bank_check(void) {
    organ_bank_ready=0; organ_bank_valid=0;
    for(uint32_t k=0;k<2u;k++) {
        const uint8_t *b=ORGAN_BANK_PTR(k);
        if(organ_le32(b)!=0x504D5346u || b[4]!=1u || b[5] || b[6]!=1u ||
           organ_le32(b+16)!=organ_bank_length[k] || organ_le32(b+20)!=organ_bank_crc[k] ||
           organ_le32(b+24)!=0x4F524731u || organ_le32(b+28)!=k) continue;
        uint32_t crc=0xffffffffu;
        for(uint32_t i=0;i<organ_bank_length[k];i++) {
            crc^=b[512u+i];
            for(unsigned j=0;j<8u;j++) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
        }
        if((crc^0xffffffffu)==organ_bank_crc[k]) organ_bank_valid|=(uint8_t)(1u<<k);
    }
    organ_bank_ready=organ_bank_valid==3u;
}
static const int8_t *organ_wave(unsigned stop) {
    uint32_t k=organ_bank_slot[stop];
    return (const int8_t *)(k<2u ? ORGAN_BANK_PTR(k)+512u+organ_bank_offset[stop] : organ_waves[stop]);
}
#ifdef ORGAN_HOST
static pipe_voice_t pipe_voices[NPART][NVOICE];
#else
static pipe_voice_t pipe_voices[NPART][NVOICE] __attribute__((section(".pool")));
#endif
typedef struct { uint16_t age[ORGAN_STOPS]; int16_t low[ORGAN_STOPS]; } organ_touch_t;
static organ_touch_t organ_touch[NPART][NVOICE];
/* Keep sustained level nearly even; velocity shapes the start of each pipe. */
static uint8_t organ_velocity(uint32_t v) {
    if(v>127u) v=127u;
    return (uint8_t)(16u + (55u*v*127u + 56u*v*v + 8064u)/16129u);
}
static void pipe_note_on(track_t *t, voice_t *v) {
    pipe_voice_t *p=&pipe_voices[(uint32_t)(t-trk)%NPART][(uint32_t)(v-t->v)%NVOICE];
    organ_touch_t *q=&organ_touch[(uint32_t)(t-trk)%NPART][(uint32_t)(v-t->v)%NVOICE];
    p->touch=v->vel; v->vel=organ_velocity(v->vel);
    for(unsigned j=0;j<ORGAN_STOPS;j++) {p->pos[j]=0; q->age[j]=0; q->low[j]=0; p->fade[j]=organ_on[j]?256u:0u;}
    p->valid=t==&trk[1] ? (v->note>=48 && v->note<=60) : v->note<=127;
}
/* 2^(semitones/12), Q16, for -36..36. Octave buttons reach ±3 octaves past the stored pipes. */
static uint32_t organ_semi(int32_t s) {
    static const uint32_t R[73]={8192,8679,9195,9742,10321,10935,11585,12274,13004,13777,14596,15464,16384,17358,18390,19484,20643,21870,23170,24548,26008,27554,29193,30929,32768,34716,36781,38968,41285,43740,46341,49097,52016,55109,58386,61858,65536,69433,73562,77936,82570,87480,92682,98193,104032,110218,116772,123715,131072,138866,147123,155872,165140,174960,185364,196386,208064,220436,233544,247431,262144,277732,294247,311744,330281,349920,370728,392772,416128,440872,467088,494862,524288};
    if(s<-36) s=-36; if(s>36) s=36;
    return R[s+36];
}
static void pipe_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m) {
    pipe_voice_t *p=&pipe_voices[(uint32_t)(t-trk)%NPART][(uint32_t)(v-t->v)%NVOICE];
    if(!p->valid || !organ_bank_ready) return;
    organ_touch_t *q=&organ_touch[(uint32_t)(t-trk)%NPART][(uint32_t)(v-t->v)%NVOICE];
    static const uint32_t pitch_factor[65]={58386,58597,58809,59022,59235,59449,59664,59880,60097,60314,60532,60751,60971,61191,61413,61635,61858,62081,62306,62531,62757,62984,63212,63441,63670,63901,64132,64364,64596,64830,65065,65300,65536,65773,66011,66250,66489,66730,66971,67213,67456,67700,67945,68191,68438,68685,68933,69183,69433,69684,69936,70189,70443,70698,70953,71210,71468,71726,71985,72246,72507,72769,73032,73297,73562};
    int32_t bend=m->pitch16-(int32_t)v->note*16+(int32_t)organ_bend14*32/8192;
    if(bend < -32) bend=-32; if(bend > 32) bend=32;
    int32_t sine=SINE[organ_trem_phase>>22];
    uint32_t dip=(uint32_t)organ_trem_depth*(uint32_t)(sine+32768)/127u*22000u/65535u;
    int32_t trem=32767-(int32_t)dip;
    int32_t cents=organ_trem_depth?(int32_t)(((int64_t)organ_trem_depth*sine)/(127*1311)):0;
    uint32_t mul=(uint32_t)(65536+cents*38);
    uint32_t bend_factor=pitch_factor[bend+32];
    bend_factor=((bend_factor>>16)*mul)+(((bend_factor&65535u)*mul)>>16);
    int32_t rel=(int32_t)v->note-48;
    for(unsigned j=0; j<ORGAN_STOPS; j++) {
        if ((t == &trk[1]) != (j==10u || j==11u)) continue;
        uint32_t target=organ_on[j]?256u:0u, fade=p->fade[j];
        if (!target && !fade) continue;
        if (target && !fade) {p->pos[j]=0; q->age[j]=0; q->low[j]=0;}
        int32_t gap=(int32_t)organ_gaps[j], last=(int32_t)organ_last[j];
        int32_t idx=rel<=0?0:rel>=last?(last+gap/2)/gap:(rel+gap/2)/gap;
        int32_t delta=rel-idx*gap;
        uint32_t rate=((organ_semi(delta)>>4)*bend_factor)>>12;
        const organ_info *a=&organ_metas[j][idx];
        const int8_t *wave=organ_wave(j)+a->offset;
        uint32_t age=q->age[j]; int32_t low=q->low[j];
        uint32_t onset=64u+(127u-(uint32_t)p->touch)*8u, onset_step=65536u/onset;
        uint32_t pos=p->pos[j], attack=(uint32_t)a->attack<<16, lim=a->attack+a->loop;
        uint32_t end=lim<<16, length=(uint32_t)a->loop<<16; int32_t gain=((int32_t)organ_gain[j]*organ_scale[j])/100;
        for(uint32_t i=0;i<n;i++) {
            uint32_t s=pos>>16, next;
            if(s>=lim) s=a->attack;
            next=s+1u; if(next>=lim) next=a->attack;
            int32_t x=(int32_t)wave[s]*32, y=(int32_t)wave[next]*32;
            /* Trumpet loops do not close. Blend the last 32 samples into the start so the seam is not a click. */
            if(j==9u && s>=a->attack && a->loop>32u) {
                uint32_t lp=s-a->attack;
                if(lp+32u>=a->loop) {
                    uint32_t k=a->loop-lp, hs=a->attack+32u-k, hn=hs+1u<lim?hs+1u:a->attack;
                    x=(x*(int32_t)k+(int32_t)wave[hs]*32*(int32_t)(32u-k))/32;
                    y=(y*(int32_t)k+(int32_t)wave[hn]*32*(int32_t)(32u-k))/32;
                }
            }
            int32_t sample=x+((((y-x)/2)*(int32_t)((pos&65535u)>>1))>>14);
            if(age<8192u) {
                low+=(sample-low)/8;
                /* Shape the pipe attack, returning to its original tone after 186 ms. */
                int32_t strength=(int32_t)p->touch-64;
                if(p->touch>96u) strength+=((int32_t)p->touch-96)*2;
                int32_t color=strength*(int32_t)(8192u-age)/8192;
                sample+=(sample-low)*color/128;
                if(p->touch>96u) {
                    uint32_t boost=((uint32_t)p->touch-96u)*(8192u-age)/8192u;
                    sample+=sample*(int32_t)boost/100;
                }
                if(age<onset) sample=(sample*(int32_t)(age*onset_step))/65536;
                age++;
            }
            if (fade<target) fade++; else if (fade>target) fade--;
            int32_t level=(((sample*gain)/127)*trem)/32767;
            if(fade!=256u) level=(level*(int32_t)fade)/256;
            out[i] += voice_amp(level, m, i)*2;
            pos += (pos<attack?1u:a->step)*rate;
            if(pos>=end) pos-=length;
        }
        p->pos[j]=pos; p->fade[j]=(uint16_t)fade; q->age[j]=(uint16_t)age; q->low[j]=(int16_t)low;
    }
}
static const preset_t PIPE_PRESETS[] = {
    {"ORGAN", {90,90,90,75,90,80,90,80}, {0,0,127,35}, 0, 0, FX(0,0,0,25)}
};
static const engine_t ENG_WHEEL = {
    .name="ORGAN", .page_title={"STOPS","STOPS"},
    .edit={{"--",F_PCT,0,127,90,0,0},{"--",F_PCT,0,127,90,0,0},{"--",F_PCT,0,127,90,0,0},{"--",F_PCT,0,127,90,0,0},
           {"--",F_PCT,0,127,90,0,0},{"--",F_PCT,0,127,90,0,0},{"--",F_PCT,0,127,90,0,0},{"--",F_PCT,0,127,90,0,0}},
    .presets=PIPE_PRESETS, .npresets=NELEM(PIPE_PRESETS), .note_on=pipe_note_on, .render=pipe_render,
    .knob={P_E0,P_E1,P_E2,P_E3}
};
