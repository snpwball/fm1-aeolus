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
typedef struct { uint32_t block; int8_t data[16]; } organ_cache_t;
typedef struct { uint32_t pos[ORGAN_STOPS]; uint16_t fade[ORGAN_STOPS]; organ_cache_t cache[ORGAN_STOPS][2]; uint8_t valid, touch; } pipe_voice_t;
/* Independent 16-sample blocks; every eighth block has a direct checkpoint. */
/* A cache hit must be a load, without saving registers for the decoder. */
static __attribute__((noinline)) void organ_decode(organ_cache_t *c, unsigned stop, uint32_t block) {
    const uint8_t *src=organ_waves[stop]; uint32_t off=organ_checks[stop][block>>3];
    for(uint32_t n=0;n<(block&7u);n++) off+=2u+2u*src[off+1u];
    int32_t base=(int8_t)src[off]; uint32_t bits=src[off+1u]; off+=2;
    uint32_t mask=(1u<<bits)-1u;
    for(unsigned group=0;group<4u;group++) {
        uint32_t bit=group*4u*bits, shift=bit&7u, count=(shift+4u*bits+7u)>>3;
        const uint8_t *s=src+off+(bit>>3);
        uint32_t acc=count?s[0]:0u;
        if(count>1u) acc|=(uint32_t)s[1]<<8;
        if(count>2u) acc|=(uint32_t)s[2]<<16;
        if(count>3u) acc|=(uint32_t)s[3]<<24;
        acc>>=shift;
        for(unsigned n=0;n<4u;n++) {
            c->data[group*4u+n]=(int8_t)(base+(int32_t)(acc&mask));
            acc>>=bits;
        }
    }
    c->block=block;
}
static __attribute__((always_inline)) inline int32_t organ_sample(pipe_voice_t *p, unsigned stop, uint32_t index) {
    if((ORGAN_RAW_MASK>>stop)&1u) return (int8_t)organ_waves[stop][index];
    uint32_t block=index>>4; organ_cache_t *c=&p->cache[stop][block&1u];
    if(c->block!=block) organ_decode(c,stop,block);
    return c->data[index&15u];
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
    for(unsigned j=0;j<ORGAN_STOPS;j++) {p->pos[j]=0; q->age[j]=0; q->low[j]=0; p->fade[j]=organ_on[j]?256u:0u; p->cache[j][0].block=UINT32_MAX; p->cache[j][1].block=UINT32_MAX;}
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
    if(!p->valid) return;
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
        uint32_t age=q->age[j]; int32_t low=q->low[j];
        uint32_t onset=64u+(127u-(uint32_t)p->touch)*8u, onset_step=65536u/onset;
        uint32_t pos=p->pos[j], attack=(uint32_t)a->attack<<16, lim=a->attack+a->loop;
        uint32_t end=lim<<16, length=(uint32_t)a->loop<<16; int32_t gain=((int32_t)organ_gain[j]*organ_scale[j])/100;
        for(uint32_t i=0;i<n;i++) {
            uint32_t s=pos>>16, next;
            if(s>=lim) s=a->attack;
            next=s+1u; if(next>=lim) next=a->attack;
            int32_t x=organ_sample(p,j,a->offset+s)*32, y=organ_sample(p,j,a->offset+next)*32;
            /* Trumpet loops do not close. Blend the last 32 samples into the start so the seam is not a click. */
            if(j==9u && s>=a->attack && a->loop>32u) {
                uint32_t lp=s-a->attack;
                if(lp+32u>=a->loop) {
                    uint32_t k=a->loop-lp, hs=a->attack+32u-k, hn=hs+1u<lim?hs+1u:a->attack;
                    x=(x*(int32_t)k+organ_sample(p,j,a->offset+hs)*32*(int32_t)(32u-k))/32;
                    y=(y*(int32_t)k+organ_sample(p,j,a->offset+hn)*32*(int32_t)(32u-k))/32;
                }
            }
            int32_t sample=x+((((y-x)/2)*(int32_t)((pos&65535u)>>1))>>14);
            if(age<8192u) {
                low+=(sample-low)/8;
                /* Shape the pipe attack, returning to its original tone after 186 ms. */
                int32_t color=((int32_t)p->touch-64)*(int32_t)(8192u-age)/8192;
                sample+=(sample-low)*color/128;
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
