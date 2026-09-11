/**
 * @file        server.c
 * @brief       Server implementation for the IPK Reliable Data Transfer
 * @author      Andrej Bližnák (xblizna00)
 * @date        2026
 * @details     Contains the core logic for the server side of the protocol,
 * managing the state machine to listen for incoming connections, perform the
 *              3-way handshake, receive data reliably, and terminate the
 * session.
 */

// Help from AI...
// struct 'hints' was not recognized by compiler..
// Intellisens bug ?
#define _POSIX_C_SOURCE 200112L

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h> // rand
#include <string.h> // memset
#include <sys/types.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include "protocol.h"
#include "server.h"
#include "utils.h"

#define RETRANSMIT_INTERVAL_MS 250

int setup_server_socket(AppConfig *config) {
  struct addrinfo hints, *result, *rider;
  memset(&hints, 0, sizeof(hints));

  // IPv4/IPv6
  // * * * *
  hints.ai_family = AF_UNSPEC;
  // UDP
  // * * * *
  hints.ai_socktype = SOCK_DGRAM;
  // Server mode
  // * * * *
  hints.ai_flags = AI_PASSIVE;

  // PORT convertion to string, getaddrinfo accepts only string 'variants'
  // for ex.: expressive "ssh", "http", ... tags
  // * * * *
  char port_str[MAX_PORT_LENGTH_IN_SIGNS];
  snprintf(port_str, sizeof(port_str), "%d", config->port);

  char *bind_address = NULL;
  if (config->address != NULL && config->address[0] != '\0') {
    bind_address = config->address;
  }

  // Resolve hostname, if address is nto specified, program listens everywhere
  // (Re-used and modified from first IPK project)
  // * * * *
  if ((getaddrinfo(bind_address, port_str, &hints, &result)) != 0) {
    fprintf(stderr, "Error: failed to resolve bind address '%s'\n",
            (bind_address != NULL) ? bind_address : "ANY");
    return -EX_NOHOST;
  }

  int my_socket;

  // Iterate through available addresses and attempt to bind to the first
  // successful one
  // * * * *
  for (rider = result; rider != NULL; rider = rider->ai_next) {
    my_socket = socket(rider->ai_family, SOCK_DGRAM, IPPROTO_UDP);

    if (my_socket >= 0) {
      // Try to bind created socket to the resolved address and port
      // * * * *
      if (bind(my_socket, rider->ai_addr, rider->ai_addrlen) == 0) {
        freeaddrinfo(result);
        return my_socket;
      }

      close(my_socket);
    }
  }

  fprintf(stderr, "Error: failed to create or bind any socket\n");
  freeaddrinfo(result);
  return -EX_OSERR;
}
//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//=====================================================
//====================STATE MACHINE====================
//=====================================================
//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
/**
 * @brief Handle the SERVER_LISTEN state (wait for SYN).
 * @param my_socket UDP socket.
 * @param config Runtime configuration.
 * @param client_addr Pointer to store the incoming client address.
 * @param client_addr_len Length of the client address structure.
 * @param send_header Buffer for the header to be sent.
 * @param recv_header Buffer for the received header.
 * @param connection Connection state structure.
 * @param last_progress_ms Pointer to the timestamp of the last successful
 * progress.
 * @param exit_code Pointer to the application exit code.
 * @return Next state in the server state machine.
 */
static ServerState handle_listen(int my_socket, AppConfig *config,
                                 struct sockaddr_storage *client_addr,
                                 socklen_t client_addr_len,
                                 RDTHeader *send_header, RDTHeader *recv_header,
                                 ConnectionState *connection,
                                 uint64_t *last_progress_ms, int *exit_code) {
  size_t payload_len = 0;
  uint8_t dummy_payload[MAX_PACKET_SIZE];

  // Check for overall protocol inactivity timeout
  // * * * *
  if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
    fprintf(stderr,
            "Error: no protocol progress while waiting for session start.\n");
    *exit_code = EX_PROTOCOL;
    return SERVER_DONE;
  }

  int ready = wait_for_packet(my_socket, RETRANSMIT_INTERVAL_MS);
  if (ready == 0) {
    if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
      fprintf(stderr,
              "Error: no protocol progress while waiting for session start.\n");
      *exit_code = EX_PROTOCOL;
      return SERVER_DONE;
    }
    return SERVER_LISTEN;
  }
  if (ready < 0) {
    fprintf(stderr, "Error: socket wait failed in LISTEN state.\n");
    *exit_code = EX_OSERR;
    return SERVER_DONE;
  }

  // Wait for incoming packet. Malformed packets are inherently dropped by
  // recv_packet
  // * * * *
  if (recv_packet(my_socket, client_addr, &client_addr_len, recv_header, dummy_payload,
                  &payload_len) < 0) {
    return SERVER_LISTEN; // Malformed pkt's are skipped
  }

  // Process initial connection request (SYN)
  // * * * *
  if (recv_header->flags == FLAG_SYN) {
    fprintf(stderr, "[SERVER] Accepted SYN from client! Sending SYN+ACK.\n");

    // Initialize connection tracking identifiers
    // * * * *
    connection->conn_id = (rand() % 60000) + 1;
    connection->seq_num = rand() % MODULER;
    connection->ack_num = recv_header->seq_num + 1;

    fill_header(send_header, connection->seq_num, connection->ack_num,
                connection->conn_id, FLAG_SYN | FLAG_ACK, 0);

    if (send_packet(my_socket, client_addr, client_addr_len, send_header, NULL,
                    0) < 0) {
      fprintf(stderr, "Error: failed to send SYN+ACK packet.\n");
      *exit_code = EX_OSERR;
      return SERVER_DONE;
    }

    mark_progress(last_progress_ms);

    return SERVER_HANDSHAKE;
  }

  return SERVER_LISTEN;
}

/**
 * @brief Handle the SERVER_HANDSHAKE state (wait for final ACK).
 * @param my_socket UDP socket.
 * @param config Runtime configuration.
 * @param client_addr Client address structure.
 * @param client_addr_len Length of the client address.
 * @param send_header Buffer for the header to be sent.
 * @param recv_header Buffer for the received header.
 * @param connection Connection state structure.
 * @param last_progress_ms Pointer to the timestamp of the last successful
 * progress.
 * @param exit_code Pointer to the application exit code.
 * @return Next state in the server state machine.
 */
static ServerState handle_handshake(
    int my_socket, AppConfig *config, struct sockaddr_storage *client_addr,
    socklen_t client_addr_len, RDTHeader *send_header, RDTHeader *recv_header,
    ConnectionState *connection, uint64_t *last_progress_ms, int *exit_code) {
  size_t payload_len = 0;
  uint8_t dummy_payload[MAX_PACKET_SIZE];

  if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
    fprintf(stderr, "Error: no protocol progress during handshake.\n");
    *exit_code = EX_PROTOCOL;
    return SERVER_DONE;
  }

  // Wait for the final ACK from the client to complete the 3-way handshake
  // * * * *
  int ready = wait_for_packet(my_socket, RETRANSMIT_INTERVAL_MS);

  if (ready == 0) {
    if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
      fprintf(stderr, "Error: no protocol progress during handshake.\n");
      *exit_code = EX_PROTOCOL;
      return SERVER_DONE;
    }

    return SERVER_HANDSHAKE;
  } else if (ready < 0) {
    fprintf(stderr, "Error: socket wait failed during handshake.\n");
    *exit_code = EX_OSERR;
    return SERVER_DONE;
  }

  if (recv_packet(my_socket, client_addr, &client_addr_len, recv_header, dummy_payload,
                  &payload_len) < 0) {
    return SERVER_HANDSHAKE; // Malformed pkt
  }

  // If client re-sends SYN, re-transmit the SYN+ACK response
  // * * * *
  if (recv_header->flags == FLAG_SYN &&
      recv_header->seq_num + 1 == connection->ack_num) {
    fill_header(send_header, connection->seq_num, connection->ack_num,
                connection->conn_id, FLAG_SYN | FLAG_ACK, 0);

    if (send_packet(my_socket, client_addr, client_addr_len, send_header, NULL,
                    0) < 0) {
      fprintf(stderr, "Error: failed to re-send SYN+ACK packet.\n");
      *exit_code = EX_OSERR;
      return SERVER_DONE;
    }

    return SERVER_HANDSHAKE;
  }

  // Check, if ACK is from same CLIENT (connection_id's are same)
  // * * * *
  if (recv_header->flags == FLAG_ACK &&
      recv_header->conn_id == connection->conn_id &&
      recv_header->ack_num == connection->seq_num + 1 &&
      recv_header->seq_num == connection->ack_num) {
    fprintf(stderr,
            "[SERVER] Accepted final ACK! 3-WAY HANDSHAKE SUCCESSFUL!\n");

    connection->ack_num = recv_header->seq_num;
    mark_progress(last_progress_ms);
    return SERVER_ESTABLISHED;
  }

  // |EDGE CASE|: Final ACK was lost, but client started sending DATA.
  // Implicitly accept the connection.
  // * * * *
  if (recv_header->flags == FLAG_DATA &&
      recv_header->conn_id == connection->conn_id &&
      recv_header->seq_num == connection->ack_num &&
      recv_header->ack_num == connection->seq_num + 1) {
    fprintf(stderr, "[SERVER] DATA received during handshake. Assuming final "
                    "ACK was lost.\n");
    mark_progress(last_progress_ms);
    return SERVER_ESTABLISHED;
  }
  return SERVER_HANDSHAKE;
}

/**
 * @brief Handle the SERVER_ESTABLISHED state (receive data and handle
 * teardown).
 * @param my_socket UDP socket.
 * @param config Runtime configuration.
 * @param file Output file stream.
 * @param payload Buffer to store the received data payload.
 * @param client_addr Client address structure.
 * @param client_addr_len Length of the client address.
 * @param send_header Buffer for the header to be sent.
 * @param recv_header Buffer for the received header.
 * @param connection Connection state structure.
 * @param last_progress_ms Pointer to the timestamp of the last successful
 * progress.
 * @param exit_code Pointer to the application exit code.
 * @return Next state in the server state machine.
 */
static ServerState
handle_established(int my_socket, AppConfig *config, FILE *file,
                   uint8_t *payload, struct sockaddr_storage *client_addr,
                   socklen_t client_addr_len, RDTHeader *send_header,
                   RDTHeader *recv_header, ConnectionState *connection,
                   uint64_t *last_progress_ms, int *exit_code) {
  size_t payload_len = 0;

  if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
    fprintf(stderr, "Error: no protocol progress during data transfer.\n");
    *exit_code = EX_PROTOCOL;
    return SERVER_DONE;
  }

  int ready = wait_for_packet(my_socket, RETRANSMIT_INTERVAL_MS);
  if (ready == 0) {
    if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
      fprintf(stderr, "Error: no protocol progress during data transfer.\n");
      *exit_code = EX_PROTOCOL;
      return SERVER_DONE;
    }
    return SERVER_ESTABLISHED;
  }
  if (ready < 0) {
    fprintf(stderr, "Error: socket wait failed in ESTABLISHED state.\n");
    *exit_code = EX_OSERR;
    return SERVER_DONE;
  }

  if (recv_packet(my_socket, client_addr, &client_addr_len, recv_header,
                  payload, &payload_len) < 0) {
    return SERVER_ESTABLISHED; // Malformed pkt
  }

  // Process connection termination request from client
  // * * * *
  if (recv_header->flags == FLAG_FIN &&
      recv_header->conn_id == connection->conn_id &&
      recv_header->seq_num == connection->ack_num) {
    fprintf(stderr, "[SERVER] Accepted FIN. Sending FIN+ACK.\n");

    // Sending FIN + ACK to CLIENT
    // * * * *
    connection->ack_num = recv_header->seq_num + 1;
    fill_header(send_header, connection->seq_num, connection->ack_num,
                connection->conn_id, FLAG_FIN | FLAG_ACK, 0);

    if (send_packet(my_socket, client_addr, client_addr_len, send_header, NULL,
                    0) < 0) {
      fprintf(stderr, "Error: failed to send FIN+ACK packet.\n");
      *exit_code = EX_OSERR;
      return SERVER_DONE;
    }

    mark_progress(last_progress_ms);

    return SERVER_LAST_ACK;
  }

  // Process incoming data packets
  // * * * *
  if (recv_header->flags == FLAG_DATA) {
    if (recv_header->conn_id == connection->conn_id) {

      // If the received sequence number matches the expected one, save the
      // payload
      // * * * *
      if (recv_header->seq_num == connection->ack_num) {
        size_t written = fwrite(payload, 1, payload_len, file);

        if (written != payload_len) {
          fprintf(
              stderr,
              "Error: failed to write received payload to output stream.\n");
          *exit_code = EX_IOERR;
          return SERVER_DONE;
        }

        // Advance the expectation window to the next required sequence number
        // * * * *
        connection->ack_num += 1;
        mark_progress(last_progress_ms);
      }

      // If the sequence number is greater than expected (out-of-order packet),
      // ignore data but mark progress
      // * * * *
      else if (recv_header->seq_num > connection->ack_num) {
        mark_progress(last_progress_ms);
      }

      // Always acknowledge the last successfully received in-order packet
      // (Cumulative ACK mechanism)
      // * * * *
      fill_header(send_header, connection->seq_num, connection->ack_num,
                  connection->conn_id, FLAG_ACK, 0);

      if (send_packet(my_socket, client_addr, client_addr_len, send_header,
                      NULL, 0) < 0) {
        fprintf(stderr, "Error: failed to send ACK packet.\n");
        *exit_code = EX_OSERR;
        return SERVER_DONE;
      }
    }
  }

  return SERVER_ESTABLISHED;
}

/**
 * @brief Handle the SERVER_LAST_ACK state (wait for final termination ACK).
 * @param my_socket UDP socket.
 * @param config Runtime configuration.
 * @param client_addr Client address structure.
 * @param client_addr_len Length of the client address.
 * @param send_header Buffer for the header to be sent.
 * @param recv_header Buffer for the received header.
 * @param connection Connection state structure.
 * @param last_progress_ms Pointer to the timestamp of the last successful
 * progress.
 * @param exit_code Pointer to the application exit code.
 * @return Next state in the server state machine.
 */
static ServerState handle_last_ack(
    int my_socket, AppConfig *config, struct sockaddr_storage *client_addr,
    socklen_t client_addr_len, RDTHeader *send_header, RDTHeader *recv_header,
    ConnectionState *connection, uint64_t *last_progress_ms, int *exit_code) {
  if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
    fprintf(stderr, "Error: no protocol progress during termination.\n");
    *exit_code = EX_PROTOCOL;
    return SERVER_DONE;
  }

  int ready = wait_for_packet(my_socket, RETRANSMIT_INTERVAL_MS);

  if (ready == 0) {
    if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
      // Assume the final ACK from client was lost, but the timeout expired
      // safely
      // * * * *
      fprintf(stderr, "Error: no protocol progress during termination.\n");
      *exit_code = EX_OK;
      return SERVER_DONE;
    }

    // Retransmit FIN+ACK if waiting times out
    // * * * *
    fill_header(send_header, connection->seq_num, connection->ack_num,
                connection->conn_id, FLAG_FIN | FLAG_ACK, 0);
    if (send_packet(my_socket, client_addr, client_addr_len, send_header, NULL,
                    0) < 0) {
      fprintf(stderr, "Error: failed to re-send FIN+ACK packet.\n");
      *exit_code = EX_OK;
      return SERVER_DONE;
    }

    return SERVER_LAST_ACK;
  } else if (ready < 0) {
    if (stop_requested) {
      *exit_code = EX_OK;
      return SERVER_DONE;
    }
    fprintf(stderr, "Error: socket wait failed in LAST_ACK.\n");
    *exit_code = EX_OSERR;
    return SERVER_DONE;
  }

  size_t payload_len = 0;
  if (recv_packet(my_socket, client_addr, &client_addr_len, recv_header, NULL,
                  &payload_len) < 0) {
    return SERVER_LAST_ACK;
  }

  // Handle duplicate FIN packet re-transmission from client
  // * * * *
  if (recv_header->flags == FLAG_FIN &&
      recv_header->conn_id == connection->conn_id &&
      recv_header->seq_num + 1 == connection->ack_num) {
    fill_header(send_header, connection->seq_num, connection->ack_num,
                connection->conn_id, FLAG_FIN | FLAG_ACK, 0);

    if (send_packet(my_socket, client_addr, client_addr_len, send_header, NULL,
                    0) < 0) {
      fprintf(stderr, "Error: failed to re-send FIN+ACK packet.\n");
      *exit_code = EX_OSERR;
      return SERVER_DONE;
    }

    return SERVER_LAST_ACK;
  }

  // Successfully received the final ACK from the client
  // * * * *
  if (recv_header->flags == FLAG_ACK &&
      recv_header->conn_id == connection->conn_id &&
      recv_header->ack_num == connection->seq_num + 1 &&
      recv_header->seq_num == connection->ack_num) {
    fprintf(stderr,
            "[SERVER] Accepted final ACK. Closing connection cleanly.\n");
    mark_progress(last_progress_ms);
    return SERVER_DONE;
  }

  return SERVER_LAST_ACK;
}

int run_server(AppConfig *config) {
  struct sockaddr_storage client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  int exit_code = EX_OK;

  int my_socket = setup_server_socket(config);
  if (my_socket < 0) {
    return -my_socket;
  }

  // Initialization of headers
  // * * * *
  RDTHeader send_header = {0};
  RDTHeader recv_header = {0};

  // Data storage
  // * * * *
  uint8_t payload[MAX_PACKET_SIZE];

  // Init SERVER state
  // * * * *
  ServerState state = SERVER_LISTEN;

  // Connection helper struct
  // * * * *
  ConnectionState connection = {0};
  uint64_t last_progress_ms = monotonic_time_ms();

  // Load given output file / use STDOUT
  // * * * *
  FILE *file = get_file_stream(config->output_file, "wb", stdout);
  if (file == NULL) {
    fprintf(stderr, "Error: failed to open output file '%s'\n",
            config->output_file);
    close(my_socket);
    return EX_CANTCREAT;
  }

  srand(time(NULL));

  // Main server state machine loop
  // * * * *
  while (state != SERVER_DONE) {
    // Handle unexpected termination signals gracefully
    // * * * *
    if (stop_requested) {
      fprintf(stderr, "[SERVER] Signal received. Shutting down.\n");
      exit_code = EX_SOFTWARE;
      state = SERVER_DONE;
      break;
    }

    // FSM Realization
    // * * * *
    switch (state) {
    // * * * *
    case SERVER_LISTEN: {
      state = handle_listen(my_socket, config, &client_addr, client_addr_len, &send_header,
                            &recv_header, &connection, &last_progress_ms, &exit_code);
      break;
    }
    // * * * *
    case SERVER_HANDSHAKE: {
      state = handle_handshake(my_socket, config, &client_addr, client_addr_len,
                             &send_header, &recv_header, &connection, &last_progress_ms, &exit_code);
      break;
    }
    // * * * *
    case SERVER_ESTABLISHED: {
      state = handle_established(my_socket, config, file, payload, &client_addr,
                                 client_addr_len, &send_header, &recv_header,
                                 &connection, &last_progress_ms, &exit_code);
      break;
    }
    // * * * *
    case SERVER_LAST_ACK: {
      state = handle_last_ack(my_socket, config, &client_addr, client_addr_len,
                              &send_header, &recv_header, &connection,
                              &last_progress_ms, &exit_code);
      break;
    }
    // * * * *
    default:
      state = SERVER_DONE;
      break;
    }
  }

  // Cleanup resources
  // * * * *
  if (file != stdout && fclose(file) != 0) {
    fprintf(stderr, "Error: failed to close output file.\n");
    exit_code = EX_IOERR;
  }
  if (close(my_socket) != 0) {
    fprintf(stderr, "Error: failed to close server socket.\n");
    exit_code = EX_OSERR;
  }
  fprintf(stderr, "[SERVER] Ending.\n");

  return exit_code;
}