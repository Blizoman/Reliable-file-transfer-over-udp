/**
 * @file        client.h
 * @brief       Client definitions for the IPK Reliable Data Transfer
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     Defines the client state machine states and public function
 *              prototypes for the RDT client implementation.
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <netdb.h>
#include <sys/socket.h>

#include "config.h"

#define MAX_PORT_LENGTH_IN_SIGNS 6
#define MODULER 10000

/**
 * @enum ClientState
 * @brief Represents the current state of the client FSM.
 */
typedef enum {
  CLIENT_CLOSED,
  CLIENT_SYN_SENT,
  CLIENT_ESTABLISHED,
  CLIENT_FIN_WAIT,
  CLIENT_TIME_WAIT,
  CLIENT_DONE
} ClientState;

/**
 * @brief Resolve the server address and create a UDP socket for the client.
 * @param config Runtime configuration (host, port).
 * @param server_addr Output address storage for the server.
 * @param server_addr_len Output length of the resolved address.
 * @return Socket file descriptor on success, negative on error.
 */
int setup_client_socket(AppConfig *config, struct sockaddr_storage *server_addr,
                        socklen_t *server_addr_len);

/**
 * @brief Run the client state machine.
 * @param config Runtime configuration.
 * @return Exit code (0 on success, non-zero on failure).
 */
int run_client(AppConfig *config);

#endif