#include "test_e2e_common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

double nowSeconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int findFreeUdpPort(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return 19000;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return 19000;
    }

    socklen_t len = sizeof(addr);
    getsockname(sock, (struct sockaddr *)&addr, &len);
    int port = ntohs(addr.sin_port);
    close(sock);
    return port;
}

int makeTempDir(char *buf, size_t bufSize) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = "/tmp";
    snprintf(buf, bufSize, "%s/ipk_test_XXXXXX", tmp);
    if (!mkdtemp(buf)) return -1;
    return 0;
}

void cleanupTempDir(const char *path) {
    if (!path || path[0] == '\0') return;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    (void)system(cmd);
}

int spawnProc(const char *const argv[], bool needStdinPipe,
              bool needStdoutPipe, SpawnedProc *out) {
    int stdinPipe[2]  = {-1, -1};
    int stdoutPipe[2] = {-1, -1};

    if (needStdinPipe  && pipe(stdinPipe)  < 0) return -1;
    if (needStdoutPipe && pipe(stdoutPipe) < 0) {
        if (needStdinPipe) { close(stdinPipe[0]); close(stdinPipe[1]); }
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        /* child */
        if (needStdinPipe) {
            dup2(stdinPipe[0], STDIN_FILENO);
            close(stdinPipe[0]); close(stdinPipe[1]);
        }
        if (needStdoutPipe) {
            dup2(stdoutPipe[1], STDOUT_FILENO);
            close(stdoutPipe[0]); close(stdoutPipe[1]);
        }
        /* silence stderr */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }

        execv(argv[0], (char *const *)argv);
        _exit(127);
    }

    /* parent */
    if (needStdinPipe)  { close(stdinPipe[0]); }
    if (needStdoutPipe) { close(stdoutPipe[1]); }

    out->pid      = pid;
    out->stdinFd  = needStdinPipe  ? stdinPipe[1]  : -1;
    out->stdoutFd = needStdoutPipe ? stdoutPipe[0] : -1;
    return 0;
}

bool waitProcUntil(pid_t pid, double deadline, int *outExitCode) {
    while (nowSeconds() < deadline) {
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            if (outExitCode) *outExitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            return true;
        }
        struct timespec ts = {0, 50000000}; /* 50 ms */
        nanosleep(&ts, NULL);
    }
    return false;
}

int killAndReap(pid_t pid) {
    kill(pid, SIGTERM);
    struct timespec ts = {0, 200000000};
    nanosleep(&ts, NULL);
    int status = 0;
    if (waitpid(pid, &status, WNOHANG) != pid) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

/* ------------------------------------------------------------------ *
 * runTransferCase                                                      *
 * ------------------------------------------------------------------ */
bool runTransferCase(const TransferCase *tc) {
    char tempDir[256];
    if (makeTempDir(tempDir, sizeof(tempDir)) != 0) return false;

    bool   ok        = false;
    int    port      = findFreeUdpPort();
    char   portStr[16], timeoutStr[16];
    snprintf(portStr,    sizeof(portStr),    "%d", port);
    snprintf(timeoutStr, sizeof(timeoutStr), "%d", tc->timeoutSec);

    /* ---- prepare input data ---- */
    char inputPath[512], outputPath[512];
    snprintf(inputPath,  sizeof(inputPath),  "%s/input.bin",  tempDir);
    snprintf(outputPath, sizeof(outputPath), "%s/output.bin", tempDir);

    if (tc->payloadSize > 0) {
        FILE *f = fopen(inputPath, "wb");
        if (!f) goto done;
        for (size_t i = 0; i < tc->payloadSize; i++) {
            uint8_t b = tc->binaryPayload ? (uint8_t)(i * 0x9e + 0x3d) : (uint8_t)('A' + i % 26);
            fwrite(&b, 1, 1, f);
        }
        fclose(f);
    } else {
        /* empty file */
        FILE *f = fopen(inputPath, "wb");
        if (!f) goto done;
        fclose(f);
    }

    /* ---- spawn server ---- */
    bool serverToFile = (tc->outputSink == outputToFile);
    const char *serverArgv[16];
    int si = 0;
    serverArgv[si++] = "./ipk-rdt";
    serverArgv[si++] = "-s";
    serverArgv[si++] = "-p"; serverArgv[si++] = portStr;
    serverArgv[si++] = "-w"; serverArgv[si++] = timeoutStr;
    if (tc->serverAddress) { serverArgv[si++] = "-a"; serverArgv[si++] = tc->serverAddress; }
    if (serverToFile)      { serverArgv[si++] = "-o"; serverArgv[si++] = outputPath; }
    serverArgv[si] = NULL;

    SpawnedProc server = {0};
    bool needServerStdout = !serverToFile;
    if (spawnProc(serverArgv, false, needServerStdout, &server) != 0) goto done;

    /* small delay so server is ready */
    struct timespec startDelay = {0, 80000000};
    nanosleep(&startDelay, NULL);

    /* ---- spawn client ---- */
    bool clientFromFile  = (tc->inputSource == inputFromFile);
    bool clientFromStdin = (tc->inputSource == inputFromStdin);

    const char *clientArgv[16];
    int ci = 0;
    clientArgv[ci++] = "./ipk-rdt";
    clientArgv[ci++] = "-c";
    clientArgv[ci++] = "-a"; clientArgv[ci++] = tc->host ? tc->host : "127.0.0.1";
    clientArgv[ci++] = "-p"; clientArgv[ci++] = portStr;
    clientArgv[ci++] = "-w"; clientArgv[ci++] = timeoutStr;
    if (clientFromFile) { clientArgv[ci++] = "-i"; clientArgv[ci++] = inputPath; }
    clientArgv[ci] = NULL;

    SpawnedProc client = {0};
    if (spawnProc(clientArgv, clientFromStdin, false, &client) != 0) {
        killAndReap(server.pid);
        goto done;
    }

    /* if stdin mode, pump data into client */
    if (clientFromStdin && client.stdinFd >= 0) {
        FILE *src = fopen(inputPath, "rb");
        if (src) {
            uint8_t buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
                (void)write(client.stdinFd, buf, n);
            fclose(src);
        }
        close(client.stdinFd);
        client.stdinFd = -1;
    }

    /* ---- wait for both to exit ---- */
    double deadline = nowSeconds() + (double)tc->sessionTimeoutSec;
    int clientExit = 1, serverExit = 1;

    bool clientDone = waitProcUntil(client.pid, deadline, &clientExit);
    bool serverDone = waitProcUntil(server.pid, deadline, &serverExit);

    if (!clientDone) killAndReap(client.pid);
    if (!serverDone) killAndReap(server.pid);

    if (clientExit != 0 || serverExit != 0) goto done;

    /* ---- collect server stdout if needed ---- */
    if (needServerStdout && server.stdoutFd >= 0) {
        FILE *f = fopen(outputPath, "wb");
        if (f) {
            uint8_t buf[4096]; ssize_t n;
            while ((n = read(server.stdoutFd, buf, sizeof(buf))) > 0)
                fwrite(buf, 1, (size_t)n, f);
            fclose(f);
        }
        close(server.stdoutFd);
        server.stdoutFd = -1;
    }

    /* ---- compare input vs output ---- */
    {
        FILE *fin  = fopen(inputPath,  "rb");
        FILE *fout = fopen(outputPath, "rb");
        if (!fin || !fout) { if (fin) fclose(fin); if (fout) fclose(fout); goto done; }

        ok = true;
        uint8_t b1[4096], b2[4096];
        while (true) {
            size_t n1 = fread(b1, 1, sizeof(b1), fin);
            size_t n2 = fread(b2, 1, sizeof(b2), fout);
            if (n1 != n2 || memcmp(b1, b2, n1) != 0) { ok = false; break; }
            if (n1 == 0) break;
        }
        fclose(fin); fclose(fout);
    }

done:
    cleanupTempDir(tempDir);
    return ok;
}

/* ------------------------------------------------------------------ *
 * tc netem helpers                                                     *
 * ------------------------------------------------------------------ */
bool isTcNetemUsable(void) {
    return system("tc qdisc show dev lo > /dev/null 2>&1") == 0;
}

bool applyTcNetem(const char *spec) {
    char cmd[256];
    system("tc qdisc del dev lo root 2>/dev/null");
    snprintf(cmd, sizeof(cmd),
             "tc qdisc replace dev lo root netem %s 2>/dev/null", spec);
    return system(cmd) == 0;
}

void clearTcNetem(void) {
    system("tc qdisc del dev lo root 2>/dev/null");
}
