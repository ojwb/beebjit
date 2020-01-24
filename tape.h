#ifndef BEEBJIT_TAPE_H
#define BEEBJIT_TAPE_H

#include <stdint.h>

struct tape_struct;

struct bbc_options;
struct serial_struct;
struct timing_struct;

struct tape_struct* tape_create(struct timing_struct* p_timing,
                                struct serial_struct* p_serial,
                                struct bbc_options* p_options);
void tape_destroy(struct tape_struct* p_tape);

void tape_load(struct tape_struct* p_disc, const char* p_filename);

int tape_is_playing(struct tape_struct* p_tape);

void tape_play(struct tape_struct* p_tape);
void tape_stop(struct tape_struct* p_tape);

#endif /* BEEBJIT_TAPE_H */
