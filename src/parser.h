/**
 * @file        parser.h
 * @brief       Command-line argument parsing definitions
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     Provides function prototypes for parsing CLI arguments,
 *              converting string numbers safely, and displaying the help
 * message.
 */

#ifndef PARSER_H
#define PARSER_H

#include "config.h"

/**
 * @brief Parse CLI arguments into an AppConfig.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Parsed configuration (exits on invalid input).
 */
AppConfig parse_arguments(int argc, char *argv[]);

/**
 * @brief Parse a numeric argument with validation.
 * @param str_value String value to parse.
 * @param arg_name Argument name for error reporting.
 * @return Parsed integer value (exits on invalid input).
 */
long parse_number(char *str_value, char *arg_name);

/**
 * @brief Print usage information to stdout.
 */
void help_message();

#endif