#include "model.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>

static const model_info_t catalog[] = {
    { "tiny",              "ggml-tiny.bin",              75,   "~1 GB",  "Fastest, lowest accuracy" },
    { "tiny.en",           "ggml-tiny.en.bin",           75,   "~1 GB",  "Fastest, English only" },
    { "base",              "ggml-base.bin",              142,  "~1 GB",  "Fast, good accuracy" },
    { "base.en",           "ggml-base.en.bin",           142,  "~1 GB",  "Fast, English only" },
    { "small",             "ggml-small.bin",             466,  "~2 GB",  "Balanced speed/accuracy" },
    { "small.en",          "ggml-small.en.bin",          466,  "~2 GB",  "Balanced, English only" },
    { "medium",            "ggml-medium.bin",            1500, "~5 GB",  "High accuracy, slower" },
    { "medium.en",         "ggml-medium.en.bin",         1500, "~5 GB",  "High accuracy, English only" },
    { "large-v3-turbo",    "ggml-large-v3-turbo.bin",    1600, "~6 GB",  "Excellent accuracy, optimized" },
    { "large-v3",          "ggml-large-v3.bin",          2900, "~10 GB", "Maximum accuracy" },
};

#define CATALOG_SIZE (sizeof(catalog) / sizeof(catalog[0]))
#define HF_BASE_URL "https://huggingface.co/ggerganov/whisper.cpp/resolve/main"

int model_catalog(const model_info_t **out) {
    *out = catalog;
    return CATALOG_SIZE;
}

const model_info_t *model_find(const char *name) {
    for (size_t i = 0; i < CATALOG_SIZE; i++) {
        if (strcmp(catalog[i].name, name) == 0)
            return &catalog[i];
    }
    return NULL;
}

char *model_path(const char *model_dir, const model_info_t *m) {
    char *path = NULL;
    asprintf(&path, "%s/%s", model_dir, m->filename);
    if (access(path, R_OK) == 0)
        return path;
    free(path);
    return NULL;
}

/* Ensure directory exists (mkdir -p equivalent) */
static int mkdirp(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

static int progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                       curl_off_t ultotal, curl_off_t ulnow) {
    (void)clientp;
    (void)ultotal;
    (void)ulnow;
    if (dltotal > 0) {
        int pct = (int)(dlnow * 100 / dltotal);
        fprintf(stderr, "\rdownloading: %d%%", pct);
    }
    return 0;
}

int model_download(const char *model_dir, const model_info_t *m) {
    if (mkdirp(model_dir) < 0) {
        fprintf(stderr, "model: failed to create %s\n", model_dir);
        return -1;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/%s", HF_BASE_URL, m->filename);

    char dest[1024];
    snprintf(dest, sizeof(dest), "%s/%s", model_dir, m->filename);

    /* Download to temp file, then rename */
    char tmp[1040];
    snprintf(tmp, sizeof(tmp), "%s.part", dest);

    FILE *f = fopen(tmp, "wb");
    if (!f) {
        fprintf(stderr, "model: cannot create %s\n", tmp);
        return -1;
    }

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    fprintf(stderr, "Downloading %s (%d MB)...\n", m->name, m->size_mb);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);
    fprintf(stderr, "\n");

    if (res != CURLE_OK) {
        fprintf(stderr, "model: download failed: %s\n", curl_easy_strerror(res));
        unlink(tmp);
        return -1;
    }

    if (rename(tmp, dest) < 0) {
        perror("model: rename");
        unlink(tmp);
        return -1;
    }

    fprintf(stderr, "model: saved to %s\n", dest);
    return 0;
}

char *model_list_string(const char *model_dir, const char *active_model) {
    char buf[8192];
    int off = 0;

    off += snprintf(buf + off, sizeof(buf) - off,
                    "%-20s %6s  %-8s  %s\n", "MODEL", "SIZE", "RAM", "STATUS");
    off += snprintf(buf + off, sizeof(buf) - off,
                    "%-20s %6s  %-8s  %s\n", "-----", "----", "---", "------");

    for (size_t i = 0; i < CATALOG_SIZE && off < (int)sizeof(buf) - 128; i++) {
        const model_info_t *m = &catalog[i];
        char *path = model_path(model_dir, m);
        const char *status;
        if (path) {
            if (active_model && strcmp(active_model, m->name) == 0)
                status = "active";
            else
                status = "downloaded";
            free(path);
        } else {
            status = "-";
        }

        char size_str[16];
        if (m->size_mb >= 1000)
            snprintf(size_str, sizeof(size_str), "%.1fGB", m->size_mb / 1000.0);
        else
            snprintf(size_str, sizeof(size_str), "%dMB", m->size_mb);

        off += snprintf(buf + off, sizeof(buf) - off,
                        "%-20s %6s  %-8s  %s\n",
                        m->name, size_str, m->ram, status);
    }

    return strdup(buf);
}
