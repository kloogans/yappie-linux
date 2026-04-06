#include "backend.h"
#include "local_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const backend_ops_t *ops_for_type(backend_type_t type) {
    switch (type) {
    case BACKEND_API: return &api_backend_ops;
    case BACKEND_TCP: return &tcp_backend_ops;
    case BACKEND_LOCAL:
#ifdef HAVE_WHISPER
        return &local_backend_ops;
#else
        fprintf(stderr, "backend: local backend not available (built without whisper.cpp)\n");
        return NULL;
#endif
    }
    return NULL;
}

backend_manager_t *backend_manager_create(const yappie_config_t *cfg) {
    backend_manager_t *bm = calloc(1, sizeof(*bm));
    bm->backends = calloc(cfg->backend_count, sizeof(backend_t));
    bm->count = 0;

    for (int i = 0; i < cfg->backend_count; i++) {
        const backend_ops_t *ops = ops_for_type(cfg->backends[i].type);
        if (!ops) {
            fprintf(stderr, "backend: skipping '%s' (unsupported type)\n",
                    cfg->backends[i].name ? cfg->backends[i].name : "?");
            continue;
        }

        backend_t *b = &bm->backends[bm->count];
        b->ops = ops;
        b->cfg = cfg->backends[i]; /* shallow copy — config outlives manager */

        /* Local backends own their model string (needed for hot-swap) */
        if (b->cfg.type == BACKEND_LOCAL && b->cfg.model)
            b->cfg.model = strdup(b->cfg.model);

        if (ops->init(b, &b->cfg) < 0) {
            fprintf(stderr, "backend: failed to init '%s'\n",
                    cfg->backends[i].name ? cfg->backends[i].name : "?");
            continue;
        }

        bm->count++;
    }

    return bm;
}

char *backend_manager_transcribe(backend_manager_t *bm,
                                 const float *samples, size_t n_samples) {
    for (int i = 0; i < bm->count; i++) {
        backend_t *b = &bm->backends[i];
        char *text = b->ops->transcribe(b, samples, n_samples);
        if (text && text[0] != '\0') {
            fprintf(stderr, "backend: transcribed via '%s'\n",
                    b->cfg.name ? b->cfg.name : b->ops->name);
            return text;
        }
        free(text);
        fprintf(stderr, "backend: '%s' failed, trying next\n",
                b->cfg.name ? b->cfg.name : b->ops->name);
    }
    return NULL;
}

int backend_manager_swap_local_model(backend_manager_t *bm,
                                     const char *model_name,
                                     const char *model_dir) {
    (void)model_dir; /* local_init resolves path via config_default_model_dir */

    /* Find the first local backend */
    backend_t *local = NULL;
    for (int i = 0; i < bm->count; i++) {
        if (bm->backends[i].cfg.type == BACKEND_LOCAL) {
            local = &bm->backends[i];
            break;
        }
    }

    if (!local) {
        fprintf(stderr, "backend: no local backend configured\n");
        return -1;
    }

    /* Tear down the old context */
    if (local->ops->destroy)
        local->ops->destroy(local);
    local->priv = NULL;

    /* Update the model name in the config (we own this copy) */
    free(local->cfg.model);
    local->cfg.model = strdup(model_name);

    /* Re-init with the new model */
    if (local->ops->init(local, &local->cfg) < 0) {
        fprintf(stderr, "backend: failed to load model '%s'\n", model_name);
        return -1;
    }

    fprintf(stderr, "backend: swapped to model '%s'\n", model_name);
    return 0;
}

void backend_manager_destroy(backend_manager_t *bm) {
    if (!bm) return;
    for (int i = 0; i < bm->count; i++) {
        if (bm->backends[i].ops->destroy)
            bm->backends[i].ops->destroy(&bm->backends[i]);
        /* Free owned model string for local backends */
        if (bm->backends[i].cfg.type == BACKEND_LOCAL)
            free(bm->backends[i].cfg.model);
    }
    free(bm->backends);
    free(bm);
}
