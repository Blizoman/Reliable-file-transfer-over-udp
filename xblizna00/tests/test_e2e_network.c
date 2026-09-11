// E2E network impairment tests
// Tests: packet loss, duplication, reordering via tc netem.
// All tests require root + tc netem – skipped otherwise.

#include "greatest.h"
#include "test_e2e_common.h"

/* ------------------------------------------------------------------ */
/* Packet loss                                                          */
/* ------------------------------------------------------------------ */

TEST testLoss5Percent(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    ASSERT(applyTcNetem("loss 5%"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 100 * 1024,
        .timeoutSec = 3, .sessionTimeoutSec = 60,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}

TEST testLoss15Percent(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    ASSERT(applyTcNetem("loss 15%"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 100 * 1024,
        .timeoutSec = 3, .sessionTimeoutSec = 60,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}

TEST testLoss30Percent(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    ASSERT(applyTcNetem("loss 30%"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 16 * 1024,
        .timeoutSec = 3, .sessionTimeoutSec = 60,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}

TEST testLossBig(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    ASSERT(applyTcNetem("loss 15%"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 200000,
        .timeoutSec = 3, .sessionTimeoutSec = 40,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}



/* ------------------------------------------------------------------ */
/* Packet duplication                                                   */
/* ------------------------------------------------------------------ */

TEST testDuplicate10Percent(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    ASSERT(applyTcNetem("duplicate 10%"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 64 * 1024,
        .timeoutSec = 5, .sessionTimeoutSec = 60,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}

TEST testDuplicate30Percent(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    ASSERT(applyTcNetem("duplicate 30%"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 32 * 1024,
        .timeoutSec = 5, .sessionTimeoutSec = 60,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}

TEST testDuplicate20Big(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    ASSERT(applyTcNetem("duplicate 20%"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 140000,
        .timeoutSec = 3, .sessionTimeoutSec = 40,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}

/* ------------------------------------------------------------------ */
/* Packet reordering                                                    */
/* ------------------------------------------------------------------ */

TEST testReorder25Percent(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    /* 25% packets reordered with 10ms delay */
    ASSERT(applyTcNetem("delay 10ms reorder 25% 50%"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 64 * 1024,
        .timeoutSec = 5, .sessionTimeoutSec = 60,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}

TEST testReorder40Big(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    ASSERT(applyTcNetem("delay 10ms reorder 40% 50%"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 150000,
        .timeoutSec = 3, .sessionTimeoutSec = 40,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}

/* ------------------------------------------------------------------ */
/* Jitter                                                               */
/* ------------------------------------------------------------------ */

TEST testJitter20ms(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    ASSERT(applyTcNetem("delay 5ms 20ms distribution normal"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 64 * 1024,
        .timeoutSec = 5, .sessionTimeoutSec = 60,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}

TEST testJitter30ms(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    ASSERT(applyTcNetem("delay 80ms 30ms distribution normal"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 150000,
        .timeoutSec = 4, .sessionTimeoutSec = 50,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}

/* ------------------------------------------------------------------ */
/* Combined: loss + duplication + reorder (hell mode)                  */
/* ------------------------------------------------------------------ */

TEST testHellMode(void) {
    if (!isTcNetemUsable()) SKIPm("Requires root + tc netem");
    ASSERT(applyTcNetem("delay 10ms 5ms loss 15% duplicate 10% reorder 15% 50%"));
    TransferCase tc = {
        .host = "127.0.0.1", .payloadSize = 16 * 1024,
        .timeoutSec = 15, .sessionTimeoutSec = 120,
        .inputSource = inputFromFile, .outputSink = outputToFile
    };
    bool ok = runTransferCase(&tc);
    clearTcNetem();
    ASSERT(ok);
    PASS();
}

SUITE(test_e2e_network) {
    RUN_TEST(testLoss5Percent);
    RUN_TEST(testLoss15Percent);
    RUN_TEST(testLoss30Percent);
    RUN_TEST(testDuplicate10Percent);
    RUN_TEST(testDuplicate30Percent);
    RUN_TEST(testReorder25Percent);
    RUN_TEST(testJitter20ms);
    RUN_TEST(testHellMode);
    //
    RUN_TEST(testJitter30ms);
    RUN_TEST(testReorder40Big);
    RUN_TEST(testDuplicate20Big);
    RUN_TEST(testLossBig);
}
