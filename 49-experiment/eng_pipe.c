/* Aeolus-derived compact pipe organ; GPL-3.0-or-later. */
#include "organ_data49.h"
static volatile uint8_t organ_on[49];
static volatile uint8_t organ_gain[49];
static uint8_t organ_page, organ_preset;
static const char *const organ_registration_names[] = {"PRINCIPAL 8", "FLUTE 8", "PRINCIPAL 8+4", "GENTLE", "TRUMPET", "PLENUM"};
static const char *const organ_names[49] = {
 "III PR8","III GEM8","III QD8","III SUA8","III RF4","III DF4","III QF","III OCT2","III SIF1","III CYM","III OBOE",
 "II RF8","II HFL8","II FD4","II NAS","II OTT2","II TER","II SESQ","II SEPT","II NONE","II KRUM","II MELO",
 "I PR8","I PR4","I OCT2","I OCT1","I Q513","I Q223","I TIB8","I CELE","I FL8","I FL4","I FL2","I CYM","I MIX","I TRUMP",
 "P SUB16","P PR16","P PR8","P PR4","P OCT2","P OCT1","P Q513","P Q223","P MIX","P FAG16","P TROM16","P BOM32","P TRUMP"
};
static uint32_t organ_slot(uint32_t page, uint32_t slot) { uint32_t n=page*8u+slot; return n<36u?n:n+1u; }
static void organ_apply_preset(uint32_t n) {
    static const uint8_t masks[6][7]={{22,255},{30,255},{22,23,255},{22,30,255},{35,255},{22,23,24,27,34,35,255}};
    for(uint32_t i=0;i<36u;i++) organ_on[i]=0;
    for(uint32_t k=0;k<7u && masks[n%6u][k]!=255u;k++) organ_on[masks[n%6u][k]]=1;
    organ_preset=(uint8_t)(n%6u);
}
static void organ_init(void) {
    for(uint32_t i=0;i<49u;i++){organ_gain[i]=90u;organ_on[i]=0;}
    organ_on[36]=1; organ_page=2; organ_apply_preset(0);
}
typedef struct { uint32_t pos[ORGAN_STOPS]; uint8_t valid; } pipe_voice_t;
static pipe_voice_t pipe_voices[NPART][NVOICE];
static void pipe_note_on(track_t *t, voice_t *v) {
    pipe_voice_t *p=&pipe_voices[(uint32_t)(t-trk)%NPART][(uint32_t)(v-t->v)%NVOICE];
    for(unsigned j=0;j<ORGAN_STOPS;j++) p->pos[j]=0;
    p->valid=(v->note>=48 && v->note<=(t == &trk[1] ? 60 : 84));
}
static int32_t organ_sample(const organ_info *a,uint32_t index) {
    uint8_t packed=organ_waves[a->offset+(index>>1)];
    return organ_levels[(index&1u) ? packed>>4 : packed&15u];
}
static void pipe_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m) {
    static const uint32_t ratio[7]={55109,58386,61858,65536,69433,73562,77936};
    pipe_voice_t *p=&pipe_voices[(uint32_t)(t-trk)%NPART][(uint32_t)(v-t->v)%NVOICE];
    if(!p->valid) return;
    uint32_t note=v->note-48, idx=(note+3u)/6u;
    int delta=(int)note-(int)(idx*6u);
    uint32_t rate=ratio[delta+3]/4u;
    for(unsigned j=(t == &trk[1] ? 36u : 0u); j<(t == &trk[1] ? 49u : 36u);j++) {
        if(!organ_on[j] || v->note<organ_ranges[j][0] || v->note>organ_ranges[j][1]) continue;
        const organ_info *a=&organ_metas[j][idx];
        uint32_t pos=p->pos[j], attack=(uint32_t)a->attack<<16, end=((uint32_t)a->attack+a->loop)<<16;
        uint32_t length=(uint32_t)a->loop<<16; int32_t gain=organ_gain[j];
        for(uint32_t i=0;i<n;i++) {
            uint32_t s=pos>>16, next=s+1u;
            if(next>=a->attack+a->loop) next=a->attack;
            int32_t x=organ_sample(a,s), y=organ_sample(a,next);
            int32_t sample=x+((((y-x)/2)*(int32_t)((pos&65535u)>>1))>>14);
            out[i] += voice_amp(((sample/8)*gain)/127, m, i)*2;
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
