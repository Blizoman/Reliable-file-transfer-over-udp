/**
 * @file        client.c
 * @brief       Client implementation for the IPK Reliable Data Transfer
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     Implements the client-side state machine and Go-Back-N logic 
 *              to ensure reliable data delivery over UDP.
 */

// Help from AI... 
// struct 'hints' was not recognized by compiler..
// Intellisens bug ?
#define _POSIX_C_SOURCE 200112L 

#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <string.h> // memset
#include <stdlib.h> // rand
#include <sysexits.h>

#include "client.h"
#include "protocol.h"
#include "utils.h"
#include "window.h"

#define RETRANSMIT_INTERVAL_MS 250
#define TIME_WAIT_DURATION_MS 5000

int setup_client_socket(
    AppConfig *config, 
    struct sockaddr_storage *server_addr, 
    socklen_t *server_addr_len
)
{ 
    struct addrinfo hints, *result, *rider;
    memset(&hints, 0, sizeof(hints));

    // IPv4/IPv6
    // * * * *
    hints.ai_family = AF_UNSPEC;
    // UDP
    // * * * *
    hints.ai_socktype = SOCK_DGRAM;

    // PORT convertion to string, getaddrinfo accepts only string 'variants'
    // for ex.: expressive "ssh", "http", ... tags
    // * * * *
    char port_str[MAX_PORT_LENGTH_IN_SIGNS];
    snprintf(port_str, sizeof(port_str), "%d", config->port);

    // Resolve hostname 
    // * * * *
    if ((getaddrinfo(config->address, port_str, &hints, &result)) != 0)
    {
        fprintf(stderr, "Error: failed to resolve hostname '%s'\n", config->address);
        return -EX_NOHOST;
    }

    int my_socket;
    // Looking for first working address
    // * * * *
    for (rider = result; rider != NULL; rider = rider->ai_next) {
        my_socket = socket(rider->ai_family, SOCK_DGRAM, IPPROTO_UDP);

        if (my_socket >= 0) {
            memcpy(server_addr, rider->ai_addr, rider->ai_addrlen);
            *server_addr_len = rider->ai_addrlen;
            break;
        }
    }

    if (rider == NULL) {
        fprintf(stderr, "Error: failed to create socket\n");
        freeaddrinfo(result);
        return -EX_OSERR;
    }

    freeaddrinfo(result);
    return my_socket;
}

//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//=====================================================
//====================STATE MACHINE====================
//=====================================================
//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
/**
 * @brief Handles the CLIENT_CLOSED state. Initiates the connection by sending a SYN packet.
 * 
 * @param my_socket The client UDP socket file descriptor.
 * @param server_addr Pointer to the server address structure.
 * @param server_addr_len Length of the server address structure.
 * @param send_header Pointer to the header structure used for sending packets.
 * @param connection Pointer to the connection state structure.
 * @param exit_code Pointer to the application exit code.
 * @return ClientState The next state of the client (usually CLIENT_SYN_SENT).
 */
static ClientState handle_closed(
    int my_socket,
    struct sockaddr_storage *server_addr,
    socklen_t server_addr_len,
    RDTHeader *send_header,
    ConnectionState *connection,
    int *exit_code
) 
{
    // Memory of connection
    // * * * *
    connection->seq_num = rand() % MODULER;
    connection->ack_num = 0;
    connection->conn_id = 0;

    // Temporary packet header fille by data from memory (connection)
    // * * * *
    fill_header(send_header, connection->seq_num, connection->ack_num, 
                connection->conn_id, FLAG_SYN, 0);

    if (send_packet(my_socket, server_addr, server_addr_len, send_header, NULL, 0) < 0) {
        fprintf(stderr, "Error: failed to send SYN packet.\n");
        *exit_code = EX_OSERR;
        return CLIENT_DONE;
    }

    return CLIENT_SYN_SENT;
}

/**
 * @brief Handles the CLIENT_SYN_SENT state. Waits for a SYN+ACK and sends an ACK.
 * 
 * @param my_socket The client UDP socket file descriptor.
 * @param config Pointer to the application configuration.
 * @param server_addr Pointer to the server address structure.
 * @param server_addr_len Length of the server address structure.
 * @param send_header Pointer to the header structure used for sending packets.
 * @param recv_header Pointer to the header structure used for receiving packets.
 * @param connection Pointer to the connection state structure.
 * @param last_progress_ms Pointer to the timestamp of the last successful progress.
 * @param exit_code Pointer to the application exit code.
 * @return ClientState The next state of the client (CLIENT_ESTABLISHED on success).
 */
static ClientState handle_syn_sent(
    int my_socket,
    AppConfig *config,
    struct sockaddr_storage *server_addr,
    socklen_t server_addr_len,
    RDTHeader *send_header,
    RDTHeader *recv_header,
    ConnectionState *connection,
    uint64_t *last_progress_ms,
    int *exit_code
) 
{
    if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
        fprintf(stderr, "Error: no protocol progress during handshake.\n");
        *exit_code = EX_PROTOCOL;
        return CLIENT_DONE;
    }

    int ready = wait_for_packet(my_socket, RETRANSMIT_INTERVAL_MS);

    if (ready == 0) {
        if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
            fprintf(stderr, "Error: no protocol progress during handshake.\n");
            *exit_code = EX_PROTOCOL;
            return CLIENT_DONE;
        }

        fprintf(stderr, "[CLIENT] Timeout while waiting for SYN+ACK. Resending SYN...\n");
        fill_header(send_header, connection->seq_num, connection->ack_num,
                    connection->conn_id, FLAG_SYN, 0);
        if (send_packet(my_socket, server_addr, server_addr_len, send_header, NULL, 0) < 0) {
            fprintf(stderr, "Error: failed to resend SYN packet.\n");
            *exit_code = EX_OSERR;
            return CLIENT_DONE;
        }
        return CLIENT_SYN_SENT;
    } 
    else if (ready < 0) {
        fprintf(stderr, "Error: socket wait failed during handshake.\n");
        *exit_code = EX_OSERR;
        return CLIENT_DONE;
    }

    size_t recv_payload_len = 0;
    if (recv_packet(my_socket, server_addr, &server_addr_len, recv_header, NULL, &recv_payload_len) < 0) {
        return CLIENT_SYN_SENT;
    }

    if (recv_header->flags == (FLAG_SYN | FLAG_ACK) &&
        recv_header->conn_id != 0 &&
        recv_header->ack_num == (connection->seq_num + 1)) {
        fprintf(stderr, "[CLIENT] Accepted SYN+ACK. Sending ACK.\n");

        connection->conn_id = recv_header->conn_id; 
        connection->ack_num = recv_header->seq_num + 1; 
        connection->seq_num += 1; 
        mark_progress(last_progress_ms);

        fill_header(send_header, connection->seq_num, connection->ack_num, connection->conn_id, FLAG_ACK, 0);

        if (send_packet(my_socket, server_addr, server_addr_len, send_header, NULL, 0) < 0) {
            fprintf(stderr, "Error: failed to send handshake ACK packet.\n");
            *exit_code = EX_OSERR;
            return CLIENT_DONE;
        }
        
        return CLIENT_ESTABLISHED;
    }
    return CLIENT_SYN_SENT;
}


/**
 * @brief Handles the CLIENT_ESTABLISHED state. Manages Go-Back-N data transfer.
 * 
 * @param my_socket The client UDP socket file descriptor.
 * @param config Pointer to the application configuration.
 * @param file Pointer to the file stream to read data from.
 * @param server_addr Pointer to the server address structure.
 * @param server_addr_len Length of the server address structure.
 * @param recv_header Pointer to the header structure used for receiving packets.
 * @param connection Pointer to the connection state structure.
 * @param last_progress_ms Pointer to the timestamp of the last successful progress.
 * @param exit_code Pointer to the application exit code.
 * @return ClientState The next state of the client (CLIENT_FIN_WAIT on completion).
 */
static ClientState handle_established(
    int my_socket,
    AppConfig *config,
    FILE *file,
    struct sockaddr_storage *server_addr,
    socklen_t server_addr_len,
    RDTHeader *recv_header,
    ConnectionState *connection,
    uint64_t *last_progress_ms,
    int *exit_code
)
{
    if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
        fprintf(stderr, "Error: no protocol progress during data transfer.\n");
        *exit_code = EX_PROTOCOL;
        return CLIENT_DONE;
    }

    GBNSenderState gbn_state = {0};
    gbn_state.base = connection->seq_num;
    gbn_state.nextseqnum = connection->seq_num;

    int eof_reached = 0;

    uint32_t last_ack = 0;
    int dup_ack_count = 0;

    while (!eof_reached || gbn_state.base < gbn_state.nextseqnum) {
        if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
            fprintf(stderr, "Error: no protocol progress during data transfer.\n");
            *exit_code = EX_PROTOCOL;
            return CLIENT_DONE;
        }

        if (!eof_reached) {
            int fill_res = gbn_fill_and_send_window(
                &gbn_state,
                file,
                my_socket,
                server_addr,
                server_addr_len,
                connection
            );

            if (fill_res < 0) {
                fprintf(stderr, "Error: failed while reading input stream.\n");
                *exit_code = EX_IOERR;
                return CLIENT_DONE;
            }

            if (fill_res > 0) {
                eof_reached = 1;
            }
        }

        if (eof_reached && gbn_state.base == gbn_state.nextseqnum) {
            break;
        }

        int wait_ms = RETRANSMIT_INTERVAL_MS;
        if (gbn_state.base == gbn_state.nextseqnum) {
            wait_ms = 10;
        } 
        else {
            uint64_t now_ms = monotonic_time_ms();
            if (gbn_state.window_timer_ms == 0) {
                gbn_state.window_timer_ms = now_ms;
            }

            uint64_t elapsed_ms = now_ms - gbn_state.window_timer_ms;
            if (elapsed_ms >= RETRANSMIT_INTERVAL_MS) {
                gbn_retransmit_window(&gbn_state, my_socket, server_addr, server_addr_len);
                continue;
            }

            wait_ms = (int)(RETRANSMIT_INTERVAL_MS - elapsed_ms);
            if (wait_ms < 1) {
                wait_ms = 1;
            }
        }

        int ready = wait_for_packet(my_socket, wait_ms);
        if (ready == 0) {
            continue;
        }
        if (ready < 0) {
            fprintf(stderr, "Error: socket wait failed during data transfer.\n");
            *exit_code = EX_OSERR;
            return CLIENT_DONE;
        }

        size_t recv_payload_len = 0;
        if (recv_packet(my_socket, server_addr, &server_addr_len, recv_header, NULL, &recv_payload_len) < 0) {
            continue;
        }

        if (recv_header->flags == FLAG_ACK && recv_header->conn_id == connection->conn_id) {
            
            uint32_t ack = recv_header->ack_num;

            if (gbn_process_ack(&gbn_state, ack)) {
                dup_ack_count = 0;
                last_ack = ack;
                mark_progress(last_progress_ms);
                
                if (gbn_state.base == gbn_state.nextseqnum) {
                    gbn_state.window_timer_ms = 0;
                } else {
                    gbn_state.window_timer_ms = monotonic_time_ms();
                }
            } 
            else {
                if (ack == last_ack) {
                    dup_ack_count++;
                    // Fast retransmit activation
                    if (dup_ack_count == 3) {
                        fprintf(stderr, "[CLIENT] Fast Retransmit triggered for ACK %u\n", ack);
                        gbn_retransmit_window(&gbn_state, my_socket, server_addr, server_addr_len);
                        dup_ack_count = 0;
                        gbn_state.window_timer_ms = monotonic_time_ms();
                    }
                } else {
                    last_ack = ack;
                    dup_ack_count = 1;
                }
            }
        }
    }

    fprintf(stderr, "[CLIENT] Reading input finished.\n");
    connection->seq_num = gbn_state.nextseqnum;
    return CLIENT_FIN_WAIT;
}

/**
 * @brief Handles the CLIENT_FIN_WAIT state. Sends FIN and waits for FIN+ACK.
 * 
 * @param my_socket The client UDP socket file descriptor.
 * @param config Pointer to the application configuration.
 * @param server_addr Pointer to the server address structure.
 * @param server_addr_len Length of the server address structure.
 * @param send_header Pointer to the header structure used for sending packets.
 * @param recv_header Pointer to the header structure used for receiving packets.
 * @param connection Pointer to the connection state structure.
 * @param last_progress_ms Pointer to the timestamp of the last successful progress.
 * @param exit_code Pointer to the application exit code.
 * @return ClientState The next state of the client (CLIENT_TIME_WAIT on success).
 */
static ClientState handle_fin_wait(
    int my_socket,
    AppConfig *config,
    struct sockaddr_storage *server_addr,
    socklen_t server_addr_len,
    RDTHeader *send_header,
    RDTHeader *recv_header,
    ConnectionState *connection,
    uint64_t *last_progress_ms,
    int *exit_code
)
{   
    if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
        fprintf(stderr, "Error: no protocol progress during termination.\n");
        *exit_code = EX_PROTOCOL;
        return CLIENT_DONE;
    }

    fprintf(stderr, "[CLIENT] Sending FIN and ending.\n");

    fill_header(send_header, connection->seq_num, connection->ack_num,
                connection->conn_id, FLAG_FIN, 0);

    if (send_packet(my_socket, server_addr, server_addr_len, send_header, NULL, 0) < 0) {
        fprintf(stderr, "Error: failed to send FIN packet.\n");
        *exit_code = EX_OSERR;
        return CLIENT_DONE;
    }

    int ready = wait_for_packet(my_socket, RETRANSMIT_INTERVAL_MS);

    if (ready == 0) {
        if (progress_timeout_reached(*last_progress_ms, config->timeout)) {
            fprintf(stderr, "Error: no protocol progress during termination.\n");
            *exit_code = EX_PROTOCOL;
            return CLIENT_DONE;
        }

        fprintf(stderr, "[CLIENT] Timeout while waiting for FIN+ACK. Trying again...\n");
        return CLIENT_FIN_WAIT;
    } 
    else if (ready < 0) {
        fprintf(stderr, "Error: socket wait failed during termination.\n");
        *exit_code = EX_OSERR;
        return CLIENT_DONE;
    }

    size_t recv_payload_len = 0;
    if (recv_packet(my_socket, server_addr, &server_addr_len, recv_header, NULL, &recv_payload_len) < 0) {
        return CLIENT_FIN_WAIT;
    }

    if (recv_header->flags == (FLAG_FIN | FLAG_ACK) &&
        recv_header->conn_id == connection->conn_id &&
        recv_header->ack_num == (connection->seq_num + 1)) {
        fprintf(stderr, "[CLIENT] Accepted FIN+ACK. Sending ACK.\n");

        connection->ack_num = recv_header->seq_num + 1; 
        connection->seq_num += 1; 
        mark_progress(last_progress_ms);

        fill_header(send_header, connection->seq_num, connection->ack_num, connection->conn_id, FLAG_ACK, 0);

        if (send_packet(my_socket, server_addr, server_addr_len, send_header, NULL, 0) < 0) {
            fprintf(stderr, "Error: failed to send handshake ACK packet.\n");
            *exit_code = EX_OSERR;
            return CLIENT_DONE;
        }

        return CLIENT_TIME_WAIT;
    }
    return CLIENT_FIN_WAIT;
}

/**
 * @brief Handles the CLIENT_TIME_WAIT state. Waits to catch and respond to retransmitted FIN+ACKs.
 * 
 * @param my_socket The client UDP socket file descriptor.
 * @param server_addr Pointer to the server address structure.
 * @param server_addr_len Length of the server address structure.
 * @param send_header Pointer to the header structure used for sending packets.
 * @param recv_header Pointer to the header structure used for receiving packets.
 * @param connection Pointer to the connection state structure.
 * @param last_progress_ms Pointer to the timestamp of the last successful progress.
 * @param exit_code Pointer to the application exit code.
 * @return ClientState The next state of the client (CLIENT_DONE after timeout).
 */
static ClientState handle_time_wait(
    int my_socket,
    struct sockaddr_storage *server_addr,
    socklen_t server_addr_len,
    RDTHeader *send_header,
    RDTHeader *recv_header,
    ConnectionState *connection,
    uint64_t *last_progress_ms,
    int *exit_code
)
{
    int ready = wait_for_packet(my_socket, TIME_WAIT_DURATION_MS);

    if (ready == 0) {
        return CLIENT_DONE;
    }
    else if (ready < 0) {
        if (stop_requested) {
            *exit_code = EX_OK;
            return CLIENT_DONE;
        }
        fprintf(stderr, "Error: socket wait failed during TIME_WAIT.\n");
        *exit_code = EX_OSERR;
        return CLIENT_DONE;
    }

    size_t recv_payload_len = 0;
    if (recv_packet(my_socket, server_addr, &server_addr_len, recv_header, NULL, &recv_payload_len) < 0) {
        return CLIENT_TIME_WAIT;
    }

    if (recv_header->flags == (FLAG_FIN | FLAG_ACK) &&
        recv_header->conn_id == connection->conn_id &&
        recv_header->ack_num == connection->seq_num) {
        fprintf(stderr, "[CLIENT] Server re-sent FIN+ACK during TIME_WAIT. Re-sending final ACK.\n");

        fill_header(send_header, connection->seq_num, connection->ack_num, connection->conn_id, FLAG_ACK, 0);
        if (send_packet(my_socket, server_addr, server_addr_len, send_header, NULL, 0) < 0) {
            fprintf(stderr, "Error: failed to re-send final ACK during TIME_WAIT.\n");
            *exit_code = EX_OSERR;
            return CLIENT_DONE;
        }

        mark_progress(last_progress_ms);
    }

    return CLIENT_TIME_WAIT;
}


/**
 * @brief Main entry point for the client process. Executes the state machine.
 * 
 * @param config Pointer to the runtime configuration structure.
 * @return int Exit code (EXIT_SUCCESS on success, EXIT_FAILURE on failure).
 */
int run_client(AppConfig *config) {
    struct sockaddr_storage server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    int exit_code = EX_OK;

    int my_socket = setup_client_socket(config, &server_addr, &server_addr_len);
    if (my_socket < 0) {
        return -my_socket;
    } 

    // Initialization
    // * * * *
    RDTHeader send_header = {0};
    RDTHeader recv_header = {0};

    // Start STATE
    // * * * *
    ClientState state = CLIENT_CLOSED;

    // Connection helper struct
    // * * * *
    ConnectionState connection = {0};
    uint64_t last_progress_ms = monotonic_time_ms();

    // Load given input file / use STDIN
    // * * * *
    FILE *file = get_file_stream(config->input_file, "rb", stdin);

    if (file == NULL) {
        fprintf(stderr, "Error: failed to open input file '%s'\n", config->input_file);
        close(my_socket);
        return EX_NOINPUT;
    }

    srand(time(NULL));

    while (state != CLIENT_DONE) {
        if (stop_requested) {
            fprintf(stderr, "[CLIENT] Signal received. Shutting down.\n");
            exit_code = EX_SOFTWARE;
            state = CLIENT_DONE;
            break;
        }

        // FSM Realization
        // * * * * 
        switch (state) {
            // * * * *
            case CLIENT_CLOSED: {
                state = handle_closed(my_socket, &server_addr, server_addr_len, &send_header, &connection, &exit_code);
                break;
            }
            // * * * *
            case CLIENT_SYN_SENT: {
                state = handle_syn_sent(my_socket, config, &server_addr, server_addr_len, &send_header, &recv_header, &connection, &last_progress_ms, &exit_code);
                break;
            }
            // * * * *
            case CLIENT_ESTABLISHED: {
                state = handle_established(my_socket, config, file, &server_addr, server_addr_len, &recv_header, &connection, &last_progress_ms, &exit_code);
                break;
            }
            // * * * *
            case CLIENT_FIN_WAIT: {
                state = handle_fin_wait(my_socket, config, &server_addr, server_addr_len, 
                                        &send_header, &recv_header, &connection, &last_progress_ms, &exit_code);
                break;
            }
            // * * * *
            case CLIENT_TIME_WAIT: {
                state = handle_time_wait(my_socket, &server_addr, server_addr_len,
                                         &send_header, &recv_header, &connection,
                                         &last_progress_ms, &exit_code);
                break;
            }
            // * * * *
            default:
                state = CLIENT_DONE;
                break;
        }
    }

    if (file != stdin && fclose(file) != 0) {
        fprintf(stderr, "Error: failed to close input file.\n");
        exit_code = EX_IOERR;
    }
    if (close(my_socket) != 0) {
        fprintf(stderr, "Error: failed to close client socket.\n");
        exit_code = EX_OSERR;
    }

    return exit_code;
}