// E2E data transfer tests
// Tests: correct byte-for-byte transfer under various payload sizes and I/O modes.

#include "greatest.h"
#include "test_e2e_common.h"

/* ------------------------------------------------------------------ */
/* Basic correctness                                                    */
/* ------------------------------------------------------------------ */

TEST testEmptyInput(void) {
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 0,
        .timeoutSec = 2, .sessionTimeoutSec = 10,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

TEST testSingleByte(void) {
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 1,
        .timeoutSec = 2, .sessionTimeoutSec = 10,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

TEST testSmallTextPayload(void) {
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 512,
        .timeoutSec = 2, .sessionTimeoutSec = 10,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

TEST testExactlyOnePacketPayload(void) {
    /* MAX_PACKET_SIZE=1200, header=15 → max payload per pkt = 1185 */
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 1185,
        .timeoutSec = 2, .sessionTimeoutSec = 10,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

TEST testExactlyWindowSizePackets(void) {
    /* WINDOW_SIZE=10 full packets */
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 1185 * 10,
        .timeoutSec = 2, .sessionTimeoutSec = 20,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

TEST testWindowSizePlusOne(void) {
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 1185 * 11,
        .timeoutSec = 2, .sessionTimeoutSec = 20,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

TEST testMediumTextPayload(void) {
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 64 * 1024,
        .timeoutSec = 2, .sessionTimeoutSec = 30,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

TEST testLargeBinaryPayload(void) {
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 10 * 1024 * 1024,
        .binaryPayload = true,
        .timeoutSec = 3, .sessionTimeoutSec = 60,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

TEST testBinaryNullBytes(void) {
    /* binaryPayload with seed 0 produces lots of zero bytes */
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 4096,
        .binaryPayload = true,
        .timeoutSec = 2, .sessionTimeoutSec = 15,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

/* ------------------------------------------------------------------ */
/* I/O mode combinations                                               */
/* ------------------------------------------------------------------ */

TEST testStdinToFile(void) {
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 8192,
        .timeoutSec = 2, .sessionTimeoutSec = 20,
        .inputSource = inputFromStdin, .outputSink = outputToFile
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

TEST testFileToStdout(void) {
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 8192,
        .timeoutSec = 2, .sessionTimeoutSec = 20,
        .inputSource = inputFromFile, .outputSink = outputToStdout
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

TEST testStdinToStdout(void) {
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 4096,
        .timeoutSec = 2, .sessionTimeoutSec = 20,
        .inputSource = inputFromStdin, .outputSink = outputToStdout
    };
    ASSERT(runTransferCase(&tc));
    PASS();
}

SUITE(test_e2e_data) {
    RUN_TEST(testEmptyInput);
    RUN_TEST(testSingleByte);
    RUN_TEST(testSmallTextPayload);
    RUN_TEST(testExactlyOnePacketPayload);
    RUN_TEST(testExactlyWindowSizePackets);
    RUN_TEST(testWindowSizePlusOne);
    RUN_TEST(testMediumTextPayload);
    RUN_TEST(testLargeBinaryPayload);
    RUN_TEST(testBinaryNullBytes);
    RUN_TEST(testStdinToFile);
    RUN_TEST(testFileToStdout);
    RUN_TEST(testStdinToStdout);
}
