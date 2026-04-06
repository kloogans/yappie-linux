#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "version.h"
#include "protocol.h"
#include "ipc.h"

static void usage(void) {
    printf("Usage: yappie <command> [args]\n\n");
    printf("Commands:\n");
    printf("  toggle              Start/stop recording\n");
    printf("  status              Show daemon status\n");
    printf("  model list          List available models\n");
    printf("  model download <n>  Download a model\n");
    printf("  model use <name>    Switch active model\n");
    printf("  config              Show current config\n");
    printf("  shutdown            Stop the daemon\n");
    printf("  --version, -v       Print version\n");
}

static int send_cmd(yappie_cmd_t cmd, const char *payload) {
    char *sock = ipc_default_socket_path();
    yappie_rsp_t rsp_type;
    char *rsp_payload = NULL;
    uint32_t rsp_len = 0;
    uint32_t payload_len = payload ? strlen(payload) : 0;

    int rc = ipc_client_send(sock, cmd, payload, payload_len,
                             &rsp_type, &rsp_payload, &rsp_len);
    free(sock);

    if (rc < 0) {
        fprintf(stderr, "yappie: cannot connect to daemon (is yappied running?)\n");
        return 1;
    }

    if (rsp_payload && rsp_len > 0) {
        if (rsp_type == RSP_ERROR)
            fprintf(stderr, "error: %s\n", rsp_payload);
        else
            printf("%s\n", rsp_payload);
    }

    free(rsp_payload);
    return (rsp_type == RSP_ERROR) ? 1 : 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage();
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        printf("yappie %s\n", YAPPIE_VERSION);
        return 0;
    }
    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        usage();
        return 0;
    }

    if (strcmp(cmd, "toggle") == 0)
        return send_cmd(CMD_TOGGLE, NULL);

    if (strcmp(cmd, "status") == 0)
        return send_cmd(CMD_STATUS, NULL);

    if (strcmp(cmd, "config") == 0)
        return send_cmd(CMD_CONFIG, NULL);

    if (strcmp(cmd, "shutdown") == 0)
        return send_cmd(CMD_SHUTDOWN, NULL);

    if (strcmp(cmd, "model") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: yappie model <list|download|use> [name]\n");
            return 1;
        }
        const char *sub = argv[2];
        if (strcmp(sub, "list") == 0)
            return send_cmd(CMD_MODEL_LIST, NULL);
        if (strcmp(sub, "download") == 0) {
            if (argc < 4) { fprintf(stderr, "Usage: yappie model download <name>\n"); return 1; }
            return send_cmd(CMD_MODEL_DOWNLOAD, argv[3]);
        }
        if (strcmp(sub, "use") == 0) {
            if (argc < 4) { fprintf(stderr, "Usage: yappie model use <name>\n"); return 1; }
            return send_cmd(CMD_MODEL_USE, argv[3]);
        }
        fprintf(stderr, "Unknown model command: %s\n", sub);
        return 1;
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    usage();
    return 1;
}
