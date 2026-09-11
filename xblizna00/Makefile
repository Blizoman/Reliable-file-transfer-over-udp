######################################################
# Made by xblizna00 -> modified trough first project #
# 2026.04.11										 #
######################################################
CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Werror -D_POSIX_C_SOURCE=200112L -D_DEFAULT_SOURCE
EXECUTABLE = ipk-rdt

SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:.c=.o)
TEST_CFLAGS = $(CFLAGS) -Isrc -Itests
TEST_SOURCES = $(wildcard tests/*.c)
TEST_LIB_SOURCES = src/parser.c src/protocol.c src/utils.c src/window.c
TEST_BIN = tests/tests_results

#### 
PROXY_SRC = test_proxy.c
PROXY_BIN = test_proxy
####

VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=all --error-exitcode=1
VM_PORT = 2222
VM_USER = student
VM_HOST = localhost
VM_DEST = /home/student/ipk-projekt2/
SSH_OPTS = -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null

.PHONY: all clean test valgrind NixDevShellName sync run-vm format

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXECUTABLE)
	chmod +x $(EXECUTABLE)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@


$(TEST_BIN): $(TEST_SOURCES) $(TEST_LIB_SOURCES)
	$(CC) $(TEST_CFLAGS) $^ -o $@

#####
$(PROXY_BIN): $(PROXY_SRC)
	$(CC) -std=c17 -Wall -Wextra -D_POSIX_C_SOURCE=200809L $< -o $@ -lpthread

proxy: $(PROXY_BIN)
#####

NixDevShellName:
	@echo "c"

# ------------ Tests ------------
test: all $(TEST_BIN)
	./$(TEST_BIN)

# ------------ Tools ------------
valgrind: $(EXECUTABLE)
	valgrind $(VALGRIND_FLAGS) ./$(EXECUTABLE) -h

clean:
	rm -f $(EXECUTABLE) src/*.o $(TEST_BIN)

sync:
	ssh -p $(VM_PORT) $(SSH_OPTS) $(VM_USER)@$(VM_HOST) "mkdir -p $(VM_DEST)"
	scp -r -P $(VM_PORT) $(SSH_OPTS) Makefile src tests $(PROXY_SRC) $(VM_USER)@$(VM_HOST):$(VM_DEST)

run-vm:
	nix run --refresh "git+https://git.fit.vutbr.cz/NESFIT/dev-envs.git#run-vm-qemu" -- --no-build --no-reset

ssh:
	ssh -p $(VM_PORT) $(SSH_OPTS) $(VM_USER)@$(VM_HOST)

format:
	clang-format -i src/*.c src/*.h