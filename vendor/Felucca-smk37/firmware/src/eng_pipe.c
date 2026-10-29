/* Aeolus-derived 12-stop pipe organ; GPL-3.0-or-later. */
#include "organ_quality.h"
static volatile uint8_t organ_on[ORGAN_STOPS];
static volatile uint8_t organ_gain[ORGAN_STOPS];
static uint8_t organ_page, organ_preset, organ_native;
static uint32_t organ_fader_until;
static volatile uint8_t organ_trem_depth;
static volatile int16_t organ_bend14;          /* pitch wheel, -8192..8191 (main.c smk_wheels) */
static uint32_t organ_trem_phase;
static uint8_t organ_card_dirty, organ_fader_dirty, organ_chrome_dirty;
static const char *const organ_registration_names[] = {"PRINCIPAL 8", "FLUTE 8", "PRINCIPAL 8+4", "GENTLE", "TRUMPET", "PLENUM", "FLUTE CHORUS", "MEDITATION", "OBOE SOLO", "CINEMA"};
static const char *const organ_names[ORGAN_STOPS] = {
 "PRIN 8","PRIN 4","OCT 2","MIXTUR","QUINT","FLUTE 8","FLUTE 4","SUABILE",
 "OBOE","TRUMPET","P SUB16","P PRIN8"
};
static uint32_t organ_slot(uint32_t page, uint32_t slot) { uint32_t n=page*8u+slot; return n<ORGAN_STOPS?n:255u; }
static void organ_apply_preset(uint32_t n) {
    static const uint8_t masks[10][7]={{0,255},{5,255},{0,1,255},{0,5,255},{9,255},{0,1,2,3,255},{5,6,255},{5,7,255},{5,8,255},{0,5,7,255}};
    uint32_t selected=n%NELEM(organ_registration_names);
    for(uint32_t i=0;i<10u;i++) {organ_on[i]=0; organ_gain[i]=100;}
    for(uint32_t k=0;k<7u && masks[selected][k]!=255u;k++) organ_on[masks[selected][k]]=1;
    if(selected==5u) {organ_gain[1]=110; organ_gain[2]=150; organ_gain[3]=170;}
    organ_preset=(uint8_t)selected;
}
static void organ_init(void) {
    for(uint32_t i=0;i<ORGAN_STOPS;i++){organ_gain[i]=100u;organ_on[i]=0;}
    organ_on[10]=1; organ_page=0; organ_apply_preset(0);
}
typedef struct { uint32_t pos[ORGAN_STOPS]; uint8_t valid; } pipe_voice_t;
static pipe_voice_t pipe_voices[NPART][NVOICE];
static void pipe_note_on(track_t *t, voice_t *v) {
    pipe_voice_t *p=&pipe_voices[(uint32_t)(t-trk)%NPART][(uint32_t)(v-t->v)%NVOICE];
    for(unsigned j=0;j<ORGAN_STOPS;j++) p->pos[j]=0;
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
    for(unsigned j=(t == &trk[1] ? 10u : 0u); j<(t == &trk[1] ? ORGAN_STOPS : 10u);j++) {
        if(!organ_on[j]) continue;
        int32_t gap=(int32_t)organ_gaps[j], last=(int32_t)organ_last[j];
        int32_t idx=rel<=0?0:rel>=last?(last+gap/2)/gap:(rel+gap/2)/gap;
        int32_t delta=rel-idx*gap;
        uint32_t rate=((organ_semi(delta)>>4)*bend_factor)>>12;
        const organ_info *a=&organ_metas[j][idx]; const int8_t *wave=organ_waves[j]+a->offset;
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
            out[i] += voice_amp((((sample*gain)/127)*trem)/32767, m, i)*2;
            pos += (pos<attack?1u:a->step)*rate;
            if(pos>=end) pos-=length;
        }
        p->pos[j]=pos;
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
