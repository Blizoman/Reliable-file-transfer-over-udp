// GBN window unit tests using greatest.h

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "greatest.h"
#include "window.h"

typedef struct {
	int stdout_fd;
	int stderr_fd;
	int devnull_fd;
} SilenceState;

static int silence_stdio(SilenceState *state) {
	if (!state) {
		return -1;
	}

	fflush(stdout);
	fflush(stderr);

	state->stdout_fd = dup(STDOUT_FILENO);
	state->stderr_fd = dup(STDERR_FILENO);
	state->devnull_fd = open("/dev/null", O_WRONLY);
	if (state->stdout_fd < 0 || state->stderr_fd < 0 || state->devnull_fd < 0) {
		return -1;
	}

	if (dup2(state->devnull_fd, STDOUT_FILENO) < 0) {
		return -1;
	}
	if (dup2(state->devnull_fd, STDERR_FILENO) < 0) {
		return -1;
	}

	return 0;
}

static void restore_stdio(SilenceState *state) {
	if (!state) {
		return;
	}

	fflush(stdout);
	fflush(stderr);

	if (state->stdout_fd >= 0) {
		dup2(state->stdout_fd, STDOUT_FILENO);
		close(state->stdout_fd);
	}
	if (state->stderr_fd >= 0) {
		dup2(state->stderr_fd, STDERR_FILENO);
		close(state->stderr_fd);
	}
	if (state->devnull_fd >= 0) {
		close(state->devnull_fd);
	}
}

static int open_udp_socket(void) {
	return socket(AF_INET, SOCK_DGRAM, 0);
}

static void fill_loopback_addr(struct sockaddr_storage *out, socklen_t *out_len, int port) {
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
	memcpy(out, &addr, sizeof(addr));
	*out_len = sizeof(addr);
}

static FILE *make_tmpfile_with_size(size_t size, uint8_t seed) {
	FILE *f = tmpfile();
	if (!f) {
		return NULL;
	}

	unsigned char buf[256];
	for (size_t i = 0; i < sizeof(buf); i++) {
		buf[i] = (uint8_t)(seed + i);
	}

	size_t remaining = size;
	while (remaining > 0) {
		size_t chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
		if (fwrite(buf, 1, chunk, f) != chunk) {
			fclose(f);
			return NULL;
		}
		remaining -= chunk;
	}

	rewind(f);
	return f;
}

TEST test_fill_window_single_packet_sets_header(void) {
	GBNSenderState state = {0};
	ConnectionState conn = {0};
	conn.ack_num = 10;
	conn.conn_id = 42;

	int sock = open_udp_socket();
	ASSERT(sock >= 0);

	struct sockaddr_storage addr;
	socklen_t addr_len = 0;
	fill_loopback_addr(&addr, &addr_len, 9);

	size_t payload_cap = MAX_PACKET_SIZE - sizeof(RDTHeader);
	FILE *f = make_tmpfile_with_size(payload_cap / 2, 0x10);
	ASSERT(f != NULL);

	int eof = gbn_fill_and_send_window(&state, f, sock, &addr, addr_len, &conn);
	ASSERT_EQ(1, eof);
	ASSERT_EQ(1, state.nextseqnum);
	ASSERT_EQ(0, state.base);
	ASSERT(state.buffer[0].is_valid);
	ASSERT_EQ(0, state.buffer[0].header.seq_num);
	ASSERT_EQ(conn.ack_num, state.buffer[0].header.ack_num);
	ASSERT_EQ(conn.conn_id, state.buffer[0].header.conn_id);
	ASSERT_EQ(FLAG_DATA, state.buffer[0].header.flags);
	ASSERT_EQ(payload_cap / 2, state.buffer[0].payload_len);

	fclose(f);
	close(sock);
	PASS();
}

TEST test_fill_window_respects_window_size(void) {
	GBNSenderState state = {0};
	ConnectionState conn = {0};
	conn.ack_num = 3;
	conn.conn_id = 7;

	int sock = open_udp_socket();
	ASSERT(sock >= 0);

	struct sockaddr_storage addr;
	socklen_t addr_len = 0;
	fill_loopback_addr(&addr, &addr_len, 9);

	size_t payload_cap = MAX_PACKET_SIZE - sizeof(RDTHeader);
	FILE *f = make_tmpfile_with_size(payload_cap * (WINDOW_SIZE + 2), 0x20);
	ASSERT(f != NULL);

	int eof = gbn_fill_and_send_window(&state, f, sock, &addr, addr_len, &conn);
	ASSERT_EQ(0, eof);
	ASSERT_EQ(WINDOW_SIZE, state.nextseqnum);

	for (int i = 0; i < WINDOW_SIZE; i++) {
		ASSERT(state.buffer[i].is_valid);
	}

	fclose(f);
	close(sock);
	PASS();
}

TEST test_fill_window_empty_file(void) {
	GBNSenderState state = {0};
	ConnectionState conn = {0};
	conn.ack_num = 1;
	conn.conn_id = 2;

	int sock = open_udp_socket();
	ASSERT(sock >= 0);

	struct sockaddr_storage addr;
	socklen_t addr_len = 0;
	fill_loopback_addr(&addr, &addr_len, 9);

	FILE *f = make_tmpfile_with_size(0, 0x00);
	ASSERT(f != NULL);

	int eof = gbn_fill_and_send_window(&state, f, sock, &addr, addr_len, &conn);
	ASSERT_EQ(1, eof);
	ASSERT_EQ(0, state.nextseqnum);
	ASSERT_EQ(0, state.base);

	fclose(f);
	close(sock);
	PASS();
}

TEST test_process_ack_advances_base(void) {
	GBNSenderState state = {0};
	state.base = 0;
	state.nextseqnum = 3;

	for (int i = 0; i < 3; i++) {
		state.buffer[i].is_valid = true;
	}

	ASSERT_EQ(1, gbn_process_ack(&state, 2));
	ASSERT_EQ(2, state.base);
	ASSERT(!state.buffer[0].is_valid);
	ASSERT(!state.buffer[1].is_valid);
	ASSERT(state.buffer[2].is_valid);
	PASS();
}

TEST test_process_ack_out_of_range_no_change(void) {
	GBNSenderState state = {0};
	state.base = 5;
	state.nextseqnum = 8;
	state.buffer[5 % WINDOW_SIZE].is_valid = true;

	ASSERT_EQ(0, gbn_process_ack(&state, 5));
	ASSERT_EQ(5, state.base);
	ASSERT(state.buffer[5 % WINDOW_SIZE].is_valid);

	ASSERT_EQ(0, gbn_process_ack(&state, 9));
	ASSERT_EQ(5, state.base);
	ASSERT(state.buffer[5 % WINDOW_SIZE].is_valid);
	PASS();
}

TEST test_retransmit_empty_window_noop(void) {
	GBNSenderState state = {0};
	state.base = 0;
	state.nextseqnum = 0;
	state.window_timer_ms = 1234;

	int sock = open_udp_socket();
	ASSERT(sock >= 0);

	struct sockaddr_storage addr;
	socklen_t addr_len = 0;
	fill_loopback_addr(&addr, &addr_len, 9);

	gbn_retransmit_window(&state, sock, &addr, addr_len);
	ASSERT_EQ(1234, state.window_timer_ms);

	close(sock);
	PASS();
}

TEST test_retransmit_sets_timer(void) {
	GBNSenderState state = {0};
	state.base = 0;
	state.nextseqnum = 1;
	state.window_timer_ms = 0;
	state.buffer[0].is_valid = true;
	state.buffer[0].payload_len = 1;
	state.buffer[0].header.seq_num = 0;
	state.buffer[0].header.ack_num = 0;
	state.buffer[0].header.conn_id = 1;
	state.buffer[0].header.flags = FLAG_DATA;

	int sock = open_udp_socket();
	ASSERT(sock >= 0);

	struct sockaddr_storage addr;
	socklen_t addr_len = 0;
	fill_loopback_addr(&addr, &addr_len, 9);

	SilenceState silence = { .stdout_fd = -1, .stderr_fd = -1, .devnull_fd = -1 };
	(void)silence_stdio(&silence);
	gbn_retransmit_window(&state, sock, &addr, addr_len);
	restore_stdio(&silence);
	ASSERT(state.window_timer_ms != 0);

	close(sock);
	PASS();
}

SUITE(test_window_suite) {
	RUN_TEST(test_fill_window_single_packet_sets_header);
	RUN_TEST(test_fill_window_respects_window_size);
	RUN_TEST(test_fill_window_empty_file);
	RUN_TEST(test_process_ack_advances_base);
	RUN_TEST(test_process_ack_out_of_range_no_change);
	RUN_TEST(test_retransmit_empty_window_noop);
	RUN_TEST(test_retransmit_sets_timer);
}
