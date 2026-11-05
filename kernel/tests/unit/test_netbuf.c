#include "lib/printk.h"
#include "net/netbuf.h"
#include <stdint.h>

static uint32_t tests_run, tests_passed, tests_failed;

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        tests_run++;                                                           \
        if (cond) {                                                            \
            tests_passed++;                                                    \
            KLOG_INFO("NETBUF_TEST", "[TEST %u] [PASS] %s\n", tests_run, msg); \
        } else {                                                               \
            tests_failed++;                                                    \
            KLOG_ERROR("NETBUF_TEST", "[TEST %u] [FAIL] %s\n", tests_run,      \
                       msg);                                                   \
        }                                                                      \
    } while (0)

void run_netbuf_tests(void) {
    tests_run = tests_passed = tests_failed = 0;
    netbuf_stats_t base_stats, before_stats, after_stats;
    KLOG_VERBOSE("NETBUF_TEST", "Starting Network Buffer tests....\n");

    /* get the base stats for refernce */
    netbuf_get_stats(&base_stats);

    /* memory would already be initialized during kernel initialization prior
     * test runner */

    /* TEST 1. NETBUF allocation test */
    KLOG_VERBOSE("NETBUF_TEST", "Test buffer allocation from pool.\n");
    uint32_t headroom_sz = 128;
    netbuf_t *buf = netbuf_alloc(headroom_sz);
    TEST_ASSERT(buf != NULL, "Checking for non null allocated buffer.\n");

    /* Cheching buffer geometry.
     * buffer state after allocation with headrom
     * <head>---128----<data><tail>---NETBUF_SLOT_SIZE-128---<end> */
    TEST_ASSERT(netbuf_headroom(buf) == headroom_sz,
                "Check for correct headroom in allocated buffer.\n");
    TEST_ASSERT(netbuf_tailroom(buf) == (NETBUF_SLOT_SIZE - 128),
                "Check for correct tailroom in allocated buffer.\n");
    TEST_ASSERT(netbuf_len(buf) == 0, "Check no data added to the buffer.\n");

    /* TEST 2. NETBUF data addition */
    uint8_t *data_ref = buf->data, *tail_ref = buf->tail;
    TEST_ASSERT(netbuf_put(buf, 100) == tail_ref,
                "Put datacheck to the buffer.\n");
    TEST_ASSERT(buf->tail == tail_ref + 100,
                "tail to advance by 100 after put.\n");
    TEST_ASSERT(netbuf_push(buf, 8) == data_ref - 8,
                "Push 8 bits data check for buffer.\n");
    TEST_ASSERT(netbuf_push(buf, 14) == data_ref - 22,
                "Successive push of 14 bits data check on buffer.\n");
    TEST_ASSERT(netbuf_len(buf) == 122,
                "Check for data legnth after push and put.\n");

    /* update with the buffer head and tail reference */
    data_ref = buf->data;
    tail_ref = buf->tail;

    /* TEST 3. NETBUF data removal */
    TEST_ASSERT(netbuf_pull(buf, 14) == data_ref,
                "Check for pulling data from buffer.\n");
    TEST_ASSERT(netbuf_len(buf) == 108,
                "Check data length shrunk post pull.\n");
    TEST_ASSERT(netbuf_pull(buf, netbuf_len(buf) + 1) == NULL,
                "Check pull more data than present.\n");
    TEST_ASSERT(buf->data == data_ref + 14,
                "Check correct data ref after pull.\n");

    /* update with the buffer head and tail reference */
    data_ref = buf->data;
    tail_ref = buf->tail;

    /* TEST 4. NETBUF data trimming */
    TEST_ASSERT(netbuf_trim(buf, 8) == true, "Check data trimmed.\n");
    TEST_ASSERT(buf->tail == data_ref + 8, "Check tail ref after trim.\n");
    TEST_ASSERT(netbuf_trim(buf, netbuf_len(buf) + 1) == false,
                "Check data trim more than present.\n");

    /* update with the buffer head and tail reference */
    data_ref = buf->data;
    tail_ref = buf->tail;

    /* TEST 5. NETBUF physical address */
    TEST_ASSERT(netbuf_phys(buf, buf->head) == buf->head_phys,
                "CHeck head points to correct physical address.\n");
    TEST_ASSERT(netbuf_phys(buf, buf->data) ==
                    buf->head_phys + netbuf_headroom(buf),
                "Check data points to the correct physical address.\n");
    TEST_ASSERT(buf->head_phys % NETBUF_SLOT_SIZE == 0,
                "Check head physical address is slot size (2048) aligned.\n");

    /* TEST 6. NETBUF reference counting */
    netbuf_get_stats(&before_stats);

    /* hold will increase and release will decrease ref count in buffer,
     * ref count not yet 0 since buffer alloc took one reference */
    netbuf_hold(buf);
    netbuf_release(buf);
    netbuf_get_stats(&after_stats);
    TEST_ASSERT(after_stats.free == before_stats.free,
                "hold + release - still allocated.\n");

    /* TEST 7. Releasing the allocated buffer */
    netbuf_get_stats(&before_stats);
    /* release will free the buffer ar ref count will reach 0 */
    netbuf_release(buf);
    netbuf_get_stats(&after_stats);
    TEST_ASSERT(after_stats.free == before_stats.free + 1,
                "Check buffer freed on release.\n");

    /* TEST 8. Exhaust the buffer pool allocating slots */
    netbuf_get_stats(&before_stats);
    netbuf_t *buf_list[NETBUF_COUNT];
    uint32_t n = 0;

    /* some slots might already be allocated, allocate remaining slots till full
     * */
    while (n < NETBUF_COUNT &&
           (buf_list[n] = netbuf_alloc(headroom_sz)) != NULL) {
        n++;
    }

    /* we allocated all free slots before allocating */
    TEST_ASSERT(n == before_stats.free, "drained every free slot");

    /* allocating another slot will fail */
    TEST_ASSERT(netbuf_alloc(headroom_sz) == NULL,
                "alloc on empty pool -> NULL");

    netbuf_get_stats(&after_stats);

    /* slot allocation failure count should be 1 */
    TEST_ASSERT(after_stats.alloc_fail - before_stats.alloc_fail == 1,
                "Verify allocation failure count in stats.\n");

    /* since all slots allocated, min free should be 0 */
    TEST_ASSERT(after_stats.min_free == 0,
                "Check no free slot post complete allocation.\n");

    /* TEST 9. Free the slot allocations */

    /* keeping the before stats same as before filling allocations */

    for (uint8_t i = 0; i < n; i++) {
        netbuf_release(buf_list[i]);
    }

    netbuf_get_stats(&after_stats);
    TEST_ASSERT(after_stats.free == before_stats.free,
                "Check all buffer freed success.\n");

    /* TEST 10. Network buffer enqueue and dequeue */
    netbuf_queue_t buf_queue;
    netbuf_queue_init(&buf_queue);

    /* allocate 2 buffer and push them to the queue */
    netbuf_t *buf1 = netbuf_alloc(headroom_sz);
    netbuf_enqueue(&buf_queue, buf1);
    TEST_ASSERT(buf_queue.head == buf1 && buf_queue.tail == buf1 &&
                    buf_queue.len == 1,
                "Buffer queue, first entry added.\n");

    netbuf_t *buf2 = netbuf_alloc(headroom_sz);
    data_ref = netbuf_push(buf2, 10);
    netbuf_enqueue(&buf_queue, buf2);
    TEST_ASSERT(buf_queue.tail == buf2 && buf_queue.head == buf1 &&
                    buf_queue.len == 2,
                "Buffer queue, both entries added successfully.\n");

    /* dequeue the buffer and verify FIFO */
    TEST_ASSERT(netbuf_dequeue(&buf_queue) == buf1,
                "First buffer dequeue should follow FIFO order.\n");
    TEST_ASSERT(buf_queue.head == buf2 && buf_queue.tail == buf2 &&
                    buf_queue.len == 1,
                "Network Buffer dequeue one entry.\n");

    TEST_ASSERT(netbuf_dequeue(&buf_queue) == buf2,
                "Second buffer dequeue follow FIFO order.\n");
    TEST_ASSERT(buf_queue.head == NULL && buf_queue.tail == NULL &&
                    buf_queue.len == 0,
                "Network Buffer dequeued all entries.\n");

    netbuf_release(buf1);
    netbuf_release(buf2);

    KLOG_INFO("NETBUF_TEST", "netbuf tests: run=%u passed=%u failed=%u\n",
              tests_run, tests_passed, tests_failed);

    KLOG_VERBOSE("NETBUF_TEST", "Completed Network Buffer tests....\n");
}
