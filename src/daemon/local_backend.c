#ifdef HAVE_WHISPER

#include "backend.h"
#include "audio.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <whisper.h>

typedef struct {
    struct whisper_context *wctx;
    int n_threads;
} local_priv_t;

static int local_init(backend_t *b, const backend_config_t *cfg) {
    local_priv_t *p = calloc(1, sizeof(*p));

    /* Resolve model path */
    const char *model = cfg->model;
    if (!model || !model[0]) {
        fprintf(stderr, "local_backend: no model specified\n");
        free(p);
        return -1;
    }

    char path[1024];
    if (model[0] == '/') {
        snprintf(path, sizeof(path), "%s", model);
    } else {
        /* Look in model directory */
        char *model_dir = config_default_model_dir();
        /* Try exact name first, then with ggml- prefix and .bin suffix */
        snprintf(path, sizeof(path), "%s/%s", model_dir, model);
        if (access(path, R_OK) != 0) {
            snprintf(path, sizeof(path), "%s/ggml-%s.bin", model_dir, model);
        }
        if (access(path, R_OK) != 0) {
            snprintf(path, sizeof(path), "%s/ggml-%s", model_dir, model);
        }
        free(model_dir);
    }

    if (access(path, R_OK) != 0) {
        fprintf(stderr, "local_backend: model not found: %s\n", path);
        fprintf(stderr, "local_backend: download with: yappie model download %s\n", model);
        free(p);
        return -1;
    }

    fprintf(stderr, "local_backend: loading model from %s\n", path);

    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = cfg->gpu;

    p->wctx = whisper_init_from_file_with_params(path, cparams);
    if (!p->wctx) {
        fprintf(stderr, "local_backend: failed to load model\n");
        free(p);
        return -1;
    }

    /* Cap threads at physical cores or 8, whichever is less */
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    p->n_threads = (ncpus > 0 && ncpus < 8) ? (int)ncpus : 8;

    fprintf(stderr, "local_backend: model loaded (gpu=%s, threads=%d)\n",
            cfg->gpu ? "yes" : "no", p->n_threads);

    b->priv = p;
    return 0;
}

static char *local_transcribe(backend_t *b, const float *samples, size_t n_samples) {
    local_priv_t *p = b->priv;
    const backend_config_t *cfg = &b->cfg;

    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.language = (cfg->language && cfg->language[0]) ? cfg->language : NULL; /* NULL = auto */
    params.n_threads = p->n_threads;
    params.translate = false;
    params.no_timestamps = true;
    params.single_segment = false;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_special = false;
    params.print_timestamps = false;

    int rc = whisper_full(p->wctx, params, samples, (int)n_samples);
    if (rc != 0) {
        fprintf(stderr, "local_backend: whisper_full failed: %d\n", rc);
        return NULL;
    }

    int n_segments = whisper_full_n_segments(p->wctx);
    if (n_segments <= 0)
        return NULL;

    /* Concatenate all segments */
    size_t total_len = 0;
    for (int i = 0; i < n_segments; i++) {
        const char *text = whisper_full_get_segment_text(p->wctx, i);
        if (text) total_len += strlen(text);
    }

    if (total_len == 0)
        return NULL;

    char *result = malloc(total_len + 1);
    result[0] = '\0';
    size_t off = 0;

    for (int i = 0; i < n_segments; i++) {
        const char *text = whisper_full_get_segment_text(p->wctx, i);
        if (text) {
            size_t len = strlen(text);
            memcpy(result + off, text, len);
            off += len;
        }
    }
    result[off] = '\0';

    /* Trim leading/trailing whitespace */
    char *start = result;
    while (*start == ' ' || *start == '\n') start++;
    if (start != result)
        memmove(result, start, strlen(start) + 1);

    size_t rlen = strlen(result);
    while (rlen > 0 && (result[rlen-1] == ' ' || result[rlen-1] == '\n'))
        result[--rlen] = '\0';

    if (result[0] == '\0') {
        free(result);
        return NULL;
    }

    return result;
}

static void local_destroy(backend_t *b) {
    local_priv_t *p = b->priv;
    if (p) {
        if (p->wctx) whisper_free(p->wctx);
        free(p);
    }
}

const backend_ops_t local_backend_ops = {
    .name = "local",
    .init = local_init,
    .transcribe = local_transcribe,
    .destroy = local_destroy,
};

#endif /* HAVE_WHISPER */
