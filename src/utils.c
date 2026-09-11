/**
 * @file        utils.c
 * @brief       Utility functions and network helpers implementation
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     Implements essential tools for packet sending/receiving,
 *              timeout handling via select(), checksum validation, and file
 * manipulation.
 */

#define _POSIX_C_SOURCE 200112L // sigaction was angry :d

#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sysexits.h>
#include <time.h>

#include "utils.h"
#include "protocol.h"

//============================================================
//+-+-+-+-+-+-+-+-+-+-+SIGNAL HANDLERS+-+-+-+-+-+-+-+-+-+-+-+-
//============================================================
// Global flag used for shut down the application when receiving a signal
// * * * *
volatile sig_atomic_t stop_requested = 0;

void handle_signal(int signum) {
  (void)signum;
  stop_requested = 1;
}

int install_signal_handlers(void) {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = handle_signal;

  if (sigemptyset(&action.sa_mask) != 0) {
    return -1;
  }

  if (sigaction(SIGINT, &action, NULL) != 0) {
    return -1;
  }

  if (sigaction(SIGTERM, &action, NULL) != 0) {
    return -1;
  }

  return 0;
}

//============================================================
//+-+-+-+-+-+-+-+-+-+-+-+-+CHECKSUM+-+-+-+-+-+-+-+-+-+-+-+-+-+
//============================================================
uint16_t calculate_checksum(void *addr, int len) {
  // Use a 32 bit "storage" to catch overflows (carries).
  // * * * *
  uint32_t sum = 0;
  // Cast to "uint8_t *" for safe byte reading.
  // * * * *
  uint8_t *data = (uint8_t *)addr;
  // Process the buffer in 2 byte (16-bit) pieces.
  // * * * *
  while (len > 1) {
    // Construct the 16 bit word in Big Endian format.
    // This ensures the calculation is mathematically identical across all CPU
    // architectures.
    // * * * *
    uint16_t word = (data[0] << 8) | data[1];
    sum += word;
    // Fold 32 bit SUM to 16 bits.
    // * * * *
    if (sum > 0xFFFF)
      sum = (sum & 0xFFFF) + 1;

    data += 2;
    len -= 2;
  }

  // If the data length is ODD, dictates we pad last byte
  // with (virtual) zero byte to form a 16 bit word.
  // * * * *
  if (len > 0) {
    // Zero pad: (data[0] << 8) | 0x00
    // * * * *
    sum += data[0] << 8;

    // Final carry handler
    // * * * *
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  // At the end, the result is transformed to "One`s complement" by flipping all
  // bits.
  // * * * *
  return (uint16_t)~sum;
}

//============================================================
//+-+-+-+-+-+-+-+-+-+TIME & TIMEOUTS+-+-+-+-+-+-+-+-+-+-+-+-+-
//============================================================
int wait_for_packet(int my_socket, int timeout_ms) {
  fd_set read_sckts;              // Package of sockets
  FD_ZERO(&read_sckts);           // Clean package, then
  FD_SET(my_socket, &read_sckts); // add socket to package

  if (timeout_ms < 0) {
    timeout_ms = 0;
  }

  // Convertion milliseconds to seconds and microseconds
  // * * * *
  struct timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;

  // Wait until the socket is ready for reading or until timeout occurs
  // * * * *
  int select_result = select(my_socket + 1, &read_sckts, NULL, NULL, &timeout);

  if (select_result > 0) {
    return 1; // Packet has arrived
  }
  if (select_result == 0) {
    return 0; // Packet has timedouted
  }

  return -EX_OSERR; // TODO: err system prob
}

uint64_t monotonic_time_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }

  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

void mark_progress(uint64_t *last_progress_ms) {
  if (last_progress_ms == NULL) {
    return;
  }

  uint64_t now_ms = monotonic_time_ms();
  if (now_ms != 0) {
    *last_progress_ms = now_ms;
  }
}

int progress_timeout_reached(uint64_t last_progress_ms, int timeout_sec) {
  if (last_progress_ms == 0 || timeout_sec <= 0) {
    return 0;
  }

  uint64_t now_ms = monotonic_time_ms();
  if (now_ms == 0) {
    return 0;
  }

  uint64_t timeout_ms = (uint64_t)timeout_sec * 1000ULL;
  return (now_ms - last_progress_ms) >= timeout_ms;
}

//============================================================
//+-+-+-+-+-+-+-+-+-+-+-+-+-+FILE+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//============================================================
FILE *get_file_stream(char *filename, char *mode, FILE *default_stream) {
  // If ommited or '-', then cleint reads from STDIN / server writed on STDOUT
  // * * * *
  if (filename == NULL || strcmp(filename, "-") == 0) {
    return default_stream;
  }
  return fopen(filename, mode);
}

int read_chunk(FILE *file, uint8_t *buffer, size_t max_len) {
  size_t n = fread(buffer, 1, max_len, file);

  if (n > 0) {
    return (int)n;
  }
  // EOF reached
  // * * * *
  if (feof(file)) {
    return 0;
  }
  // I/O err
  // * * * *
  return -EX_IOERR;
}

//============================================================
//+-+-+-+-+-+-+-+-+-+-+PACKET WRAPPERS+-+-+-+-+-+-+-+-+-+-+-+-
//============================================================
void fill_header(RDTHeader *header, uint32_t seq, uint32_t ack,
                 uint16_t conn_id, uint8_t flags, uint16_t payload_len) {
  header->seq_num = seq;
  header->ack_num = ack;
  header->conn_id = conn_id;
  header->flags = flags;
  header->payload_len = payload_len;
}

ssize_t send_packet(int sock, struct sockaddr_storage *addr, socklen_t addr_len,
                    RDTHeader *header, uint8_t *payload, uint16_t payload_len) {
  // Storage for 1200 bytes - 15 used by HEADER
  // * * * *
  uint8_t buffer[MAX_PACKET_SIZE];
  // Store packet (header with data) onto buffer containing 0s and 1s and gets
  // It's final size
  // * * * *
  size_t pkt_size = build_packet(header, payload, payload_len, buffer);
  // Packet is being send to given ADDRESS
  // * * * *
  return sendto(sock, buffer, pkt_size, 0, (struct sockaddr *)addr, addr_len);
}

int recv_packet(int sock, struct sockaddr_storage *addr, socklen_t *addr_len,
                RDTHeader *out_header, uint8_t *out_payload,
                size_t *out_payload_len) {
  // Storage for 1200 bytes - 15 bites used by HEADER
  // * * * *
  uint8_t buffer[MAX_PACKET_SIZE];
  // Waiting for incomming socket, save It's 'receiving' address for future
  // message approach
  // * * * *
  ssize_t recv_len = recvfrom(sock, buffer, sizeof(buffer), 0,
                              (struct sockaddr *)addr, addr_len);

  if (recv_len < 0) {
    return -EX_OSERR;
  }
  // Calcultae checksum and build / fill header and payload structures from
  // buffer datas
  // * * * *
  return parse_packet(out_header, buffer, out_payload, out_payload_len,
                      recv_len);
}
