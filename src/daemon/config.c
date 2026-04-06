#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <toml.h>

static char *xstrdup(const char *s) {
    return s ? strdup(s) : NULL;
}

/* Expand leading ~ to $HOME. Caller frees. */
static char *expand_tilde(const char *path) {
    if (!path || path[0] != '~')
        return xstrdup(path);
    const char *home = getenv("HOME");
    if (!home) return xstrdup(path);
    size_t hlen = strlen(home);
    size_t plen = strlen(path + 1);
    char *out = malloc(hlen + plen + 1);
    memcpy(out, home, hlen);
    memcpy(out + hlen, path + 1, plen + 1);
    return out;
}

/* Read entire file into a string. Caller frees. Returns NULL on error. */
static char *read_file_trimmed(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len <= 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    size_t n = fread(buf, 1, len, f);
    fclose(f);
    buf[n] = '\0';
    /* trim trailing whitespace/newline */
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' '))
        buf[--n] = '\0';
    return buf;
}

static backend_type_t parse_type(const char *s) {
    if (strcmp(s, "api") == 0)   return BACKEND_API;
    if (strcmp(s, "tcp") == 0)   return BACKEND_TCP;
    if (strcmp(s, "local") == 0) return BACKEND_LOCAL;
    return BACKEND_API; /* default fallback */
}

char *config_default_path(void) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        char *p = NULL;
        asprintf(&p, "%s/yappie/config.toml", xdg);
        return p;
    }
    const char *home = getenv("HOME");
    char *p = NULL;
    asprintf(&p, "%s/.config/yappie/config.toml", home ? home : "/tmp");
    return p;
}

char *config_default_model_dir(void) {
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg && xdg[0]) {
        char *p = NULL;
        asprintf(&p, "%s/yappie/models", xdg);
        return p;
    }
    const char *home = getenv("HOME");
    char *p = NULL;
    asprintf(&p, "%s/.local/share/yappie/models", home ? home : "/tmp");
    return p;
}

int config_load(const char *path, yappie_config_t *cfg, char *err, int errlen) {
    memset(cfg, 0, sizeof(*cfg));

    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(err, errlen, "No config at %s", path);
        return -1;
    }

    char toml_err[256];
    toml_table_t *root = toml_parse_file(f, toml_err, sizeof(toml_err));
    fclose(f);

    if (!root) {
        snprintf(err, errlen, "Config parse error: %s", toml_err);
        return -1;
    }

    toml_array_t *arr = toml_array_in(root, "backend");
    if (!arr) {
        snprintf(err, errlen, "No backends configured in %s", path);
        toml_free(root);
        return -1;
    }

    int n = toml_array_nelem(arr);
    if (n <= 0) {
        snprintf(err, errlen, "No backends configured in %s", path);
        toml_free(root);
        return -1;
    }

    cfg->model_dir = config_default_model_dir();

    cfg->backends = calloc(n, sizeof(backend_config_t));
    cfg->backend_count = n;

    for (int i = 0; i < n; i++) {
        toml_table_t *t = toml_table_at(arr, i);
        backend_config_t *b = &cfg->backends[i];

        /* defaults */
        b->host = strdup("127.0.0.1");
        b->port = 9876;
        b->gpu  = true;

        toml_datum_t d;

        d = toml_string_in(t, "name");
        if (d.ok) b->name = d.u.s; else b->name = strdup("");

        d = toml_string_in(t, "type");
        if (d.ok) { b->type = parse_type(d.u.s); free(d.u.s); }

        d = toml_string_in(t, "url");
        if (d.ok) {
            /* strip trailing slash */
            size_t len = strlen(d.u.s);
            if (len > 0 && d.u.s[len-1] == '/') d.u.s[len-1] = '\0';
            b->url = d.u.s;
        }

        d = toml_string_in(t, "model");
        if (d.ok) b->model = d.u.s;

        d = toml_string_in(t, "api_key");
        if (d.ok) b->api_key = d.u.s;

        d = toml_string_in(t, "api_key_file");
        if (d.ok) {
            char *expanded = expand_tilde(d.u.s);
            free(d.u.s);
            char *key = read_file_trimmed(expanded);
            free(expanded);
            if (key) {
                free(b->api_key);
                b->api_key = key;
            }
        }

        d = toml_string_in(t, "host");
        if (d.ok) { free(b->host); b->host = d.u.s; }

        d = toml_int_in(t, "port");
        if (d.ok) b->port = (int)d.u.i;

        d = toml_string_in(t, "language");
        if (d.ok) b->language = d.u.s;

        d = toml_bool_in(t, "gpu");
        if (d.ok) b->gpu = d.u.b;
    }

    toml_free(root);
    return 0;
}

void config_free(yappie_config_t *cfg) {
    for (int i = 0; i < cfg->backend_count; i++) {
        backend_config_t *b = &cfg->backends[i];
        free(b->name);
        free(b->url);
        free(b->model);
        free(b->api_key);
        free(b->host);
        free(b->language);
    }
    free(cfg->backends);
    free(cfg->model_dir);
    memset(cfg, 0, sizeof(*cfg));
}
