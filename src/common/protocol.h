#ifndef YAPPIE_PROTOCOL_H
#define YAPPIE_PROTOCOL_H

#include <stdint.h>

/*
 * IPC wire format: [cmd: u32] [payload_len: u32] [payload: bytes]
 * Response:        [rsp: u32] [payload_len: u32] [payload: bytes]
 * All integers are native byte order (local socket only).
 */

typedef enum {
    CMD_TOGGLE         = 1,
    CMD_STATUS         = 2,
    CMD_MODEL_LIST     = 3,
    CMD_MODEL_DOWNLOAD = 4,
    CMD_MODEL_USE      = 5,
    CMD_CONFIG         = 6,
    CMD_SHUTDOWN       = 7,
} yappie_cmd_t;

typedef enum {
    RSP_OK    = 0,
    RSP_ERROR = 1,
} yappie_rsp_t;

typedef struct {
    uint32_t type;
    uint32_t payload_len;
} yappie_msg_header_t;

#define YAPPIE_MAX_PAYLOAD (64 * 1024)
#define YAPPIE_SOCKET_NAME "yappie.sock"

#endif
