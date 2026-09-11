// CLI parsing tests for parse_arguments using greatest.h

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sysexits.h>

#include "config.h"
#include "parser.h"
#include "greatest.h"

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

static void reset_getopt_state(void) {
	optind = 1;
	opterr = 0;
	optopt = 0;
}

static int expect_parse_exit(int expected_exit, int argc, char **argv) {
	pid_t pid = fork();
	if (pid < 0) {
        perror("fork failed in test");
		return 1;
	}

	if (pid == 0) {
		SilenceState silence = { .stdout_fd = -1, .stderr_fd = -1, .devnull_fd = -1 };
		(void)silence_stdio(&silence);
		reset_getopt_state();
		(void)parse_arguments(argc, argv);
		restore_stdio(&silence);
		_exit(2);
	}

	int status = 0;
	if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid failed in test");
		return 1;
	}

	if (!WIFEXITED(status)) {
		return 1;
	}

	return WEXITSTATUS(status) == expected_exit ? 0 : 1;
}

TEST test_help_short_returns_zero(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-h"};
	ASSERT_EQ(0, expect_parse_exit(0, 2, argv));
	PASS();
}

TEST test_help_long_returns_zero(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"--help"};
	ASSERT_EQ(0, expect_parse_exit(0, 2, argv));
	PASS();
}

TEST test_valid_client_defaults(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"9000"};
	reset_getopt_state();
	AppConfig cfg = parse_arguments(6, argv);

	ASSERT(cfg.is_client);
	ASSERT(!cfg.is_server);
	ASSERT_EQ(9000, cfg.port);
	ASSERT_EQ(1, cfg.timeout);
	ASSERT(cfg.address != NULL);
	ASSERT_EQ(0, strcmp(cfg.address, "127.0.0.1"));
	ASSERT(cfg.input_file == NULL);
	ASSERT(cfg.output_file == NULL);
	PASS();
}

TEST test_valid_client_full_any_order(void) {
	char *argv[] = {
		(char *)"./ipk-rdt",
		(char *)"-p", (char *)"65535",
		(char *)"-i", (char *)"input.bin",
		(char *)"-c",
		(char *)"-a", (char *)"localhost",
		(char *)"-w", (char *)"7"
	};

	reset_getopt_state();
	AppConfig cfg = parse_arguments(11, argv);

	ASSERT(cfg.is_client);
	ASSERT(!cfg.is_server);
	ASSERT_EQ(65535, cfg.port);
	ASSERT_EQ(7, cfg.timeout);
	ASSERT(cfg.address != NULL);
	ASSERT_EQ(0, strcmp(cfg.address, "localhost"));
	ASSERT(cfg.input_file != NULL);
	ASSERT_EQ(0, strcmp(cfg.input_file, "input.bin"));
	ASSERT(cfg.output_file == NULL);
	PASS();
}

TEST test_valid_server_defaults(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-s", (char *)"-p", (char *)"0"};
	reset_getopt_state();
	AppConfig cfg = parse_arguments(4, argv);

	ASSERT(cfg.is_server);
	ASSERT(!cfg.is_client);
	ASSERT_EQ(0, cfg.port);
	ASSERT_EQ(1, cfg.timeout);
	ASSERT(cfg.address == NULL);
	ASSERT(cfg.input_file == NULL);
	ASSERT(cfg.output_file == NULL);
	PASS();
}

TEST test_valid_server_full_any_order(void) {
	char *argv[] = {
		(char *)"./ipk-rdt",
		(char *)"-o", (char *)"output.bin",
		(char *)"-a", (char *)"::1",
		(char *)"-p", (char *)"1234",
		(char *)"-w", (char *)"9",
		(char *)"-s"
	};

	reset_getopt_state();
	AppConfig cfg = parse_arguments(11, argv);

	ASSERT(cfg.is_server);
	ASSERT(!cfg.is_client);
	ASSERT_EQ(1234, cfg.port);
	ASSERT_EQ(9, cfg.timeout);
	ASSERT(cfg.address != NULL);
	ASSERT_EQ(0, strcmp(cfg.address, "::1"));
	ASSERT(cfg.output_file != NULL);
	ASSERT_EQ(0, strcmp(cfg.output_file, "output.bin"));
	ASSERT(cfg.input_file == NULL);
	PASS();
}

TEST test_missing_mode_is_rejected(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"9000"};
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 5, argv));
	PASS();
}

TEST test_both_modes_are_rejected(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-s", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"9000"};
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 7, argv));
	PASS();
}

TEST test_client_missing_host_is_rejected(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-p", (char *)"9000"};
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 4, argv));
	PASS();
}

TEST test_client_missing_port_is_rejected(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1"};
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 4, argv));
	PASS();
}

TEST test_client_rejects_output_file(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"9000", (char *)"-o", (char *)"out.bin"};
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 8, argv));
	PASS();
}

TEST test_server_missing_port_is_rejected(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-s"};
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 2, argv));
	PASS();
}

TEST test_server_rejects_input_file(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-s", (char *)"-p", (char *)"9000", (char *)"-i", (char *)"input.bin"};
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 6, argv));
	PASS();
}

TEST test_unknown_flag_is_rejected(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-x"};
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 2, argv));
	PASS();
}

TEST test_dangling_value_flags_are_rejected(void) {
	char *argv1[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a"};
	char *argv2[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p"};
	char *argv3[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"9000", (char *)"-w"};
	char *argv4[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"9000", (char *)"-i"};
	char *argv5[] = {(char *)"./ipk-rdt", (char *)"-s", (char *)"-p", (char *)"9000", (char *)"-o"};

	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 3, argv1));
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 5, argv2));
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 7, argv3));
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 7, argv4));
	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 5, argv5));
	PASS();
}

TEST test_explicit_stdio_hyphen_is_accepted(void) {
	char *argv_client[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"9000", (char *)"-i", (char *)"-"};
	char *argv_server[] = {(char *)"./ipk-rdt", (char *)"-s", (char *)"-p", (char *)"9000", (char *)"-o", (char *)"-"};

	reset_getopt_state();
	AppConfig c = parse_arguments(8, argv_client);
	ASSERT(c.is_client);
	ASSERT(c.input_file != NULL);
	ASSERT_EQ(0, strcmp(c.input_file, "-"));

	reset_getopt_state();
	AppConfig s = parse_arguments(6, argv_server);
	ASSERT(s.is_server);
	ASSERT(s.output_file != NULL);
	ASSERT_EQ(0, strcmp(s.output_file, "-"));
	PASS();
}

TEST test_invalid_port_values_are_rejected(void) {
	char *argv1[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"-1"};
	char *argv2[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"65536"};
	char *argv3[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"abc"};

	ASSERT_EQ(0, expect_parse_exit(EX_USAGE, 6, argv1));
	ASSERT_EQ(0, expect_parse_exit(EX_DATAERR, 6, argv2));
	ASSERT_EQ(0, expect_parse_exit(EX_DATAERR, 6, argv3));
	PASS();
}

TEST test_invalid_timeout_values_are_rejected(void) {
	char *argv1[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"9000", (char *)"-w", (char *)"0"};
	char *argv2[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"9000", (char *)"-w", (char *)"-1"};
	char *argv3[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"9000", (char *)"-w", (char *)"abc"};

	ASSERT_EQ(0, expect_parse_exit(EX_DATAERR, 8, argv1));
	ASSERT_EQ(0, expect_parse_exit(EX_DATAERR, 8, argv2));
	ASSERT_EQ(0, expect_parse_exit(EX_DATAERR, 8, argv3));
	PASS();
}

TEST test_overflow_numbers_are_rejected(void) {
	char *argv1[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"999999999999999999999"};
	char *argv2[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"9000", (char *)"-w", (char *)"999999999999999999999"};

	ASSERT_EQ(0, expect_parse_exit(EX_DATAERR, 6, argv1));
	ASSERT_EQ(0, expect_parse_exit(EX_DATAERR, 8, argv2));
	PASS();
}

TEST test_duplicate_arguments_last_wins(void) {
	char *argv[] = {(char *)"./ipk-rdt", (char *)"-c", (char *)"-a", (char *)"127.0.0.1", (char *)"-p", (char *)"8000", (char *)"-p", (char *)"9000"};
	reset_getopt_state();
	AppConfig cfg = parse_arguments(8, argv);

	ASSERT(cfg.is_client);
	ASSERT_EQ(9000, cfg.port);
	PASS();
}

SUITE(test_cli_parsing) {
	RUN_TEST(test_help_short_returns_zero);
	RUN_TEST(test_help_long_returns_zero);
	RUN_TEST(test_valid_client_defaults);
	RUN_TEST(test_valid_client_full_any_order);
	RUN_TEST(test_valid_server_defaults);
	RUN_TEST(test_valid_server_full_any_order);
	RUN_TEST(test_missing_mode_is_rejected);
	RUN_TEST(test_both_modes_are_rejected);
	RUN_TEST(test_client_missing_host_is_rejected);
	RUN_TEST(test_client_missing_port_is_rejected);
	RUN_TEST(test_client_rejects_output_file);
	RUN_TEST(test_server_missing_port_is_rejected);
	RUN_TEST(test_server_rejects_input_file);
	RUN_TEST(test_unknown_flag_is_rejected);
	RUN_TEST(test_dangling_value_flags_are_rejected);
	RUN_TEST(test_explicit_stdio_hyphen_is_accepted);
	RUN_TEST(test_invalid_port_values_are_rejected);
	RUN_TEST(test_invalid_timeout_values_are_rejected);
	RUN_TEST(test_overflow_numbers_are_rejected);
	RUN_TEST(test_duplicate_arguments_last_wins);
}
