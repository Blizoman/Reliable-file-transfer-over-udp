/**
 * @file        protocol.c
 * @brief       Packet building and parsing implementation
 * @author      Andrej Bližnák (xblizna00)
 * @date        30.4.2026
 * @details     Implements serialization and deserialization of network packets,
 *              including host-to-network byte order conversion and checksum
 * validation.
 */

#include <arpa/inet.h>
#include <string.h>
#include <sysexits.h>

#include "protocol.h"
#include "utils.h"

size_t build_packet(RDTHeader *header, uint8_t *payload, uint16_t payload_len,
                    uint8_t *out_buffer) {
  // Setup/convertion of ENDIANESS
  // * * * *
  RDTHeader net_header = {
      .seq_num = htonl(header->seq_num),
      .ack_num = htonl(header->ack_num),
      .conn_id = htons(header->conn_id),
      .payload_len = htons(payload_len),
      .flags = header->flags,
      .checksum = 0, // Set to 0, later calculated
  };

  // First, copy header to buff.: [-HEADER-]
  // * * * *
  memcpy(out_buffer, &net_header, sizeof(RDTHeader));

  // Then, add data.: [-HEADER-DATA-]
  // * * * *
  if (payload_len > 0 && payload != NULL) {
    memcpy(out_buffer + sizeof(RDTHeader), payload, payload_len);
  }

  // Compute checksum (reused from first project -> reused from (RFC 1071)
  uint16_t checksum =
      calculate_checksum(out_buffer, sizeof(RDTHeader) + payload_len);

  // Insert calculated checksum directly into already built packet
  ((RDTHeader *)out_buffer)->checksum = htons(checksum);

  return sizeof(RDTHeader) + payload_len;
}

int parse_packet(RDTHeader *out_header, uint8_t *in_buffer,
                 uint8_t *out_payload, size_t *out_payload_len,
                 size_t total_len) {
  // Corrupted packet
  // * * * *
  if (total_len < sizeof(RDTHeader)) {
    return -EX_DATAERR;
  }

  if (total_len > MAX_PACKET_SIZE) {
    return -EX_DATAERR;
  }

  // Copy orginal pkt info (buff)
  // * * * *
  uint8_t tmp_buff[MAX_PACKET_SIZE];
  memcpy(tmp_buff, in_buffer, total_len);

  // Save original checksum from buff before reset
  // * * * *
  RDTHeader *tmp_header = (RDTHeader *)tmp_buff;
  uint16_t obtained_checksum = ntohs(tmp_header->checksum);
  tmp_header->checksum = 0;

  uint16_t checksum = calculate_checksum(tmp_buff, total_len);

  // CHECK
  // Integrity (re-build)
  // * * * *
  if (checksum != obtained_checksum) {
    return -EX_DATAERR;
  }

  // Loading header from buff
  // * * * *
  RDTHeader net_header;
  memcpy(&net_header, in_buffer, sizeof(RDTHeader));

  // Endian convertion
  // * * * *
  out_header->seq_num = ntohl(net_header.seq_num);
  out_header->ack_num = ntohl(net_header.ack_num);
  out_header->conn_id = ntohs(net_header.conn_id);
  out_header->payload_len = ntohs(net_header.payload_len);
  out_header->flags = net_header.flags;
  out_header->checksum = obtained_checksum;

  // Payload lenght
  // * * * *
  size_t payload_len = out_header->payload_len;

  // CHECK
  // Lenght contorl of pkt
  // * * * *
  if (total_len < sizeof(RDTHeader) + payload_len) {
    return -EX_DATAERR;
  }

  // Extract the payload if data is present
  // * * * *
  if (payload_len > 0) {
    if (out_payload != NULL) {
      memcpy(out_payload, in_buffer + sizeof(RDTHeader), payload_len);
    }
  }

  // Set the output length pointer
  // * * * *
  if (out_payload_len != NULL) {
    *out_payload_len = payload_len;
  }

  return 0;
}