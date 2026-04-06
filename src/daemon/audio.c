#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>

#define MAX_RECORD_SECONDS 300  /* 5 minutes */
#define MAX_SAMPLES (YAPPIE_SAMPLE_RATE * MAX_RECORD_SECONDS)

struct audio_capture {
    struct pw_stream *stream;
    struct pw_loop   *loop;

    float  *buf;
    size_t  buf_cap;
    size_t  buf_len;
    int     active;
};

static void on_process(void *data) {
    audio_capture_t *ac = data;
    struct pw_buffer *b = pw_stream_dequeue_buffer(ac->stream);
    if (!b) return;

    struct spa_buffer *sb = b->buffer;
    float *src = sb->datas[0].data;
    if (!src) {
        pw_stream_queue_buffer(ac->stream, b);
        return;
    }

    uint32_t n_bytes = sb->datas[0].chunk->size;
    uint32_t n_samples = n_bytes / sizeof(float);

    /* Drop samples if buffer is full (RT-safe: no realloc in callback) */
    if (ac->buf_len + n_samples > ac->buf_cap)
        n_samples = ac->buf_cap > ac->buf_len ? ac->buf_cap - ac->buf_len : 0;

    if (n_samples > 0) {
        memcpy(ac->buf + ac->buf_len, src, n_samples * sizeof(float));
        ac->buf_len += n_samples;
    }

    pw_stream_queue_buffer(ac->stream, b);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

audio_capture_t *audio_capture_create(struct pw_loop *loop) {
    audio_capture_t *ac = calloc(1, sizeof(*ac));
    ac->loop = loop;

    struct pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Communication",
        PW_KEY_NODE_NAME, "yappie",
        PW_KEY_APP_NAME, "Yappie",
        NULL);

    ac->stream = pw_stream_new_simple(loop, "yappie-capture", props,
                                       &stream_events, ac);
    if (!ac->stream) {
        fprintf(stderr, "audio: failed to create PipeWire stream\n");
        free(ac);
        return NULL;
    }

    return ac;
}

int audio_capture_start(audio_capture_t *ac) {
    if (ac->active) return -1;

    ac->buf_cap = MAX_SAMPLES;
    ac->buf = malloc(ac->buf_cap * sizeof(float));
    ac->buf_len = 0;

    /* Request 16kHz mono float32 */
    uint8_t params_buf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(params_buf, sizeof(params_buf));
    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_F32,
            .rate = YAPPIE_SAMPLE_RATE,
            .channels = 1));

    int res = pw_stream_connect(ac->stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                                PW_STREAM_FLAG_AUTOCONNECT |
                                PW_STREAM_FLAG_MAP_BUFFERS |
                                PW_STREAM_FLAG_RT_PROCESS,
                                params, 1);
    if (res < 0) {
        fprintf(stderr, "audio: stream connect failed: %d\n", res);
        free(ac->buf);
        ac->buf = NULL;
        return -1;
    }

    ac->active = 1;
    return 0;
}

int audio_capture_stop(audio_capture_t *ac, float **out_samples, size_t *out_n) {
    if (!ac->active) return -1;

    pw_stream_disconnect(ac->stream);
    ac->active = 0;

    *out_samples = ac->buf;
    *out_n = ac->buf_len;

    /* Detach buffer from capture (caller owns it now) */
    ac->buf = NULL;
    ac->buf_len = 0;
    ac->buf_cap = 0;

    return 0;
}

void audio_capture_destroy(audio_capture_t *ac) {
    if (!ac) return;
    if (ac->active)
        pw_stream_disconnect(ac->stream);
    pw_stream_destroy(ac->stream);
    free(ac->buf);
    free(ac);
}
