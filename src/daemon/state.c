#include "state.h"

const char *state_to_string(yappie_state_t s) {
    switch (s) {
    case STATE_IDLE:         return "idle";
    case STATE_RECORDING:    return "recording";
    case STATE_TRANSCRIBING: return "transcribing";
    }
    return "unknown";
}

int state_toggle(yappie_state_t *s) {
    switch (*s) {
    case STATE_IDLE:
        *s = STATE_RECORDING;
        return 0;
    case STATE_RECORDING:
        *s = STATE_TRANSCRIBING;
        return 0;
    case STATE_TRANSCRIBING:
        return -1;
    }
    return -1;
}
