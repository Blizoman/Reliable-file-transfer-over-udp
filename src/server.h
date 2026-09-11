/**
 * @file        server.h
 * @brief       Server definitions for the IPK Reliable Data Transfer
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     Provides the server state machine enumeration and function
 * prototypes necessary for establishing the connection and receiving data.
 */

#ifndef SERVER_H
#define SERVER_H

#include "config.h"
#include "utils.h"

#define MAX_PORT_LENGTH_IN_SIGNS 6
#define MODULER 10000

/**
 * @enum ServerState
 * @brief Represents the current state of the server FSM.
 */
typedef enum {
  SERVER_LISTEN,
  SERVER_HANDSHAKE,
  SERVER_ESTABLISHED,
  SERVER_LAST_ACK,
  SERVER_DONE
} ServerState;

/**
 * @brief Create and bind a UDP socket for the server.
 * @param config Runtime configuration (bind address, port).
 * @return Socket file descriptor on success, negative on error.
 */
int setup_server_socket(AppConfig *config);

/**
 * @brief Run the server state machine.
 * @param config Runtime configuration.
 * @return Exit code (0 on success, non-zero on failure).
 */
int run_server(AppConfig *config);

#endif