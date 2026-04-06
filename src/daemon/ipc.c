#include "ipc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

char *ipc_default_socket_path(void) {
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (!runtime) runtime = "/tmp";
    char *p = NULL;
    asprintf(&p, "%s/%s", runtime, YAPPIE_SOCKET_NAME);
    return p;
}

void ipc_cleanup_stale(const char *socket_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        /* Connection refused — stale socket, remove it */
        unlink(socket_path);
    }
    close(fd);
}

int ipc_server_create(const char *socket_path) {
    ipc_cleanup_stale(socket_path);

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        perror("ipc: socket");
        return -1;
    }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);
    unlink(socket_path); /* remove any leftover */

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("ipc: bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 4) < 0) {
        perror("ipc: listen");
        close(fd);
        return -1;
    }

    return fd;
}

/* Read exactly n bytes. Returns 0 on success, -1 on error/EOF. */
static int read_exact(int fd, void *buf, size_t n) {
    size_t done = 0;
    while (done < n) {
        ssize_t r = read(fd, (char *)buf + done, n - done);
        if (r <= 0) return -1;
        done += r;
    }
    return 0;
}

/* Write exactly n bytes. */
static int write_exact(int fd, const void *buf, size_t n) {
    size_t done = 0;
    while (done < n) {
        ssize_t w = write(fd, (const char *)buf + done, n - done);
        if (w <= 0) return -1;
        done += w;
    }
    return 0;
}

int ipc_server_handle_client(int client_fd, ipc_handler_fn handler, void *userdata) {
    int ret = 0;

    /* Read header */
    yappie_msg_header_t hdr;
    if (read_exact(client_fd, &hdr, sizeof(hdr)) < 0)
        goto done;

    if (hdr.payload_len > YAPPIE_MAX_PAYLOAD)
        goto done;

    /* Read payload */
    char *payload = NULL;
    if (hdr.payload_len > 0) {
        payload = malloc(hdr.payload_len + 1);
        if (read_exact(client_fd, payload, hdr.payload_len) < 0) {
            free(payload);
            goto done;
        }
        payload[hdr.payload_len] = '\0';
    }

    /* Dispatch */
    yappie_rsp_t rsp_type = RSP_OK;
    char *rsp_payload = NULL;
    uint32_t rsp_len = 0;

    ret = handler((yappie_cmd_t)hdr.type, payload, hdr.payload_len,
                  &rsp_type, &rsp_payload, &rsp_len, userdata);

    /* Send response */
    yappie_msg_header_t rsp_hdr = { .type = rsp_type, .payload_len = rsp_len };
    write_exact(client_fd, &rsp_hdr, sizeof(rsp_hdr));
    if (rsp_len > 0 && rsp_payload)
        write_exact(client_fd, rsp_payload, rsp_len);

    free(rsp_payload);
    free(payload);
done:
    close(client_fd);
    return ret;
}

/* --- Client side --- */

int ipc_client_send(const char *socket_path,
                    yappie_cmd_t cmd,
                    const char *payload, uint32_t payload_len,
                    yappie_rsp_t *rsp_type,
                    char **rsp_payload, uint32_t *rsp_len) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    /* 5 second timeout */
    struct timeval tv = { .tv_sec = 5 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    /* Send command */
    yappie_msg_header_t hdr = { .type = cmd, .payload_len = payload_len };
    if (write_exact(fd, &hdr, sizeof(hdr)) < 0) { close(fd); return -1; }
    if (payload_len > 0 && payload)
        if (write_exact(fd, payload, payload_len) < 0) { close(fd); return -1; }

    /* Read response */
    yappie_msg_header_t rsp_hdr;
    if (read_exact(fd, &rsp_hdr, sizeof(rsp_hdr)) < 0) { close(fd); return -1; }

    *rsp_type = (yappie_rsp_t)rsp_hdr.type;
    *rsp_len = rsp_hdr.payload_len;
    *rsp_payload = NULL;

    if (rsp_hdr.payload_len > 0 && rsp_hdr.payload_len <= YAPPIE_MAX_PAYLOAD) {
        *rsp_payload = malloc(rsp_hdr.payload_len + 1);
        if (read_exact(fd, *rsp_payload, rsp_hdr.payload_len) < 0) {
            free(*rsp_payload);
            *rsp_payload = NULL;
            close(fd);
            return -1;
        }
        (*rsp_payload)[rsp_hdr.payload_len] = '\0';
    }

    close(fd);
    return 0;
}
