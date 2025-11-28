#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "gc_core.h"

static int test_and_2(void) {
    gc_circuit *c = gc_circuit_and_2();
    if (!c) {
        fprintf(stderr, "test_and_2: gc_circuit_and_2 returned NULL\n");
        return 1;
    }

    uint8_t in[2];
    uint8_t out[1];

    // truth table for AND
    struct {uint8_t a, b, expected; } cases[] = {
        {0,0,0},
        {0,1,0},
        {1,0,0},
        {1,1,1},
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        in[0] = cases[i].a;
        in[1] = cases[i].b;
        out[0] = 0xff;

        int rc = gc_eval_clear(c, in, out);
        if (rc != 0) {
            fprintf(stderr, "test_and_2: gc_eval_clear rc=%d on case %zu\n", rc, i);
            gc_circuit_free(c);
            return 1;
        }
        if (out[0] != cases[i].expected) {
            fprintf(stderr, "test_and_2: mismatch case %zu (a=%u,b=%u): got=%u, expected=%u\n", i, cases[i].a, cases[i].b, out[0], cases[i].expected);
            gc_circuit_free(c);
            return 1;
        }
    }

    gc_circuit_free(c);
    return 0;
}

static int test_xor_2(void) {
    gc_circuit *c = gc_circuit_xor_2();
    if (!c) {
        fprintf(stderr, "test_xor_2: gc_circuit_xor_2 returned NULL\n");
        return 1;
    }

    uint8_t in[2];
    uint8_t out[1];

    struct { uint8_t a, b, expected; } cases[] = {
        {0,0,0},
        {0,1,1},
        {1,0,1},
        {1,1,0},
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        in[0] = cases[i].a;
        in[1] = cases[i].b;
        out[0] = 0xff;

        int rc = gc_eval_clear(c, in, out);
        if (rc != 0) {
            fprintf(stderr, "test_xor_2: gc_eval_clear rc=%d on case %zu\n", rc, i);
            gc_circuit_free(c);
            return 1;
        }
        if (out[0] != cases[i].expected) {
            fprintf(stderr, "test_xor_2: mismatch case %zu (a=%u,b=%u): got=%u, expected=%u\n",
                    i, cases[i].a, cases[i].b, out[0], cases[i].expected);
            gc_circuit_free(c);
            return 1;
        }
    }

    gc_circuit_free(c);
    return 0;
}

static int test_eq_2bit(void) {
    gc_circuit *c = gc_circuit_eq_2bit();
    if (!c) {
        fprintf(stderr, "test_eq_2bit: gc_circuit_eq_2bit returned NULL\n");
        return 1;
    }

    uint8_t in[4];
    uint8_t out[1];

    /* Exhaustive test over all 4-bit pairs (2-bit a, 2-bit b). */
    for (uint8_t a = 0; a < 4; ++a) {
        for (uint8_t b = 0; b < 4; ++b) {
            in[0] = (a >> 0) & 1u;
            in[1] = (a >> 1) & 1u;
            in[2] = (b >> 0) & 1u;
            in[3] = (b >> 1) & 1u;

            out[0] = 0xff;

            int rc = gc_eval_clear(c, in, out);
            if (rc != 0) {
                fprintf(stderr, "test_eq_2bit: gc_eval_clear rc=%d on a=%u,b=%u\n", rc, a, b);
                gc_circuit_free(c);
                return 1;
            }

            uint8_t expected = (a == b) ? 1u : 0u;
            if (out[0] != expected) {
                fprintf(stderr, "test_eq_2bit: mismatch a=%u,b=%u: got=%u, expected=%u\n",
                        a, b, out[0], expected);
                gc_circuit_free(c);
                return 1;
            }
        }
    }

    gc_circuit_free(c);
    return 0;
}

int main(void) {
    int failed = 0;
    if (test_and_2() != 0) failed = 1;
    if (test_xor_2() != 0) failed = 1;
    if (test_eq_2bit() != 0) failed = 1;

    if (failed) {
        fprintf(stderr, "gc_core tests FAILED\n");
        return 1;
    }
    printf("gc_core tests PASSED\n");
    return 0;
}
