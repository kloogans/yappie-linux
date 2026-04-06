#ifndef YAPPIE_CONFIG_H
#define YAPPIE_CONFIG_H

#include <stdbool.h>

typedef enum {
    BACKEND_LOCAL,
    BACKEND_API,
    BACKEND_TCP,
} backend_type_t;

typedef struct {
    char *name;
    backend_type_t type;
    /* API fields */
    char *url;
    char *model;
    char *api_key;      /* resolved (inline or read from file) */
    /* TCP fields */
    char *host;
    int   port;
    /* Local fields */
    char *language;     /* NULL = auto-detect */
    bool  gpu;
} backend_config_t;

typedef struct {
    backend_config_t *backends;
    int               backend_count;
    char             *model_dir;   /* ~/.local/share/yappie/models */
} yappie_config_t;

/* Load config from path.  Returns 0 on success, -1 on error (msg written to err). */
int config_load(const char *path, yappie_config_t *cfg, char *err, int errlen);

/* Free all memory owned by cfg. */
void config_free(yappie_config_t *cfg);

/* Return the default config file path (allocated). */
char *config_default_path(void);

/* Return the default model directory path (allocated). */
char *config_default_model_dir(void);

#endif
