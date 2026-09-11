// Protocol unit tests using greatest.h

#include <arpa/inet.h>
#include <string.h>
#include <sysexits.h>

#include "greatest.h"
#include "protocol.h"
#include "utils.h"

TEST test_build_parse_roundtrip(void) {
	RDTHeader hdr = {0};
	uint8_t payload[4] = {0x01, 0x02, 0x03, 0x04};
	uint8_t buffer[MAX_PACKET_SIZE];

	hdr.seq_num = 123;
	hdr.ack_num = 456;
	hdr.conn_id = 789;
	hdr.flags = FLAG_DATA;

	size_t pkt_len = build_packet(&hdr, payload, sizeof(payload), buffer);

	RDTHeader out_hdr = {0};
	uint8_t out_payload[8] = {0};
	size_t out_len = 0;

	ASSERT_EQ(0, parse_packet(&out_hdr, buffer, out_payload, &out_len, pkt_len));
	ASSERT_EQ(hdr.seq_num, out_hdr.seq_num);
	ASSERT_EQ(hdr.ack_num, out_hdr.ack_num);
	ASSERT_EQ(hdr.conn_id, out_hdr.conn_id);
	ASSERT_EQ(hdr.flags, out_hdr.flags);
	ASSERT_EQ(sizeof(payload), out_hdr.payload_len);
	ASSERT_EQ(sizeof(payload), out_len);
	ASSERT_MEM_EQ(payload, out_payload, out_len);
	PASS();
}

TEST test_checksum_corruption_is_detected(void) {
	RDTHeader hdr = {0};
	uint8_t payload[4] = {0xAA, 0xBB, 0xCC, 0xDD};
	uint8_t buffer[MAX_PACKET_SIZE];

	hdr.seq_num = 1;
	hdr.ack_num = 2;
	hdr.conn_id = 3;
	hdr.flags = FLAG_DATA;

	size_t pkt_len = build_packet(&hdr, payload, sizeof(payload), buffer);
	buffer[sizeof(RDTHeader)] ^= 0xFF; // flip one byte

	RDTHeader out_hdr = {0};
	uint8_t out_payload[8] = {0};
	size_t out_len = 0;

	ASSERT_EQ(-EX_DATAERR, parse_packet(&out_hdr, buffer, out_payload, &out_len, pkt_len));
	PASS();
}

TEST test_truncated_header_is_rejected(void) {
	uint8_t buffer[10] = {0};
	RDTHeader out_hdr = {0};

	ASSERT_EQ(-EX_DATAERR, parse_packet(&out_hdr, buffer, NULL, NULL, sizeof(buffer)));
	PASS();
}

TEST test_oversized_packet_is_rejected(void) {
	uint8_t buffer[MAX_PACKET_SIZE + 1] = {0};
	RDTHeader out_hdr = {0};

	ASSERT_EQ(-EX_DATAERR, parse_packet(&out_hdr, buffer, NULL, NULL, sizeof(buffer)));
	PASS();
}

TEST test_payload_len_mismatch_is_rejected(void) {
	uint8_t buffer[MAX_PACKET_SIZE] = {0};
	uint8_t payload[5] = {1, 2, 3, 4, 5};
	RDTHeader hdr = {0};

	hdr.seq_num = htonl(10);
	hdr.ack_num = htonl(20);
	hdr.conn_id = htons(30);
	hdr.flags = FLAG_DATA;
	hdr.payload_len = htons(10); // claim 10 bytes, provide only 5
	hdr.checksum = 0;

	memcpy(buffer, &hdr, sizeof(RDTHeader));
	memcpy(buffer + sizeof(RDTHeader), payload, sizeof(payload));

	uint16_t checksum = calculate_checksum(buffer, sizeof(RDTHeader) + sizeof(payload));
	((RDTHeader *)buffer)->checksum = htons(checksum);

	RDTHeader out_hdr = {0};
	uint8_t out_payload[16] = {0};
	size_t out_len = 0;

	ASSERT_EQ(-EX_DATAERR, parse_packet(&out_hdr, buffer, out_payload, &out_len,
			sizeof(RDTHeader) + sizeof(payload)));
	PASS();
}

TEST test_network_byte_order_is_enforced(void) {
	RDTHeader hdr = {0};
	uint8_t buffer[MAX_PACKET_SIZE];

	hdr.seq_num = 0x11223344;
	hdr.ack_num = 0;
	hdr.conn_id = 0;
	hdr.flags = FLAG_SYN;

	(void)build_packet(&hdr, NULL, 0, buffer);

	ASSERT_EQ(0x11, buffer[0]);
	ASSERT_EQ(0x22, buffer[1]);
	ASSERT_EQ(0x33, buffer[2]);
	ASSERT_EQ(0x44, buffer[3]);
	PASS();
}

TEST test_control_packet_no_payload(void) {
	RDTHeader hdr = {0};
	uint8_t buffer[MAX_PACKET_SIZE];

	hdr.seq_num = 100;
	hdr.ack_num = 0;
	hdr.conn_id = 42;
	hdr.flags = FLAG_SYN;

	size_t pkt_len = build_packet(&hdr, NULL, 0, buffer);

	RDTHeader out_hdr = {0};
	size_t out_len = 0;

	ASSERT_EQ(0, parse_packet(&out_hdr, buffer, NULL, &out_len, pkt_len));
	ASSERT_EQ(FLAG_SYN, out_hdr.flags);
	ASSERT_EQ(0, out_len);
	PASS();
}

SUITE(test_protocol_suite) {
	RUN_TEST(test_build_parse_roundtrip);
	RUN_TEST(test_checksum_corruption_is_detected);
	RUN_TEST(test_truncated_header_is_rejected);
	RUN_TEST(test_oversized_packet_is_rejected);
	RUN_TEST(test_payload_len_mismatch_is_rejected);
	RUN_TEST(test_network_byte_order_is_enforced);
	RUN_TEST(test_control_packet_no_payload);
}
