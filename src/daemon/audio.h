#ifndef YAPPIE_AUDIO_H
#define YAPPIE_AUDIO_H

#include <stddef.h>
#include <pipewire/pipewire.h>

#define YAPPIE_SAMPLE_RATE 16000

typedef struct audio_capture audio_capture_t;

/* Create audio capture (does not start recording). pw_loop is the daemon's loop. */
audio_capture_t *audio_capture_create(struct pw_loop *loop);

/* Start capturing audio into internal float buffer. Returns 0 on success. */
int audio_capture_start(audio_capture_t *ac);

/* Stop capturing. Transfers ownership of float buffer to caller.
   *out_samples and *out_n must be freed by caller (free(*out_samples)). */
int audio_capture_stop(audio_capture_t *ac, float **out_samples, size_t *out_n);

void audio_capture_destroy(audio_capture_t *ac);

#endif
