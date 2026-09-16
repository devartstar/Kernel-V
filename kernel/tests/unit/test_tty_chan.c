#include "test_tty_chan.h"
#include "drivers/tty_chan.h"

/* Test 1: init + basic stage/commit/read round-trip. */
uint8_t tty_chan_basic_test(void) {
    tty_chan_t ch;
    uint8_t storage[8];
    uint8_t out[8];

    if (tty_chan_init(&ch, storage, sizeof(storage)) != TTY_CHAN_OK) {
        KLOG_ERROR("TTYCHAN_TEST", "init failed.\n");
        return 0;
    }

    /* Nothing committed yet: readable must be 0. */
    if (tty_chan_readable(&ch) != 0) {
        KLOG_ERROR("TTYCHAN_TEST", "fresh channel not empty.\n");
        return 0;
    }

    /* Stage two bytes — still invisible to reader until commit. */
    tty_chan_stage(&ch, 'A');
    tty_chan_stage(&ch, 'B');
    if (tty_chan_readable(&ch) != 0) {
        KLOG_ERROR("TTYCHAN_TEST", "staged bytes visible before commit.\n");
        return 0;
    }
    if (tty_chan_staged(&ch) != 2) {
        KLOG_ERROR("TTYCHAN_TEST", "staged count wrong.\n");
        return 0;
    }

    /* Commit publishes both atomically. */
    if (tty_chan_commit(&ch) != 2) {
        KLOG_ERROR("TTYCHAN_TEST", "commit returned wrong count.\n");
        return 0;
    }
    if (tty_chan_readable(&ch) != 2) {
        KLOG_ERROR("TTYCHAN_TEST", "committed bytes not readable.\n");
        return 0;
    }

    /* Read them back. */
    if (tty_chan_read(&ch, out, sizeof(out)) != 2) {
        KLOG_ERROR("TTYCHAN_TEST", "read count wrong.\n");
        return 0;
    }
    if (out[0] != 'A' || out[1] != 'B') {
        KLOG_ERROR("TTYCHAN_TEST", "read data mismatch.\n");
        return 0;
    }

    KLOG_INFO("TTYCHAN_TEST", "basic test passed.\n");
    return 1;
}

/* Test 2: rollback un-stages the newest byte; can't cross the commit line. */
uint8_t tty_chan_rollback_test(void) {
    tty_chan_t ch;
    uint8_t storage[8];
    uint8_t out[8];

    if (tty_chan_init(&ch, storage, sizeof(storage)) != TTY_CHAN_OK) {
        KLOG_ERROR("TTYCHAN_TEST", "rollback: init failed.\n");
        return 0;
    }

    tty_chan_stage(&ch, 'X');
    tty_chan_stage(&ch, 'Y');
    tty_chan_stage(&ch, 'Z');
    if (tty_chan_staged(&ch) != 3) {
        KLOG_ERROR("TTYCHAN_TEST", "rollback: staged != 3.\n");
        return 0;
    }

    /* Undo the last staged byte ('Z'); staged drops to 2. */
    if (tty_chan_rollback(&ch) != TTY_CHAN_OK) {
        KLOG_ERROR("TTYCHAN_TEST", "rollback: op failed.\n");
        return 0;
    }
    if (tty_chan_staged(&ch) != 2) {
        KLOG_ERROR("TTYCHAN_TEST", "rollback: staged != 2 after rollback.\n");
        return 0;
    }

    /* Commit + read: only the first two staged bytes survive. */
    if (tty_chan_commit(&ch) != 2) {
        KLOG_ERROR("TTYCHAN_TEST", "rollback: commit count != 2.\n");
        return 0;
    }
    if (tty_chan_read(&ch, out, sizeof(out)) != 2) {
        KLOG_ERROR("TTYCHAN_TEST", "rollback: read count != 2.\n");
        return 0;
    }
    if (out[0] != 'X' || out[1] != 'Y') {
        KLOG_ERROR("TTYCHAN_TEST", "rollback: data mismatch (want XY).\n");
        return 0;
    }

    /* Everything is committed+read now: nothing staged to roll back. */
    if (tty_chan_rollback(&ch) != TTY_CHAN_ERR_EMPTY) {
        KLOG_ERROR("TTYCHAN_TEST",
                   "rollback: expected ERR_EMPTY past commit line.\n");
        return 0;
    }

    KLOG_INFO("TTYCHAN_TEST", "rollback test passed.\n");
    return 1;
}

/* Test 3: fullness via free-running counters; 5th stage into cap=4 fails. */
uint8_t tty_chan_full_test(void) {
    tty_chan_t ch;
    uint8_t storage[4];
    uint8_t out[4];

    if (tty_chan_init(&ch, storage, sizeof(storage)) != TTY_CHAN_OK) {
        KLOG_ERROR("TTYCHAN_TEST", "full: init failed.\n");
        return 0;
    }

    if (tty_chan_free(&ch) != 4) {
        KLOG_ERROR("TTYCHAN_TEST", "full: fresh free != 4.\n");
        return 0;
    }

    /* Fill the ring completely. */
    for (uint8_t i = 0; i < 4; i++) {
        if (tty_chan_stage(&ch, (uint8_t)('0' + i)) != TTY_CHAN_OK) {
            KLOG_ERROR("TTYCHAN_TEST", "full: stage %u failed early.\n", i);
            return 0;
        }
    }
    if (tty_chan_free(&ch) != 0) {
        KLOG_ERROR("TTYCHAN_TEST", "full: free != 0 when full.\n");
        return 0;
    }

    /* One more must be rejected. */
    if (tty_chan_stage(&ch, '!') != TTY_CHAN_ERR_FULL) {
        KLOG_ERROR("TTYCHAN_TEST", "full: 5th stage not rejected.\n");
        return 0;
    }

    /* Drain everything; free returns to capacity. */
    if (tty_chan_commit(&ch) != 4) {
        KLOG_ERROR("TTYCHAN_TEST", "full: commit != 4.\n");
        return 0;
    }
    if (tty_chan_read(&ch, out, sizeof(out)) != 4) {
        KLOG_ERROR("TTYCHAN_TEST", "full: read != 4.\n");
        return 0;
    }
    if (tty_chan_free(&ch) != 4) {
        KLOG_ERROR("TTYCHAN_TEST", "full: free != 4 after drain.\n");
        return 0;
    }

    KLOG_INFO("TTYCHAN_TEST", "full test passed.\n");
    return 1;
}

/* Test 4: physical wrap-around. cap=4, advance cursors to 3, then write 3 more
 * that wrap to indices 3,0,1. Catches broken %capacity indexing. */
uint8_t tty_chan_wraparound_test(void) {
    tty_chan_t ch;
    uint8_t storage[4];
    uint8_t out[4];

    if (tty_chan_init(&ch, storage, sizeof(storage)) != TTY_CHAN_OK) {
        KLOG_ERROR("TTYCHAN_TEST", "wrap: init failed.\n");
        return 0;
    }

    /* Push read/commit/write all to 3 by round-tripping 3 bytes. */
    tty_chan_stage(&ch, 'a');
    tty_chan_stage(&ch, 'b');
    tty_chan_stage(&ch, 'c');
    tty_chan_commit(&ch);
    if (tty_chan_read(&ch, out, 3) != 3) {
        KLOG_ERROR("TTYCHAN_TEST", "wrap: warmup read != 3.\n");
        return 0;
    }

    /* Now stage 3 more: physical slots 3, 0, 1 (wrap). */
    tty_chan_stage(&ch, 'P');
    tty_chan_stage(&ch, 'Q');
    tty_chan_stage(&ch, 'R');
    if (tty_chan_commit(&ch) != 3) {
        KLOG_ERROR("TTYCHAN_TEST", "wrap: commit != 3.\n");
        return 0;
    }

    if (tty_chan_read(&ch, out, sizeof(out)) != 3) {
        KLOG_ERROR("TTYCHAN_TEST", "wrap: read != 3.\n");
        return 0;
    }
    if (out[0] != 'P' || out[1] != 'Q' || out[2] != 'R') {
        KLOG_ERROR("TTYCHAN_TEST",
                   "wrap: order wrong across wrap (want PQR).\n");
        return 0;
    }

    KLOG_INFO("TTYCHAN_TEST", "wraparound test passed.\n");
    return 1;
}

/* Test 5: partial reads clamp to len, and `read` advances correctly. */
uint8_t tty_chan_partial_read_test(void) {
    tty_chan_t ch;
    uint8_t storage[8];
    uint8_t out[8];

    if (tty_chan_init(&ch, storage, sizeof(storage)) != TTY_CHAN_OK) {
        KLOG_ERROR("TTYCHAN_TEST", "partial: init failed.\n");
        return 0;
    }

    for (uint8_t i = 0; i < 5; i++) {
        tty_chan_stage(&ch, (uint8_t)('1' + i)); /* '1'..'5' */
    }
    if (tty_chan_commit(&ch) != 5) {
        KLOG_ERROR("TTYCHAN_TEST", "partial: commit != 5.\n");
        return 0;
    }

    /* Read only 2 of 5. */
    if (tty_chan_read(&ch, out, 2) != 2) {
        KLOG_ERROR("TTYCHAN_TEST", "partial: first read != 2.\n");
        return 0;
    }
    if (out[0] != '1' || out[1] != '2') {
        KLOG_ERROR("TTYCHAN_TEST", "partial: first chunk mismatch.\n");
        return 0;
    }
    if (tty_chan_readable(&ch) != 3) {
        KLOG_ERROR("TTYCHAN_TEST", "partial: readable != 3 after first.\n");
        return 0;
    }

    /* Ask for more than remains: clamps to 3. */
    if (tty_chan_read(&ch, out, sizeof(out)) != 3) {
        KLOG_ERROR("TTYCHAN_TEST", "partial: second read != 3.\n");
        return 0;
    }
    if (out[0] != '3' || out[1] != '4' || out[2] != '5') {
        KLOG_ERROR("TTYCHAN_TEST", "partial: second chunk mismatch.\n");
        return 0;
    }
    if (tty_chan_readable(&ch) != 0) {
        KLOG_ERROR("TTYCHAN_TEST", "partial: readable != 0 at end.\n");
        return 0;
    }

    KLOG_INFO("TTYCHAN_TEST", "partial read test passed.\n");
    return 1;
}
