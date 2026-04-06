#ifndef YAPPIE_WAV_H
#define YAPPIE_WAV_H

#include <stddef.h>
#include <stdint.h>

/* Encode float samples (16kHz mono) as a WAV file in memory.
   Caller must free(*out_buf). Returns 0 on success. */
int wav_encode(const float *samples, size_t n_samples,
               uint8_t **out_buf, size_t *out_size);

#endif
