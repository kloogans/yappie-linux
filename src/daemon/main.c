#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include <curl/curl.h>
#include <pipewire/pipewire.h>
#include <spa/utils/result.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "version.h"
#include "audio.h"
#include "backend.h"
#include "config.h"
#include "ipc.h"
#include "model.h"
#include "paste.h"
#include "state.h"

#define MIN_SAMPLES (YAPPIE_SAMPLE_RATE / 2)  /* 0.5s minimum recording */

#define SET_RSP(pp, lp, s) do { *(pp) = strdup(s); *(lp) = strlen(*(pp)); } while(0)
#define SET_RSP_ERR(tp, pp, lp, s) do { *(tp) = RSP_ERROR; SET_RSP(pp, lp, s); } while(0)

/* ---- Worker thread infrastructure ----
 *
 * Heavy operations (transcription, paste, model download) run on a worker
 * thread so the PipeWire event loop stays responsive. A pipe fd registered
 * with pw_loop notifies the main loop when the worker finishes, and the
 * completion callback runs on the main thread.
 */

typedef void (*worker_fn)(void *arg);
typedef void (*worker_done_fn)(void *arg);

typedef struct {
    worker_fn      work;       /* runs on worker thread */
    worker_done_fn done;       /* runs on main thread after work completes */
    void          *arg;        /* shared argument (worker owns lifetime) */
    int            done_fd;    /* write end of pipe — signals main loop */
} worker_ctx_t;

typedef struct {
    yappie_config_t      config;
    yappie_state_t       state;
    char                *socket_path;
    int                  ipc_fd;
    struct pw_main_loop *loop;
    audio_capture_t     *audio;
    backend_manager_t   *backends;
    compositor_t         compositor;
    char                *wclass;
    int                  worker_pipe[2]; /* [0]=read (main loop), [1]=write (worker) */
    worker_ctx_t        *pending_worker; /* current in-flight worker, or NULL */
} daemon_ctx_t;

static daemon_ctx_t ctx;

/* ---- Worker thread ---- */

static void *worker_thread(void *arg) {
    worker_ctx_t *w = arg;
    w->work(w->arg);
    /* Signal main loop that we're done */
    char c = 1;
    (void)write(w->done_fd, &c, 1);
    return NULL;
}

static void on_worker_done(void *data, int fd, uint32_t mask) {
    daemon_ctx_t *d = data;
    (void)mask;

    /* Drain the pipe */
    char c;
    (void)read(fd, &c, 1);

    worker_ctx_t *w = d->pending_worker;
    if (!w) return;
    d->pending_worker = NULL;

    if (w->done)
        w->done(w->arg);

    free(w);
}

static int worker_start(daemon_ctx_t *d, worker_fn work, worker_done_fn done, void *arg) {
    if (d->pending_worker) {
        fprintf(stderr, "yappied: worker already running, rejecting\n");
        return -1;
    }

    worker_ctx_t *w = calloc(1, sizeof(*w));
    w->work = work;
    w->done = done;
    w->arg = arg;
    w->done_fd = d->worker_pipe[1];
    d->pending_worker = w;

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int rc = pthread_create(&tid, &attr, worker_thread, w);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        fprintf(stderr, "yappied: failed to create worker thread\n");
        d->pending_worker = NULL;
        free(w);
        return -1;
    }

    return 0;
}

/* ---- Transcription worker ---- */

typedef struct {
    daemon_ctx_t *daemon;
    float        *samples;
    size_t        n_samples;
    char         *wclass;     /* owned — freed after paste */
} transcribe_job_t;

static void transcribe_work(void *arg) {
    transcribe_job_t *job = arg;
    daemon_ctx_t *d = job->daemon;

    fprintf(stderr, "yappied: captured %zu samples (%.1fs)\n",
            job->n_samples, (double)job->n_samples / YAPPIE_SAMPLE_RATE);

    if (job->n_samples < MIN_SAMPLES) {
        notify("Dictation", "No speech detected", 2000, 0);
        free(job->samples);
        job->samples = NULL;
        return;
    }

    char *text = backend_manager_transcribe(d->backends, job->samples, job->n_samples);
    free(job->samples);
    job->samples = NULL;

    if (text && text[0]) {
        paste_text(text, job->wclass);
        free(text);
    } else {
        notify("Dictation", "No speech detected", 2000, 0);
        free(text);
    }
}

static void transcribe_done(void *arg) {
    transcribe_job_t *job = arg;
    daemon_ctx_t *d = job->daemon;

    free(job->wclass);
    free(job->samples); /* in case work was skipped */
    free(job);

    d->state = STATE_IDLE;
    fprintf(stderr, "yappied: ready\n");
}

/* ---- Model download worker ---- */

typedef struct {
    daemon_ctx_t      *daemon;
    const model_info_t *model;
    char               *model_dir;
    int                 result;
} download_job_t;

static void download_work(void *arg) {
    download_job_t *job = arg;
    job->result = model_download(job->model_dir, job->model);
}

static void download_done(void *arg) {
    download_job_t *job = arg;
    if (job->result == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Model '%s' downloaded", job->model->name);
        notify("Dictation", msg, 3000, 0);
        fprintf(stderr, "yappied: %s\n", msg);
    } else {
        notify("Dictation", "Model download failed", 3000, 1);
        fprintf(stderr, "yappied: model download failed\n");
    }
    free(job->model_dir);
    free(job);
}

/* ---- Model swap worker ---- */

typedef struct {
    daemon_ctx_t *daemon;
    char         *model_name;
    char         *model_dir;
    int           result;
} swap_job_t;

static void swap_work(void *arg) {
    swap_job_t *job = arg;
    job->result = backend_manager_swap_local_model(
        job->daemon->backends, job->model_name, job->model_dir);
}

static void swap_done(void *arg) {
    swap_job_t *job = arg;
    if (job->result == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Switched to model '%s'", job->model_name);
        notify("Dictation", msg, 3000, 0);
        fprintf(stderr, "yappied: %s\n", msg);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to load model '%s'", job->model_name);
        notify("Dictation", msg, 3000, 1);
        fprintf(stderr, "yappied: %s\n", msg);
    }
    free(job->model_name);
    free(job->model_dir);
    free(job);
}

/* ---- IPC command handler ---- */

static int on_command(yappie_cmd_t cmd,
                      const char *payload, uint32_t payload_len,
                      yappie_rsp_t *rsp_type, char **rsp_payload, uint32_t *rsp_len,
                      void *userdata) {
    daemon_ctx_t *d = userdata;

    switch (cmd) {
    case CMD_TOGGLE: {
        if (state_toggle(&d->state) < 0) {
            SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, "busy (transcribing)");
            return 0;
        }

        if (d->state == STATE_RECORDING) {
            free(d->wclass);
            d->wclass = paste_get_window_class(d->compositor);

            if (audio_capture_start(d->audio) < 0) {
                SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, "failed to start recording");
                d->state = STATE_IDLE;
                return 0;
            }
            notify("Dictation", "Recording...", 1500, 0);
            SET_RSP(rsp_payload, rsp_len, "recording");
        } else if (d->state == STATE_TRANSCRIBING) {
            float *samples = NULL;
            size_t n_samples = 0;
            audio_capture_stop(d->audio, &samples, &n_samples);

            transcribe_job_t *job = calloc(1, sizeof(*job));
            job->daemon = d;
            job->samples = samples;
            job->n_samples = n_samples;
            job->wclass = d->wclass;
            d->wclass = NULL; /* ownership transferred to job */

            if (worker_start(d, transcribe_work, transcribe_done, job) < 0) {
                /* Fallback: run synchronously if thread creation fails */
                transcribe_work(job);
                transcribe_done(job);
            }

            SET_RSP(rsp_payload, rsp_len, "transcribing");
        }
        return 0;
    }

    case CMD_STATUS:
        SET_RSP(rsp_payload, rsp_len, state_to_string(d->state));
        return 0;

    case CMD_CONFIG: {
        char buf[4096];
        int off = 0;
        for (int i = 0; i < d->config.backend_count && off < (int)sizeof(buf) - 128; i++) {
            backend_config_t *b = &d->config.backends[i];
            const char *tname = b->type == BACKEND_LOCAL ? "local" :
                                b->type == BACKEND_API   ? "api"   : "tcp";
            off += snprintf(buf + off, sizeof(buf) - off,
                           "[%d] %s (%s)", i + 1, b->name ? b->name : "", tname);
            if (b->type == BACKEND_API && b->url)
                off += snprintf(buf + off, sizeof(buf) - off, " url=%s", b->url);
            if (b->type == BACKEND_TCP)
                off += snprintf(buf + off, sizeof(buf) - off, " %s:%d", b->host, b->port);
            if (b->type == BACKEND_LOCAL && b->model)
                off += snprintf(buf + off, sizeof(buf) - off, " model=%s", b->model);
            off += snprintf(buf + off, sizeof(buf) - off, "\n");
        }
        SET_RSP(rsp_payload, rsp_len, buf);
        return 0;
    }

    case CMD_SHUTDOWN:
        SET_RSP(rsp_payload, rsp_len, "shutting down");
        return -1;

    case CMD_MODEL_LIST: {
        const char *active = NULL;
        for (int i = 0; i < d->config.backend_count; i++) {
            if (d->config.backends[i].type == BACKEND_LOCAL) {
                active = d->config.backends[i].model;
                break;
            }
        }
        *rsp_payload = model_list_string(d->config.model_dir, active);
        *rsp_len = *rsp_payload ? strlen(*rsp_payload) : 0;
        return 0;
    }

    case CMD_MODEL_DOWNLOAD: {
        if (!payload || payload_len == 0) {
            SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, "usage: yappie model download <name>");
            return 0;
        }
        const model_info_t *m = model_find(payload);
        if (!m) {
            char msg[256];
            snprintf(msg, sizeof(msg), "unknown model: %s", payload);
            SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, msg);
            return 0;
        }
        if (d->pending_worker) {
            SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, "busy (worker in progress)");
            return 0;
        }

        download_job_t *job = calloc(1, sizeof(*job));
        job->daemon = d;
        job->model = m;
        job->model_dir = strdup(d->config.model_dir);

        if (worker_start(d, download_work, download_done, job) < 0) {
            free(job->model_dir);
            free(job);
            SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, "failed to start download");
            return 0;
        }

        char msg[256];
        snprintf(msg, sizeof(msg), "downloading %s (%d MB) in background...",
                 m->name, m->size_mb);
        SET_RSP(rsp_payload, rsp_len, msg);
        return 0;
    }

    case CMD_MODEL_USE: {
        if (!payload || payload_len == 0) {
            SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, "usage: yappie model use <name>");
            return 0;
        }
        if (d->state != STATE_IDLE) {
            SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, "cannot swap model while recording/transcribing");
            return 0;
        }
        if (d->pending_worker) {
            SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, "busy (worker in progress)");
            return 0;
        }

        /* Verify model file exists before starting the swap */
        const model_info_t *m = model_find(payload);
        if (m) {
            char *path = model_path(d->config.model_dir, m);
            if (!path) {
                char msg[256];
                snprintf(msg, sizeof(msg), "model '%s' not downloaded — run: yappie model download %s",
                         payload, payload);
                SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, msg);
                return 0;
            }
            free(path);
        }
        /* If not in catalog, allow it — could be a custom model file */

        swap_job_t *job = calloc(1, sizeof(*job));
        job->daemon = d;
        job->model_name = strdup(payload);
        job->model_dir = strdup(d->config.model_dir);

        if (worker_start(d, swap_work, swap_done, job) < 0) {
            free(job->model_name);
            free(job->model_dir);
            free(job);
            SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, "failed to start model swap");
            return 0;
        }

        char msg[256];
        snprintf(msg, sizeof(msg), "loading model '%s'...", payload);
        SET_RSP(rsp_payload, rsp_len, msg);
        return 0;
    }

    default:
        SET_RSP_ERR(rsp_type, rsp_payload, rsp_len, "unknown command");
        return 0;
    }
}

static void on_ipc_readable(void *data, int fd, uint32_t mask) {
    daemon_ctx_t *d = data;
    (void)mask;

    int client = accept(fd, NULL, NULL);
    if (client < 0) return;

    int ret = ipc_server_handle_client(client, on_command, d);
    if (ret < 0)
        pw_main_loop_quit(d->loop);
}

static void on_signal(void *data, int signal_number) {
    daemon_ctx_t *d = data;
    (void)signal_number;
    fprintf(stderr, "yappied: caught signal, shutting down\n");
    pw_main_loop_quit(d->loop);
}

int main(int argc, char *argv[]) {
    if (argc > 1 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
        printf("yappied %s\n", YAPPIE_VERSION);
        return 0;
    }
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printf("Usage: yappied [OPTIONS]\n");
        printf("  -v, --version   Print version and exit\n");
        printf("  -h, --help      Print this help and exit\n");
        return 0;
    }

    pw_init(&argc, &argv);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    memset(&ctx, 0, sizeof(ctx));
    ctx.state = STATE_IDLE;

    /* Worker pipe: worker thread writes to [1], main loop reads [0] */
    if (pipe(ctx.worker_pipe) < 0) {
        perror("yappied: pipe");
        return 1;
    }

    ctx.compositor = paste_detect_compositor();
    fprintf(stderr, "yappied: compositor=%s\n",
            ctx.compositor == WM_HYPRLAND ? "hyprland" :
            ctx.compositor == WM_SWAY     ? "sway"     : "generic");

    char *config_path = config_default_path();
    char err[256];
    if (config_load(config_path, &ctx.config, err, sizeof(err)) < 0) {
        fprintf(stderr, "yappied: %s\n", err);
        notify("Dictation", err, 5000, 1);
        free(config_path);
        return 1;
    }
    fprintf(stderr, "yappied: loaded %d backend(s) from %s\n",
            ctx.config.backend_count, config_path);
    free(config_path);

    ctx.backends = backend_manager_create(&ctx.config);
    if (!ctx.backends || ctx.backends->count == 0) {
        fprintf(stderr, "yappied: no usable backends configured\n");
        notify("Dictation", "No usable backends configured", 5000, 1);
        config_free(&ctx.config);
        return 1;
    }
    fprintf(stderr, "yappied: initialized %d backend(s)\n", ctx.backends->count);

    ctx.loop = pw_main_loop_new(NULL);
    if (!ctx.loop) {
        fprintf(stderr, "yappied: failed to create PipeWire main loop\n");
        backend_manager_destroy(ctx.backends);
        config_free(&ctx.config);
        return 1;
    }

    struct pw_loop *loop = pw_main_loop_get_loop(ctx.loop);

    ctx.audio = audio_capture_create(loop);
    if (!ctx.audio) {
        fprintf(stderr, "yappied: failed to create audio capture\n");
        pw_main_loop_destroy(ctx.loop);
        backend_manager_destroy(ctx.backends);
        config_free(&ctx.config);
        return 1;
    }

    pw_loop_add_signal(loop, SIGINT, on_signal, &ctx);
    pw_loop_add_signal(loop, SIGTERM, on_signal, &ctx);

    /* Register worker pipe with event loop */
    pw_loop_add_io(loop, ctx.worker_pipe[0], SPA_IO_IN, false, on_worker_done, &ctx);

    ctx.socket_path = ipc_default_socket_path();
    ctx.ipc_fd = ipc_server_create(ctx.socket_path);
    if (ctx.ipc_fd < 0) {
        fprintf(stderr, "yappied: failed to create IPC socket at %s\n", ctx.socket_path);
        audio_capture_destroy(ctx.audio);
        pw_main_loop_destroy(ctx.loop);
        backend_manager_destroy(ctx.backends);
        config_free(&ctx.config);
        free(ctx.socket_path);
        return 1;
    }

    pw_loop_add_io(loop, ctx.ipc_fd, SPA_IO_IN, false, on_ipc_readable, &ctx);

    fprintf(stderr, "yappied %s ready (socket: %s)\n", YAPPIE_VERSION, ctx.socket_path);

#ifdef HAVE_SYSTEMD
    sd_notify(0, "READY=1\nSTATUS=Idle");
#endif

    pw_main_loop_run(ctx.loop);

    close(ctx.ipc_fd);
    unlink(ctx.socket_path);
    close(ctx.worker_pipe[0]);
    close(ctx.worker_pipe[1]);
    audio_capture_destroy(ctx.audio);
    backend_manager_destroy(ctx.backends);
    pw_main_loop_destroy(ctx.loop);
    config_free(&ctx.config);
    free(ctx.socket_path);
    free(ctx.wclass);
    curl_global_cleanup();
    pw_deinit();

    fprintf(stderr, "yappied: stopped\n");
    return 0;
}
