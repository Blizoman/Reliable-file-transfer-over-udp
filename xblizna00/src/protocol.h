/**
 * @file        protocol.h
 * @brief       Protocol definitions and structures for IPK Reliable Data
 * Transfer
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     Defines the custom RDT header structure, protocol flags, and
 * connection state. Provides function prototypes for packet serialization and
 * deserialization.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define FLAG_SYN (1 << 0)
#define FLAG_ACK (1 << 1)
#define FLAG_FIN (1 << 2)
#define FLAG_DATA (1 << 3)
#define MAX_PACKET_SIZE 1200 // Max allowed UDP payload size

//============================================================
//+-+-+-+-+-+-+-+-+-+Custom Header Struct+-+-+-+-+-+-+-+-+-+-+
//============================================================
typedef struct __attribute__((packed)) {
  uint32_t seq_num;     // 4 B
  uint32_t ack_num;     // 4 B
  uint16_t conn_id;     // 2 B
  uint16_t checksum;    // 2 B
  uint16_t payload_len; // 2 B
  uint8_t flags;        // 1 B
                        //--------------------> 15 B
                        // Used `__attribute__`, so there is no padding
} RDTHeader;

//============================================================
//+-+-+-+-+-+-+-+-+Connection State Struct+-+-+-+-+-+-+-+-+-+-
//============================================================
typedef struct {
  uint32_t seq_num; // Current sequence number
  uint32_t ack_num; // Expected sequence number / Acknowledgment
  uint16_t conn_id; // Unique connection identifier
} ConnectionState;

//============================================================
//+-+-+-+-+-+-+-+-+-+Packet Functions+-+-+-+-+-+-+-+-+-+-+-+-+
//============================================================

//========SERIALIZATION========
/**
 * @brief Build a wire-format packet from header and payload.
 * @param header Packet header in host byte order.
 * @param payload Payload buffer (may be NULL if payload_len is 0).
 * @param payload_len Payload length in bytes.
 * @param out_buffer Output buffer for the serialized packet.
 * @return Total serialized packet length in bytes.
 */
size_t build_packet(RDTHeader *header, uint8_t *payload, uint16_t payload_len,
                    uint8_t *out_buffer);

//=======DESERIALIZATION=======
/**
 * @brief Parse a wire-format packet into header and payload.
 * @param out_header Output header in host byte order.
 * @param in_buffer Input buffer containing the packet.
 * @param out_payload Output payload buffer (may be NULL if payload_len is 0).
 * @param out_payload_len Output payload length.
 * @param total_len Total packet length in bytes.
 * @return 0 on success, negative on parse or integrity errors.
 */
int parse_packet(RDTHeader *out_header, uint8_t *in_buffer,
                 uint8_t *out_payload, size_t *out_payload_len,
                 size_t total_len);

#endif