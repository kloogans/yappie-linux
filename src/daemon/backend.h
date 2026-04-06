#ifndef YAPPIE_BACKEND_H
#define YAPPIE_BACKEND_H

#include "config.h"
#include <stddef.h>

typedef struct backend backend_t;

typedef struct backend_ops {
    const char *name;
    int   (*init)(backend_t *b, const backend_config_t *cfg);
    char *(*transcribe)(backend_t *b, const float *samples, size_t n_samples);
    void  (*destroy)(backend_t *b);
} backend_ops_t;

struct backend {
    const backend_ops_t *ops;
    void                *priv;
    backend_config_t     cfg;
};

/* Backend implementations */
extern const backend_ops_t api_backend_ops;
extern const backend_ops_t tcp_backend_ops;

typedef struct {
    backend_t *backends;
    int        count;
} backend_manager_t;

/* Create backends from config. Returns NULL on failure. */
backend_manager_t *backend_manager_create(const yappie_config_t *cfg);

/* Try each backend in order, return first successful transcription.
   Returns allocated text or NULL. */
char *backend_manager_transcribe(backend_manager_t *bm,
                                 const float *samples, size_t n_samples);

void backend_manager_destroy(backend_manager_t *bm);

/* Hot-swap the local backend's model. Destroys old whisper context, loads new one.
   model_name is the short name (e.g. "small.en"), model_dir is the directory.
   Returns 0 on success, -1 on error (old model is destroyed either way). */
int backend_manager_swap_local_model(backend_manager_t *bm,
                                     const char *model_name,
                                     const char *model_dir);

#endif
