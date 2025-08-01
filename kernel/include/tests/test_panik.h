#ifndef KERNEL_TEST_panik_H
#define KERNEL_TEST_panik_H

/**
 * Compact Kernel panik Testing - Real Scenario Focus
 * Tests actual panik conditions that occur in real kernels
 */

// Main test runner function
void run_panik_unit_tests(void);

// Real-world panik scenario test functions
void test_null_pointer_panik(void);
void test_memory_corruption_panik(void);
void test_division_by_zero_panik(void);
void test_assert_failures(void);
void test_stack_overflow_panik(void);
void test_hardware_fault_panik(void);
void test_panik_with_real_formatting(void);

#endif /* KERNEL_TEST_panik_H */

