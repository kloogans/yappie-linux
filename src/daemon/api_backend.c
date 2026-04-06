#include "backend.h"
#include "wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cJSON.h>

typedef struct {
    CURL *curl;
} api_priv_t;

static int api_init(backend_t *b, const backend_config_t *cfg) {
    (void)cfg;
    api_priv_t *p = calloc(1, sizeof(*p));
    p->curl = curl_easy_init();
    if (!p->curl) { free(p); return -1; }
    b->priv = p;
    return 0;
}

/* curl write callback — accumulate response into a buffer */
typedef struct {
    char  *data;
    size_t len;
} response_buf_t;

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    response_buf_t *r = userdata;
    size_t total = size * nmemb;
    r->data = realloc(r->data, r->len + total + 1);
    memcpy(r->data + r->len, ptr, total);
    r->len += total;
    r->data[r->len] = '\0';
    return total;
}

static char *api_transcribe(backend_t *b, const float *samples, size_t n_samples) {
    api_priv_t *p = b->priv;
    const backend_config_t *cfg = &b->cfg;

    /* Encode WAV in memory */
    uint8_t *wav_buf = NULL;
    size_t wav_size = 0;
    if (wav_encode(samples, n_samples, &wav_buf, &wav_size) < 0)
        return NULL;

    /* Build URL */
    char url[1024];
    snprintf(url, sizeof(url), "%s/audio/transcriptions",
             cfg->url ? cfg->url : "");

    /* Build multipart form */
    curl_mime *mime = curl_mime_init(p->curl);

    curl_mimepart *file_part = curl_mime_addpart(mime);
    curl_mime_name(file_part, "file");
    curl_mime_data(file_part, (const char *)wav_buf, wav_size);
    curl_mime_filename(file_part, "audio.wav");
    curl_mime_type(file_part, "audio/wav");

    if (cfg->model && cfg->model[0]) {
        curl_mimepart *model_part = curl_mime_addpart(mime);
        curl_mime_name(model_part, "model");
        curl_mime_data(model_part, cfg->model, CURL_ZERO_TERMINATED);
    }

    /* Headers */
    struct curl_slist *headers = NULL;
    if (cfg->api_key && cfg->api_key[0]) {
        char auth[512];
        snprintf(auth, sizeof(auth), "Authorization: Bearer %s", cfg->api_key);
        headers = curl_slist_append(headers, auth);
    }

    /* Response buffer */
    response_buf_t resp = { .data = NULL, .len = 0 };

    curl_easy_setopt(p->curl, CURLOPT_URL, url);
    curl_easy_setopt(p->curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(p->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(p->curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(p->curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(p->curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(p->curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(p->curl, CURLOPT_FAILONERROR, 1L);

    CURLcode res = curl_easy_perform(p->curl);
    curl_easy_reset(p->curl);
    curl_mime_free(mime);
    curl_slist_free_all(headers);
    free(wav_buf);

    if (res != CURLE_OK) {
        free(resp.data);
        return NULL;
    }

    /* Parse JSON response — extract .text */
    char *result = NULL;
    if (resp.data) {
        cJSON *json = cJSON_Parse(resp.data);
        if (json) {
            cJSON *text = cJSON_GetObjectItemCaseSensitive(json, "text");
            if (cJSON_IsString(text) && text->valuestring[0])
                result = strdup(text->valuestring);
            cJSON_Delete(json);
        }
        /* Fallback: if no JSON .text, use raw response */
        if (!result && resp.data[0])
            result = strdup(resp.data);
    }
    free(resp.data);
    return result;
}

static void api_destroy(backend_t *b) {
    api_priv_t *p = b->priv;
    if (p) {
        curl_easy_cleanup(p->curl);
        free(p);
    }
}

const backend_ops_t api_backend_ops = {
    .name = "api",
    .init = api_init,
    .transcribe = api_transcribe,
    .destroy = api_destroy,
};
