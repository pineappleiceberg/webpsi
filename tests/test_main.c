#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "psi_gc.h"
#include "psi_hash_blake3.h"

#define HASH_BYTES PSI_BLAKE3_DIGEST_LEN

// hash an array of strings into a flat buffer: count * HASH_BYTES bytes using BLAKE3 keyed hashing
static void hash_strings_to_flat(
    const char **strings,
    size_t       count,
    uint8_t     *flat_out
) {
    //for tests we use the default key (pass NULL)
    psi_blake3_hash_strings_to_flat(strings, count, flat_out, NULL);
}

static void compute_reference_mask(
    const uint8_t *flat_a,
    const uint8_t *flat_b,
    size_t         count,
    size_t         elem_bytes,
    uint8_t       *out_mask
) {
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *ai = flat_a + i * elem_bytes;
        uint8_t found = 0;

        for (size_t j = 0; j < count; ++j) {
            const uint8_t *bj = flat_b + j * elem_bytes;
            if (memcmp(ai, bj, elem_bytes) == 0) {
                found = 1;
                break;
            }
        }

        out_mask[i] = found;
    }
}

static int check_mask(const uint8_t *mask, const uint8_t *expected, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (mask[i] != expected[i]) {
            return 0;
        }
    }
    return 1;
}

static int run_random_like_test(psi_gc_ctx *ctx) {
    if (!ctx) {
        fprintf(stderr, "FAIL: ctx is NULL in random-like test\n");
        return 1;
    }

    const size_t count = 8;
    const size_t elem_bits = HASH_BYTES * 8u;
    const size_t elem_bytes = (elem_bits + 7u) / 8u;

    const char *A[] = {
        "item0", "item1", "item2", "item3",
        "item4", "item5", "item6", "item7"
    };
    const char *B[] = {
        "item3", "item1", "item9", "foo",
        "item7", "bar", "baz", "item0"
    };

    uint8_t *flat_a = (uint8_t *)malloc(count * HASH_BYTES);
    uint8_t *flat_b = (uint8_t *)malloc(count * HASH_BYTES);
    uint8_t *mask_lib = (uint8_t *)malloc(count);
    uint8_t *mask_ref = (uint8_t *)malloc(count);

    int failed = 0;

    if (!flat_a || !flat_b || !mask_lib || !mask_ref) {
        fprintf(stderr, "FAIL: malloc in random-like test\n");
        failed = 1;
    } else {
        hash_strings_to_flat(A, count, flat_a);
        hash_strings_to_flat(B, count, flat_b);

        if (psi_gc_compute(ctx, flat_a, flat_b, count, mask_lib) != 0) {
            fprintf(stderr, "FAIL: psi_gc_compute in random-like test\n");
            failed = 1;
        } else {
            compute_reference_mask(flat_a, flat_b, count, elem_bytes, mask_ref);
            if (!check_mask(mask_lib, mask_ref, count)) {
                fprintf(stderr, "FAIL: mask mismatch in random-like test\n");
                failed = 1;
            } else {
                printf("PASS: random-like test\n");
            }
        }
    }

    free(flat_a);
    free(flat_b);
    free(mask_lib);
    free(mask_ref);

    return failed ? 1 : 0;
}

static int run_basic_tests(void) {
    const size_t max_elems = 8;
    const size_t elem_bits = HASH_BYTES * 8u;

    psi_gc_ctx *ctx = psi_gc_create(max_elems, elem_bits);
    if (!ctx) {
        fprintf(stderr, "FAIL: psi_gc_create returned NULL\n");
        return 1;
    }

    if (psi_gc_prepare_circuit(ctx) != 0) {
        fprintf(stderr, "FAIL: psi_gc_prepare_circuit failed\n");
        psi_gc_destroy(ctx);
        return 1;
    }

    int failed = 0;

    // test case 1, A has one element in common with B
    {
        const char *A[] = { "alice", "bob", "carol" };
        const char *B[] = { "bob", "dave", "eve" };
        const size_t count = 3;
        uint8_t expected[] = { 0, 1, 0 };

        uint8_t *flat_a = (uint8_t *)malloc(count * HASH_BYTES);
        uint8_t *flat_b = (uint8_t *)malloc(count * HASH_BYTES);
        uint8_t *mask   = (uint8_t *)malloc(count);

        if (!flat_a || !flat_b || !mask) {
            fprintf(stderr, "FAIL: malloc in test 1\n");
            failed = 1;
        } else {
            hash_strings_to_flat(A, count, flat_a);
            hash_strings_to_flat(B, count, flat_b);

            if (psi_gc_compute(ctx, flat_a, flat_b, count, mask) != 0) {
                fprintf(stderr, "FAIL: psi_gc_compute in test 1\n");
                failed = 1;
            } else if (!check_mask(mask, expected, count)) {
                fprintf(stderr, "FAIL: mask mismatch in test 1\n");
                failed = 1;
            } else {
                printf("PASS: test 1\n");
            }
        }

        free(flat_a);
        free(flat_b);
        free(mask);
    }

    // test case 2, no intersection
    {
        const char *A[] = { "x", "y" };
        const char *B[] = { "u", "v" };
        const size_t count = 2;
        uint8_t expected[] = { 0, 0 };

        uint8_t *flat_a = (uint8_t *)malloc(count * HASH_BYTES);
        uint8_t *flat_b = (uint8_t *)malloc(count * HASH_BYTES);
        uint8_t *mask   = (uint8_t *)malloc(count);

        if (!flat_a || !flat_b || !mask) {
            fprintf(stderr, "FAIL: malloc in test 2\n");
            failed = 1;
        } else {
            hash_strings_to_flat(A, count, flat_a);
            hash_strings_to_flat(B, count, flat_b);

            if (psi_gc_compute(ctx, flat_a, flat_b, count, mask) != 0) {
                fprintf(stderr, "FAIL: psi_gc_compute in test 2\n");
                failed = 1;
            } else if (!check_mask(mask, expected, count)) {
                fprintf(stderr, "FAIL: mask mismatch in test 2\n");
                failed = 1;
            } else {
                printf("PASS: test 2\n");
            }
        }

        free(flat_a);
        free(flat_b);
        free(mask);
    }

    // test case 3, identical sets 
    {
        const char *A[] = { "same1", "same2" };
        const char *B[] = { "same1", "same2" };
        const size_t count = 2;
        uint8_t expected[] = { 1, 1 };

        uint8_t *flat_a = (uint8_t *)malloc(count * HASH_BYTES);
        uint8_t *flat_b = (uint8_t *)malloc(count * HASH_BYTES);
        uint8_t *mask   = (uint8_t *)malloc(count);

        if (!flat_a || !flat_b || !mask) {
            fprintf(stderr, "FAIL: malloc in test 3\n");
            failed = 1;
        } else {
            hash_strings_to_flat(A, count, flat_a);
            hash_strings_to_flat(B, count, flat_b);

            if (psi_gc_compute(ctx, flat_a, flat_b, count, mask) != 0) {
                fprintf(stderr, "FAIL: psi_gc_compute in test 3\n");
                failed = 1;
            } else if (!check_mask(mask, expected, count)) {
                fprintf(stderr, "FAIL: mask mismatch in test 3\n");
                failed = 1;
            } else {
                printf("PASS: test 3\n");
            }
        }

        free(flat_a);
        free(flat_b);
        free(mask);
    }

    if (!failed) {
        if (run_random_like_test(ctx) != 0) {
            failed = 1;
        }
    }

    psi_gc_destroy(ctx);

    if (failed) {
        fprintf(stderr, "Some tests FAILED\n");
        return 1;
    }

    printf("All tests PASSED\n");
    return 0;
}

int main(void) {
    return run_basic_tests();
}
