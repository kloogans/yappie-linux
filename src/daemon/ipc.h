#ifndef YAPPIE_IPC_H
#define YAPPIE_IPC_H

#include "protocol.h"

/* Callback invoked when a command arrives.
   Must fill rsp_type and (optionally) rsp_payload (caller frees).
   Return 0 to continue, -1 to shut down the daemon. */
typedef int (*ipc_handler_fn)(yappie_cmd_t cmd,
                              const char *payload, uint32_t payload_len,
                              yappie_rsp_t *rsp_type,
                              char **rsp_payload, uint32_t *rsp_len,
                              void *userdata);

/* Create the listening Unix socket. Returns fd >= 0, or -1 on error. */
int ipc_server_create(const char *socket_path);

/* Handle one client connection (blocking read/write, then close).
   Returns the handler's return value. */
int ipc_server_handle_client(int client_fd, ipc_handler_fn handler, void *userdata);

/* Clean up stale socket file if no daemon is listening. */
void ipc_cleanup_stale(const char *socket_path);

/* Build the default socket path. Caller frees. */
char *ipc_default_socket_path(void);

/* --- Client side (used by CLI) --- */

/* Connect to daemon, send a command, receive response.
   Returns 0 on success. rsp_payload is allocated (caller frees). */
int ipc_client_send(const char *socket_path,
                    yappie_cmd_t cmd,
                    const char *payload, uint32_t payload_len,
                    yappie_rsp_t *rsp_type,
                    char **rsp_payload, uint32_t *rsp_len);

#endif
