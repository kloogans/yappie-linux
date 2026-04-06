#ifndef YAPPIE_MODEL_H
#define YAPPIE_MODEL_H

#include <stddef.h>

typedef struct {
    const char *name;       /* e.g. "base.en" */
    const char *filename;   /* e.g. "ggml-base.en.bin" */
    int         size_mb;    /* approx download size */
    const char *ram;        /* recommended RAM */
    const char *desc;       /* short description */
} model_info_t;

/* Get the curated model catalog. Returns number of entries. */
int model_catalog(const model_info_t **out);

/* Find a model by name. Returns NULL if not found. */
const model_info_t *model_find(const char *name);

/* Check if a model is downloaded. Returns the full path (caller frees) or NULL. */
char *model_path(const char *model_dir, const model_info_t *m);

/* Build a formatted list of models with download status.
   Caller frees the returned string. active_model can be NULL. */
char *model_list_string(const char *model_dir, const char *active_model);

/* Download a model to model_dir. Returns 0 on success. */
int model_download(const char *model_dir, const model_info_t *m);

#endif
