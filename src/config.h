/**
 * @file        config.h
 * @brief       Configuration structures for the IPK Reliable Data Transfer
 * protocol
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     This file defines the main configuration structure used to store
 * parsed command-line arguments for both the client and server modes of the
 * application.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

/**
 * @struct AppConfig
 * @brief Holds the application configuration parsed from CLI arguments.
 */
typedef struct {
  bool is_server;    // -s
  bool is_client;    // -c
  char *address;     // -a ADDRESS / HOST
  char *input_file;  // -i INPUT
  char *output_file; // -o OUTPUT
  int timeout;       // -w TIMEOUT (def 1 (ms?))
  int port;          // -p PORT (max num portov (def -1))
} AppConfig;

#endif