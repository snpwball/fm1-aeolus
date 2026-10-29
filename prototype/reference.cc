// Reference render using Aeolus's original pipe generation and playback.
// GPL-3.0-or-later; see vendor/aeolus-main/COPYING.
#include "addsynth.h"
#include "rankwave.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <vector>

static void u16(FILE *f, uint16_t x) { fputc(x & 255, f); fputc(x >> 8, f); }
static void u32(FILE *f, uint32_t x) { u16(f, x & 65535); u16(f, x >> 16); }
int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "Usage: %s STOP-DIR stop.ae0 output.wav\n", argv[0]); return 1; }
    Addsynth stop;
    if (strlen(argv[2]) >= sizeof stop._filename) return 1;
    strcpy(stop._filename, argv[2]);
    if (stop.load(argv[1])) { fprintf(stderr, "Cannot load stop\n"); return 1; }
    float scale[12]; for (int i=0; i<12; ++i) scale[i] = powf(2.0f, i/12.0f);
    Rankwave rank(stop._n0, stop._n1);
    rank.gen_waves(&stop, 44100, 440, scale);
    float bus[4 * PERIOD] = {};
    rank.set_param(bus, 0, 'C');
    const int notes[] = {60,64,67};
    for (int n : notes) rank.note_on(n);
    std::vector<float> pcm;
    for (int block=0; block<2068; ++block) {
        if (block==1378) for (int n : notes) rank.note_off(n);
        memset(bus, 0, sizeof bus); rank.play(1);
        for (int i=0; i<PERIOD; ++i) pcm.push_back(bus[PERIOD+i] + bus[2*PERIOD+i]);
    }
    float peak=0; for (float x : pcm) peak=std::max(peak, fabsf(x));
    if (!(peak>0) || !std::isfinite(peak)) { fprintf(stderr, "No finite audio\n"); return 1; }
    FILE *f=fopen(argv[3], "wb"); if (!f) return 1;
    uint32_t bytes=uint32_t(pcm.size()*2); fwrite("RIFF",1,4,f); u32(f,36+bytes); fwrite("WAVEfmt ",1,8,f); u32(f,16); u16(f,1); u16(f,1); u32(f,44100); u32(f,88200); u16(f,2); u16(f,16); fwrite("data",1,4,f); u32(f,bytes);
    for (float x : pcm) { int16_t y=(int16_t)lrintf(x/peak*28000); u16(f,(uint16_t)y); }
    fclose(f); printf("%s: notes %d..%d, %.2f seconds, peak %.6f\n", stop._stopname, stop._n0, stop._n1, pcm.size()/44100.0, peak);
}
