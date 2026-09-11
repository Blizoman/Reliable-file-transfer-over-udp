/**
 * @file        window.c
 * @brief       Go-Back-N window implementation
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     Implements the core logic for the Go-Back-N sliding window
 * protocol, including filling the window with file data, sending packets,
 * processing cumulative acknowledgments, and handling retransmissions.
 */

#include <stdio.h>
#include <string.h>
#include <sysexits.h>

#include "utils.h"
#include "window.h"

void gbn_retransmit_window(GBNSenderState *state, int my_socket,
                           struct sockaddr_storage *server_addr,
                           socklen_t server_addr_len) {
  // Empty window is skipped
  // * * * *
  if (state->base == state->nextseqnum) {
    return;
  }

  fprintf(stderr, "[Go-back-N] Retransmitting window from seq %u to %u\n",
          state->base, state->nextseqnum - 1);

  // By kurose, try to re-send all packets from base to next sequence number - 1
  // * * * *
  for (uint32_t seq = state->base; seq < state->nextseqnum; seq++) {
    int index = seq % WINDOW_SIZE;

    // If packet was not ACKed yet, try to rentransmit it
    // * * * *
    if (state->buffer[index].is_valid) {
      if (send_packet(my_socket, server_addr, server_addr_len,
                      &state->buffer[index].header,
                      state->buffer[index].payload,
                      state->buffer[index].payload_len) < 0) {
        fprintf(stderr, "[Go-back-N] Retransmit failed for seq %u\n", seq);
      }
    }
  }

  // Restart the timer after a retransmission
  // * * * *
  state->window_timer_ms = monotonic_time_ms();
}

int gbn_process_ack(GBNSenderState *state, uint32_t ack_num) {
  // Ignore ACKs that fall outside the current unacknowledged window bounds
  // * * * *
  if (ack_num <= state->base || ack_num > state->nextseqnum) {
    return 0;
  }

  int acked_new = 0;

  // Cumulatively acknowledge all packets up to the received ack_num
  // * * * *
  for (uint32_t seq = state->base; seq < ack_num; seq++) {
    int index = seq % WINDOW_SIZE;
    if (state->buffer[index].is_valid) {
      state->buffer[index].is_valid = 0; // Mark as `ACKed`
      acked_new = 1;
    }
  }

  // Advance the window base to the newly acknowledged sequence number
  // * * * *
  state->base = ack_num;

  return acked_new;
}

int gbn_fill_and_send_window(GBNSenderState *state, FILE *file, int my_socket,
                             struct sockaddr_storage *server_addr,
                             socklen_t server_addr_len,
                             ConnectionState *connection) {
  int eof_reached = 0;

  // Continue sending until the window is completely full
  // * * * *
  while (state->nextseqnum < state->base + WINDOW_SIZE) {
    int index = state->nextseqnum % WINDOW_SIZE;

    // Read a chunk of data from the input stream
    // * * * *
    int read_result = read_chunk(file, state->buffer[index].payload,
                                 MAX_PACKET_SIZE - sizeof(RDTHeader));

    if (read_result == 0) {
      eof_reached = 1;
      break;
    }
    if (read_result < 0) {
      fprintf(stderr, "Error: read_chunk failed\n");
      return -EX_IOERR;
    }

    // Type convertion from 'size_t' is ok cuz max size is
    // 1185 bytes so there is no chcance for overflow
    // * * * *
    uint16_t bytes_read = (uint16_t)read_result;

    // Start timer when first "UN-ACKED" packet goes by
    // * * * *
    if (state->base == state->nextseqnum) {
      state->window_timer_ms = monotonic_time_ms();
    }

    // Build header with seq nums and connection identifiers
    // * * * *
    state->buffer[index].header = (RDTHeader){.seq_num = state->nextseqnum,
                                              .ack_num = connection->ack_num,
                                              .conn_id = connection->conn_id,
                                              .flags = FLAG_DATA,
                                              .payload_len = bytes_read};

    state->buffer[index].payload_len = bytes_read;
    state->buffer[index].is_valid =
        1; // Mark pkt as ready, so is waiting for beign ACKed

    // Transmit the newly built packet
    // * * * *
    if (send_packet(my_socket, server_addr, server_addr_len,
                    &state->buffer[index].header, state->buffer[index].payload,
                    bytes_read) < 0) {
      fprintf(stderr, "[GBN] send_packet failed for seq %u\n",
              state->nextseqnum);
    }

    // Move to the next sequence number
    // * * * *
    state->nextseqnum++;
  }

  return eof_reached;
}