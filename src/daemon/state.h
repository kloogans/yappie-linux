#ifndef YAPPIE_STATE_H
#define YAPPIE_STATE_H

typedef enum {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_TRANSCRIBING,
} yappie_state_t;

const char *state_to_string(yappie_state_t s);

/* Returns 0 on valid transition, -1 if rejected. */
int state_toggle(yappie_state_t *s);

#endif
