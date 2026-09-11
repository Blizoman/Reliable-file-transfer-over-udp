/**
 * @file        utils.h
 * @brief       Utility functions and network helpers
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     Provides essential helper functions for the IPK project,
 * including signal handling, checksum calculation, precise non-blocking socket
 * waiting, and file I/O operations.
 */

#ifndef UTILS_H
#define UTILS_H

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "protocol.h"

/**
 * @brief Read up to max_len bytes from a file into buffer.
 * @param file Input stream.
 * @param buffer Output buffer.
 * @param max_len Maximum number of bytes to read.
 * @return Number of bytes read, 0 on EOF, negative on error.
 */
int read_chunk(FILE *file, uint8_t *buffer, size_t max_len);

/**
 * @brief Compute the 16-bit Internet checksum over a buffer.
 * @param addr Buffer start.
 * @param len Buffer length in bytes.
 * @return Checksum value.
 */
uint16_t calculate_checksum(void *addr, int len);

/**
 * @brief Flag set by signal handler when termination is requested.
 */
extern volatile sig_atomic_t stop_requested;

/**
 * @brief Install SIGINT and SIGTERM handlers.
 * @return 0 on success, negative on error.
 */
int install_signal_handlers(void);

/**
 * @brief Wait for packet availability on a socket.
 * @param sockfd Socket file descriptor.
 * @param timeout_ms Timeout in milliseconds.
 * @return 1 if ready, 0 on timeout, negative on error.
 */
int wait_for_packet(int sockfd, int timeout_ms);

/**
 * @brief Get monotonic time in milliseconds.
 * @return Current timestamp in ms, or 0 on error.
 */
uint64_t monotonic_time_ms(void);

/**
 * @brief Update last_progress_ms to current time.
 * @param last_progress_ms Pointer to progress timestamp.
 */
void mark_progress(uint64_t *last_progress_ms);

/**
 * @brief Check if progress timeout has been exceeded.
 * @param last_progress_ms Last progress timestamp in ms.
 * @param timeout_sec Timeout in seconds.
 * @return 1 if exceeded, 0 otherwise.
 */
int progress_timeout_reached(uint64_t last_progress_ms, int timeout_sec);

/**
 * @brief Open a file stream or return default stream for NULL or "-".
 * @param filename Path or "-" for default.
 * @param mode fopen mode string.
 * @param default_stream Stream to return for NULL or "-".
 * @return File stream pointer or NULL on error.
 */
FILE *get_file_stream(char *filename, char *mode, FILE *default_stream);

/**
 * @brief Fill an RDTHeader with the provided values.
 * @param header Header to fill.
 * @param seq Sequence number.
 * @param ack Acknowledgement number.
 * @param conn_id Connection identifier.
 * @param flags Packet flags.
 * @param payload_len Payload length in bytes.
 */
void fill_header(RDTHeader *header, uint32_t seq, uint32_t ack,
                 uint16_t conn_id, uint8_t flags, uint16_t payload_len);

/**
 * @brief Serialize and send a packet over UDP.
 * @param sock Socket file descriptor.
 * @param addr Destination address.
 * @param addr_len Destination address length.
 * @param header Packet header in host byte order.
 * @param payload Payload buffer (may be NULL if payload_len is 0).
 * @param payload_len Payload length in bytes.
 * @return Bytes sent on success, negative on error.
 */
ssize_t send_packet(int sock, struct sockaddr_storage *addr, socklen_t addr_len,
                    RDTHeader *header, uint8_t *payload, uint16_t payload_len);

/**
 * @brief Receive and parse a packet from a UDP socket.
 * @param sock Socket file descriptor.
 * @param addr Source address storage.
 * @param addr_len In/out length of source address.
 * @param out_header Output header in host byte order.
 * @param out_payload Output payload buffer.
 * @param out_payload_len Output payload length.
 * @return 0 on success, negative on parse or receive error.
 */
int recv_packet(int sock, struct sockaddr_storage *addr, socklen_t *addr_len,
                RDTHeader *out_header, uint8_t *out_payload,
                size_t *out_payload_len);

#endif