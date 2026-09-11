#ifndef TEST_E2E_COMMON_H
#define TEST_E2E_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include "greatest.h"

typedef enum { inputFromFile, inputFromStdin } InputSource;
typedef enum { outputToFile, outputToStdout } OutputSink;

typedef struct {
    const char* host;
    const char* serverAddress;
    int timeoutSec;
    int sessionTimeoutSec;
    size_t payloadSize;
    bool binaryPayload;
    InputSource inputSource;
    OutputSink outputSink;
} TransferCase;

typedef struct {
    pid_t pid;
    int stdinFd;
    int stdoutFd;
} SpawnedProc;

double nowSeconds(void);
int findFreeUdpPort(void);
int makeTempDir(char* buf, size_t bufSize);
void cleanupTempDir(const char* path);
int spawnProc(const char* const argv[], bool needStdinPipe, bool needStdoutPipe, SpawnedProc* out);
bool waitProcUntil(pid_t pid, double deadline, int* outExitCode);
int killAndReap(pid_t pid);
bool runTransferCase(const TransferCase* tc);

bool isTcNetemUsable(void);
bool applyTcNetem(const char* spec);
void clearTcNetem(void);

#endif