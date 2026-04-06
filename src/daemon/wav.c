#include "wav.h"
#include "audio.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int wav_encode(const float *samples, size_t n_samples,
               uint8_t **out_buf, size_t *out_size) {
    uint32_t sample_rate = YAPPIE_SAMPLE_RATE;
    uint16_t channels = 1;
    uint16_t bits = 16;
    uint32_t data_size = n_samples * sizeof(int16_t);
    uint32_t file_size = 44 + data_size;

    uint8_t *buf = malloc(file_size);
    if (!buf) return -1;

    /* RIFF header */
    memcpy(buf, "RIFF", 4);
    uint32_t chunk_size = file_size - 8;
    memcpy(buf + 4, &chunk_size, 4);
    memcpy(buf + 8, "WAVE", 4);

    /* fmt chunk */
    memcpy(buf + 12, "fmt ", 4);
    uint32_t fmt_size = 16;
    memcpy(buf + 16, &fmt_size, 4);
    uint16_t audio_fmt = 1; /* PCM */
    memcpy(buf + 20, &audio_fmt, 2);
    memcpy(buf + 22, &channels, 2);
    memcpy(buf + 24, &sample_rate, 4);
    uint32_t byte_rate = sample_rate * channels * bits / 8;
    memcpy(buf + 28, &byte_rate, 4);
    uint16_t block_align = channels * bits / 8;
    memcpy(buf + 32, &block_align, 2);
    memcpy(buf + 34, &bits, 2);

    /* data chunk */
    memcpy(buf + 36, "data", 4);
    memcpy(buf + 40, &data_size, 4);

    int16_t *pcm = (int16_t *)(buf + 44);
    for (size_t i = 0; i < n_samples; i++)
        pcm[i] = (int16_t)fmaxf(-32768.0f, fminf(32767.0f, samples[i] * 32767.0f));

    *out_buf = buf;
    *out_size = file_size;
    return 0;
}
