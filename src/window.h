/**
 * @file        window.h
 * @brief       Go-Back-N window definitions and structures
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     Defines the structures and function prototypes necessary for
 * handling the sliding window mechanism used in the Go-Back-N reliable data
 * transfer.
 */

#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>

#include "protocol.h"

#define WINDOW_SIZE 30

/**
 * @struct WindowPacket
 * @brief Represents a single packet buffered inside the sender's sliding
 * window.
 */
typedef struct {
  RDTHeader header; // Pre-built network header
  uint8_t payload[MAX_PACKET_SIZE - sizeof(RDTHeader)]; // Data chunk
  uint16_t payload_len; // Length of the data chunk
  bool is_valid;        // 1 - waiting for ACK, 0 - ACKed
} WindowPacket;

/**
 * @struct GBNSenderState
 * @brief Maintains the state of the Go-Back-N sender window.
 */
typedef struct {
  uint32_t base;       // Sequence number of the oldest unacknowledged packet
  uint32_t nextseqnum; // Sequence number for the next packet to be sent
  WindowPacket buffer[WINDOW_SIZE]; // Circular buffer holding window packets
  uint64_t window_timer_ms; // Timer tracking the oldest unacknowledged packet
} GBNSenderState;

/**
 * @brief Fill the GBN window and send packets until the window is full or EOF.
 * @param state Sender window state.
 * @param file Input stream to read from.
 * @param my_socket UDP socket.
 * @param server_addr Destination address.
 * @param server_addr_len Destination address length.
 * @param connection Connection state (seq/ack/conn_id).
 * @return 1 if EOF reached, 0 if more data remains, negative on error.
 */
int gbn_fill_and_send_window(GBNSenderState *state, FILE *file, int my_socket,
                             struct sockaddr_storage *server_addr,
                             socklen_t server_addr_len,
                             ConnectionState *connection);

/**
 * @brief Process a cumulative ACK and advance the window.
 * @param state Sender window state.
 * @param ack_num Acknowledged sequence number (next expected).
 * @return 1 if new data was acknowledged, 0 otherwise.
 */
int gbn_process_ack(GBNSenderState *state, uint32_t ack_num);

/**
 * @brief Retransmit all unacknowledged packets in the window.
 * @param state Sender window state.
 * @param my_socket UDP socket.
 * @param server_addr Destination address.
 * @param server_addr_len Destination address length.
 */
void gbn_retransmit_window(GBNSenderState *state, int my_socket,
                           struct sockaddr_storage *server_addr,
                           socklen_t server_addr_len);

#endif