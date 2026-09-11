/**
 * @file        parser.c
 * @brief       Command-line argument parsing implementation
 * @author      Andrej Bližnák (xblizna00)
 * @date        2026
 * @details     Implements parsing logic using getopt_long, validates all inputs
 *              according to the assignment specifications, and handles errors.
 */

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include "parser.h"

long parse_number(char *str_value, char *arg_name) {
  char *endptr;

  // Reset errno before conversion
  // * * * *
  errno = 0;
  long value = strtol(str_value, &endptr, 10);

  // Check for overflow, underflow or invalid characters
  // * * * *
  if (errno == ERANGE || str_value == endptr || *endptr != '\0') {
    fprintf(stderr, "Error: Invalid number provided for %s: '%s'\n", arg_name,
            str_value);
    exit(EX_DATAERR);
  }

  return value;
}

AppConfig parse_arguments(int argc, char *argv[]) {
  // Init config struct with defualt values
  // * * * *
  AppConfig config = {
      .is_server = false,
      .is_client = false,
      .address = NULL,
      .input_file = NULL,
      .output_file = NULL,
      .timeout = 1,
      .port = -1,
  };

  // Transform --help to -h
  // * * * *
  struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                  {NULL, 0, NULL, 0}};

  int opt;
  int option_index = 0;

  //=====================================================
  //====================ARGS SELECTOR====================
  //=====================================================
  while ((opt = getopt_long(argc, argv, "scp:a:i:o:w:h", long_options,
                            &option_index)) != -1) {
    switch (opt) {
    case 's':
      config.is_server = true;
      break;
    case 'c':
      config.is_client = true;
      break;
    case 'p':
      config.port = parse_number(optarg, "-p");
      break;
    case 'a':
      config.address = optarg;
      break;
    case 'i':
      config.input_file = optarg;
      break;
    case 'o':
      config.output_file = optarg;
      break;
    case 'w':
      config.timeout = parse_number(optarg, "-w");
      break;
    case 'h':
      help_message();
      exit(0);
    case '?':
      fprintf(stderr, "Error: Unknown flag!\n");
      exit(EX_USAGE);
    default:
      fprintf(stderr, "Error: Unknown flag!\n");
      exit(EX_USAGE);
    }
  }

  //=====================================================
  //====================ERROR CATCHES====================
  //=====================================================
  if (config.is_server == config.is_client) {
    fprintf(
        stderr,
        "Error: Client or server were not given, or they were given both!\n");
    exit(EX_USAGE);
  }

  if (config.port == -1) {
    fprintf(stderr, "Error: PORT expected a number, but was not given!\n");
    exit(EX_USAGE);
  }

  if (config.port < 0 || config.port > 65535) {
    fprintf(stderr, "Error: Given PORT number is in un-allowed range!\n");
    exit(EX_DATAERR);
  }

  if (config.timeout <= 0) {
    fprintf(stderr, "Error: Unallowed TIMEOUT value!\n");
    exit(EX_DATAERR);
  }

  if (config.is_client) {
    if (config.address == NULL) {
      fprintf(stderr, "Error: Expected an ADDRESS, but was not given!\n");
      exit(EX_USAGE);
    }

    if (config.output_file != NULL) {
      fprintf(stderr, "Error: OUTPUT file was given, but not expected!\n");
      exit(EX_USAGE);
    }
  }

  if (config.is_server) {
    if (config.input_file != NULL) {
      fprintf(stderr, "Error: INPUT file was given, but not expected!\n");
      exit(EX_USAGE);
    }
  }

  return config;
}

void help_message() {
  printf("=====================================================\n");
  printf("      IPK Project 2 - Reliable File Transfer         \n");
  printf("=====================================================\n\n");

  printf("USAGE:\n");
  printf("  Server: ./ipk-rdt -s -p PORT [-a ADDRESS] [-o OUTPUT] [-w TIMEOUT] "
         "[-h | --help]\n");
  printf("  Client: ./ipk-rdt -c -a HOST -p PORT [-i INPUT] [-w TIMEOUT] [-h | "
         "--help]\n\n");

  printf("MANDATORY OPTIONS:\n");
  printf("  -s            Starts the receiving side of the application (Server "
         "mode).\n");
  printf("  -c            Starts the sending side of the application (Client "
         "mode).\n");
  printf("                (Exactly one of -c or -s MUST be specified)\n");
  printf("  -p PORT       Specifies the UDP port number.\n\n");

  printf("ADDRESS OPTIONS:\n");
  printf("  -a ADDRESS    Server mode: Specifies the local bind address.\n");
  printf("                             If omitted, listens on all suitable "
         "local addresses.\n");
  printf("  -a HOST       Client mode: Specifies the destination hostname or "
         "IPv4/IPv6 address (Required).\n\n");

  printf("FILE OPTIONS:\n");
  printf("  -i INPUT      Client mode: Specifies the input file to send.\n");
  printf("                             If omitted or if INPUT is '-', reads "
         "from stdin.\n");
  printf("  -o OUTPUT     Server mode: Specifies the output file to create or "
         "overwrite.\n");
  printf("                             If omitted or if OUTPUT is '-', writes "
         "to stdout.\n\n");

  printf("TIMING & OTHER OPTIONS:\n");
  printf("  -w TIMEOUT    Specifies a positive timeout in whole seconds.\n");
  printf("                Maximum allowed interval without protocol progress "
         "(Default: 1).\n");
  printf("  -h, --help    Writes this usage instruction to stdout and "
         "terminates.\n");

  printf("=====================================================\n");
  printf(" For more detailed information about the protocol    \n");
  printf(" architecture and behavior, or overall project information, \n");
  printf(" please look to README.md                            \n");
  printf("=====================================================\n");
}