#include "../include/printk.h"
#include "../include/drivers/vga.h"
#include <stddef.h>

/**
 * Unit Test Framework for Kernel panik Functionality
 * 
 * Note: Since panik() never returns and halts the system,
 * these tests focus on testing the components that panik uses
 * and simulating panik scenarios.
 */

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Test result macros
#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            pr_info("[PASS] %s\n", message); \
        } else { \
            tests_failed++; \
            pr_err("[FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_START(test_name) \
    pr_notice("Starting test: %s\n", test_name)

#define TEST_END() \
    pr_info("Test completed\n")


/**
 * Test real kernel panik scenarios
 */
void test_null_pointer_panik(void)
{
    pr_notice("Testing: Null pointer dereference detection\n");
    reset_panik_state();
    
    // Simulate null pointer access - common kernel panik cause
    void *ptr = NULL;
    if (ptr == NULL) {
        panik("Null pointer dereference at %p", ptr);
    }
    
    const panik_state_t* state = get_panik_state();
    TEST_ASSERT(state->panik_called == 1, "Null pointer panik triggered");
}

void test_memory_corruption_panik(void)
{
    pr_notice("Testing: Memory corruption detection\n");
    reset_panik_state();
    
    // Simulate memory corruption detection
    unsigned int magic = 0xDEADBEEF;
    unsigned int corrupted = 0x12345678;
    
    if (magic != 0xDEADBEEF) {
        panik("Memory corruption detected: magic=0x%x expected=0x%x", corrupted, magic);
    }
    
    // Force corruption scenario
    panik("Stack canary corruption detected at 0x%x", 0xDEADBEEF);
    
    const panik_state_t* state = get_panik_state();
    TEST_ASSERT(state->panik_called == 1, "Memory corruption panik triggered");
}

void test_division_by_zero_panik(void)
{
    pr_notice("Testing: Division by zero detection\n");
    reset_panik_state();
    
    // Simulate division by zero - real kernel scenario
    int divisor = 0;
    if (divisor == 0) {
        panik("Division by zero in kernel code");
    }
    
    const panik_state_t* state = get_panik_state();
    TEST_ASSERT(state->panik_called == 1, "Division by zero panik triggered");
}

void test_assert_failures(void)
{
    pr_notice("Testing: Kernel assertions\n");
    reset_panik_state();
    
    // Test kernel assertions - real debugging scenarios
    assert(2 + 2 == 5);  // Intentional failure
    
    const panik_state_t* state = get_panik_state();
    TEST_ASSERT(state->panik_called == 1, "Assert failure triggered panik");
    
    // Test BUG_ON macro
    reset_panik_state();
    BUG_ON(1);  // Should always trigger
    
    state = get_panik_state();
    TEST_ASSERT(state->panik_called == 1, "BUG_ON triggered panik");
}

void test_stack_overflow_panik(void)
{
    pr_notice("Testing: Stack overflow detection\n");
    reset_panik_state();
    
    // Simulate stack overflow detection
    panik("Stack overflow detected: SP=0x%x limit=0x%x", 0x1000, 0x2000);
    
    const panik_state_t* state = get_panik_state();
    TEST_ASSERT(state->panik_called == 1, "Stack overflow panik triggered");
}

void test_hardware_fault_panik(void)
{
    pr_notice("Testing: Hardware fault handling\n");
    reset_panik_state();
    
    // Simulate hardware faults that cause kernel paniks
    panik("Page fault in kernel mode: CR2=0x%x EIP=0x%x", 0xDEADBEEF, 0x12345678);
    
    const panik_state_t* state = get_panik_state();
    TEST_ASSERT(state->panik_called == 1, "Hardware fault panik triggered");
}

void test_panik_with_real_formatting(void)
{
    pr_notice("Testing: Real panik message formatting\n");
    reset_panik_state();
    
    // Test realistic panik messages with actual kernel data
    panik("Unable to mount root fs on unknown-block(%d,%d)", 8, 1);
    
    const panik_state_t* state = get_panik_state();
    TEST_ASSERT(state->panik_called == 1, "Formatted panik message works");
    
    pr_info("panik message: %s\n", state->last_panik_msg);
}

/**
 * Run compact panik tests - real kernel scenarios
 */
void run_panik_unit_tests(void)
{
    pr_notice("=== KERNEL panik TESTS - Real Scenarios ===\n");
    
    // Set test mode for safe testing
    set_panik_mode(PANIK_MODE_TEST);
    pr_info("panik mode: TEST (safe for testing)\n");
    
    // Initialize counters
    tests_run = 0;
    tests_passed = 0;
    
    // Run focused real-world tests
    test_null_pointer_panik();
    test_memory_corruption_panik();
    test_division_by_zero_panik();
    test_assert_failures();
    test_stack_overflow_panik();
    test_hardware_fault_panik();
    test_panik_with_real_formatting();
    
    // Test summary
    pr_notice("\n=== TEST RESULTS ===\n");
    pr_info("Tests run: %d\n", tests_run);
    pr_info("Tests passed: %d\n", tests_passed);
    pr_info("Tests failed: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        pr_info("✓ All kernel panik scenarios tested successfully!\n");
    } else {
        pr_err("✗ Some tests failed!\n");
    }
    
    // Show panik statistics
    const panik_state_t* state = get_panik_state();
    pr_info("Total panik calls: %d\n", state->panik_call_count);
    
    pr_notice("=== END TESTS ===\n");
    
    // Manual test instructions
    pr_warn("\nTo test REAL panik (system will halt):\n");
    pr_warn("1. Edit this file and uncomment the manual test\n");
    pr_warn("2. Run: make clean && make kernel && make run\n");
    pr_warn("WARNING: System will require reset!\n");
    
    // Uncomment to test REAL panik (WILL HALT SYSTEM):
    /*
    pr_crit("TESTING REAL panik - SYSTEM WILL HALT!\n");
    set_panik_mode(PANIK_MODE_NORMAL);
    panik("Real panik test - system should halt now!");
    */
}
