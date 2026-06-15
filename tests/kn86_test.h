/*
 * kn86_test.h — Minimal test framework for KN-86 (no external deps)
 *
 * Usage:
 *   KN86_TEST(test_name) { ... KN86_ASSERT(expr); ... }
 *   int main(void) { KN86_RUN_TEST(test_name); return KN86_REPORT(); }
 */

#ifndef KN86_TEST_H
#define KN86_TEST_H

#include <stdio.h>
#include <string.h>

static int _kn86_tests_run = 0;
static int _kn86_tests_passed = 0;
static int _kn86_tests_failed = 0;
static const char *_kn86_current_test = NULL;
static int _kn86_current_failed = 0;

#define KN86_TEST(name) static void test_##name(void)

#define KN86_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            _kn86_current_failed = 1; \
        } \
    } while (0)

#define KN86_ASSERT_EQ(a, b) \
    do { \
        long _a = (long)(a); \
        long _b = (long)(b); \
        if (_a != _b) { \
            fprintf(stderr, "  FAIL: %s:%d: %s == %s (got %ld, expected %ld)\n", \
                    __FILE__, __LINE__, #a, #b, _a, _b); \
            _kn86_current_failed = 1; \
        } \
    } while (0)

#define KN86_ASSERT_STR_EQ(a, b) \
    do { \
        const char *_a = (a); \
        const char *_b = (b); \
        if (strcmp(_a, _b) != 0) { \
            fprintf(stderr, "  FAIL: %s:%d: \"%s\" != \"%s\"\n", \
                    __FILE__, __LINE__, _a, _b); \
            _kn86_current_failed = 1; \
        } \
    } while (0)

#define KN86_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "  FAIL: %s:%d: %s is NULL\n", \
                    __FILE__, __LINE__, #ptr); \
            _kn86_current_failed = 1; \
        } \
    } while (0)

#define KN86_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "  FAIL: %s:%d: %s is not NULL\n", \
                    __FILE__, __LINE__, #ptr); \
            _kn86_current_failed = 1; \
        } \
    } while (0)

#define KN86_RUN_TEST(name) \
    do { \
        _kn86_current_test = #name; \
        _kn86_current_failed = 0; \
        _kn86_tests_run++; \
        test_##name(); \
        if (_kn86_current_failed) { \
            _kn86_tests_failed++; \
            fprintf(stderr, "FAILED: %s\n", #name); \
        } else { \
            _kn86_tests_passed++; \
            fprintf(stdout, "  ok: %s\n", #name); \
        } \
    } while (0)

#define KN86_REPORT() \
    ( \
        fprintf(stdout, "\n%d tests: %d passed, %d failed\n", \
                _kn86_tests_run, _kn86_tests_passed, _kn86_tests_failed), \
        (_kn86_tests_failed > 0) ? 1 : 0 \
    )

#endif /* KN86_TEST_H */
