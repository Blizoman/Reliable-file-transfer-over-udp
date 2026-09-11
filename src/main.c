/**
 * @file        main.c
 * @brief       Entry point for the IPK Reliable Data Transfer application
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     This file contains the main function which initializes signal
 * handlers, parses command-line arguments, and starts either the client or
 * server mode based on the provided configuration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include "client.h"
#include "config.h"
#include "parser.h"
#include "server.h"
#include "utils.h"

int main(int argc, char *argv[]) {
  // 1. Install signal handlers (SIGINT, SIGTERM) for graceful shutdown
  // * * * *
  if (install_signal_handlers() != 0) {
    fprintf(stderr, "Error: failed to install signal handlers.\n");
    return EX_OSERR;
  }

  // 2. Parse CLI input into configuration structure
  // * * * *
  AppConfig config = parse_arguments(argc, argv);
  int exit_code = EX_SOFTWARE;

  // 3.a Run SERVER
  // * * * *
  if (config.is_server) {
    exit_code = run_server(&config);
  }

  // 3.b Run CLIENT
  // * * * *
  else if (config.is_client) {
    exit_code = run_client(&config);
  }

  return exit_code;
}