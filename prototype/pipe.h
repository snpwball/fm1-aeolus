/* Portable playback of pre-generated Aeolus pipes. GPL-3.0-or-later. */
#ifndef ORGAN_PIPE_H
#define ORGAN_PIPE_H
#include <stdint.h>
#include <stddef.h>
typedef struct { const uint8_t *samples; uint32_t attack, loop, step, release; } organ_pipe;
typedef struct { organ_pipe pipe[37]; } organ_bank;
typedef struct { const organ_pipe *pipe; uint32_t pos, remaining, release_total; int on; } organ_voice;
int organ_bank_load(organ_bank *bank, const void *data, size_t bytes);
int organ_note_on(organ_voice *voice, const organ_bank *bank, unsigned midi_note);
void organ_note_off(organ_voice *voice);
void organ_render(organ_voice *voice, int32_t *out, unsigned frames);
#endif
