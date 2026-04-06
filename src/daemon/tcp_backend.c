#include "backend.h"
#include "wav.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

static int tcp_init(backend_t *b, const backend_config_t *cfg) {
    (void)b;
    (void)cfg;
    return 0;
}

static char *tcp_transcribe(backend_t *b, const float *samples, size_t n_samples) {
    const backend_config_t *cfg = &b->cfg;

    /* Encode WAV in memory */
    uint8_t *wav_buf = NULL;
    size_t wav_size = 0;
    if (wav_encode(samples, n_samples, &wav_buf, &wav_size) < 0)
        return NULL;

    /* Resolve host */
    struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM };
    struct addrinfo *res = NULL;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", cfg->port);

    if (getaddrinfo(cfg->host, port_str, &hints, &res) != 0 || !res) {
        free(wav_buf);
        return NULL;
    }

    int fd = socket(res->ai_family, SOCK_STREAM, 0);
    if (fd < 0) {
        freeaddrinfo(res);
        free(wav_buf);
        return NULL;
    }

    /* 5 second timeout for connect and I/O */
    struct timeval tv = { .tv_sec = 5 };
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd);
        freeaddrinfo(res);
        free(wav_buf);
        return NULL;
    }
    freeaddrinfo(res);

    /* Send WAV data */
    size_t sent = 0;
    while (sent < wav_size) {
        ssize_t n = write(fd, wav_buf + sent, wav_size - sent);
        if (n <= 0) { close(fd); free(wav_buf); return NULL; }
        sent += n;
    }
    free(wav_buf);
    shutdown(fd, SHUT_WR);

    /* Read response */
    char buf[65536];
    size_t total = 0;
    while (total < sizeof(buf) - 1) {
        ssize_t n = read(fd, buf + total, sizeof(buf) - 1 - total);
        if (n <= 0) break;
        total += n;
    }
    close(fd);
    buf[total] = '\0';

    /* Check for ERROR: prefix */
    if (total >= 6 && strncmp(buf, "ERROR:", 6) == 0)
        return NULL;

    if (total == 0)
        return NULL;

    return strdup(buf);
}

static void tcp_destroy(backend_t *b) {
    (void)b;
}

const backend_ops_t tcp_backend_ops = {
    .name = "tcp",
    .init = tcp_init,
    .transcribe = tcp_transcribe,
    .destroy = tcp_destroy,
};
