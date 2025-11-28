#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "gc_core.h"

static int test_garbled_and_2(void) {
    gc_circuit *plain = gc_circuit_and_2();
    if (!plain) {
        fprintf(stderr, "garbled_and_2: plain circuit NULL\n");
        return 1;
    }

    gc_garbled_circuit *gc = NULL;
    if (gc_garble(plain, &gc) != 0 || !gc) {
        fprintf(stderr, "garbled_and_2: gc_garble failed\n");
        gc_circuit_free(plain);
        return 1;
    }

    uint8_t in_bits[2];
    uint8_t out_bits_clear[1];
    gc_label in_labels[2];
    gc_label out_labels[1];
    uint8_t out_bits_garbled[1];

    struct { uint8_t a, b; } cases[] = {
        {0,0},
        {0,1},
        {1,0},
        {1,1},
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        in_bits[0] = cases[i].a;
        in_bits[1] = cases[i].b;

        if (gc_eval_clear(plain, in_bits, out_bits_clear) != 0) {
            fprintf(stderr, "garbled_and_2: gc_eval_clear failed case %zu\n", i);
            gc_garbled_free(gc);
            gc_circuit_free(plain);
            return 1;
        }

        // we gotta build input labels
        // use wire_labels0/1 according to input bits
        for (uint16_t j = 0; j < gc->n_inputs; ++j) {
            uint16_t w = gc->input_wires[j];
            uint8_t bit = in_bits[j] & 1u;
            in_labels[j] = (bit == 0)
                ? gc->wire_labels0[w]
                : gc->wire_labels1[w];
        }

        if (gc_eval_garbled(gc, in_labels, out_labels) != 0) {
            fprintf(stderr, "garbled_and_2: gc_eval_garbled failed case %zu\n", i);
            gc_garbled_free(gc);
            gc_circuit_free(plain);
            return 1;
        }

        if (gc_decode_outputs(gc, out_labels, out_bits_garbled) != 0) {
            fprintf(stderr, "garbled_and_2: gc_decode_outputs failed case %zu\n", i);
            gc_garbled_free(gc);
            gc_circuit_free(plain);
            return 1;
        }

        if (out_bits_garbled[0] != out_bits_clear[0]) {
            fprintf(stderr, "garbled_and_2: mismatch case %zu (a=%u,b=%u): gc=%u, clear=%u\n", i, cases[i].a, cases[i].b, out_bits_garbled[0], out_bits_clear[0]);
            gc_garbled_free(gc);
            gc_circuit_free(plain);
            return 1;
        }
    }

    gc_garbled_free(gc);
    gc_circuit_free(plain);
    return 0;
}

static int test_garbled_xor_2(void) {
    gc_circuit *plain = gc_circuit_xor_2();
    if (!plain) {
        fprintf(stderr, "garbled_xor_2: plain circuit NULL\n");
        return 1;
    }

    gc_garbled_circuit *gc = NULL;
    if (gc_garble(plain, &gc) != 0 || !gc) {
        fprintf(stderr, "garbled_xor_2: gc_garble failed\n");
        gc_circuit_free(plain);
        return 1;
    }

    uint8_t in_bits[2];
    uint8_t out_bits_clear[1];
    gc_label in_labels[2];
    gc_label out_labels[1];
    uint8_t out_bits_garbled[1];

    struct { uint8_t a, b; } cases[] = {
        {0,0},
        {0,1},
        {1,0},
        {1,1},
    };

    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        in_bits[0] = cases[i].a;
        in_bits[1] = cases[i].b;

        if (gc_eval_clear(plain, in_bits, out_bits_clear) != 0) {
            fprintf(stderr, "garbled_xor_2: gc_eval_clear failed case %zu\n", i);
            gc_garbled_free(gc);
            gc_circuit_free(plain);
            return 1;
        }

        for (uint16_t j = 0; j < gc->n_inputs; ++j) {
            uint16_t w = gc->input_wires[j];
            uint8_t bit = in_bits[j] & 1u;
            in_labels[j] = (bit == 0)
                ? gc->wire_labels0[w]
                : gc->wire_labels1[w];
        }

        if (gc_eval_garbled(gc, in_labels, out_labels) != 0) {
            fprintf(stderr, "garbled_xor_2: gc_eval_garbled failed case %zu\n", i);
            gc_garbled_free(gc);
            gc_circuit_free(plain);
            return 1;
        }

        if (gc_decode_outputs(gc, out_labels, out_bits_garbled) != 0) {
            fprintf(stderr, "garbled_xor_2: gc_decode_outputs failed case %zu\n", i);
            gc_garbled_free(gc);
            gc_circuit_free(plain);
            return 1;
        }

        if (out_bits_garbled[0] != out_bits_clear[0]) {
            fprintf(stderr, "garbled_xor_2: mismatch case %zu (a=%u,b=%u): gc=%u, clear=%u\n", i, cases[i].a, cases[i].b, out_bits_garbled[0], out_bits_clear[0]);
            gc_garbled_free(gc);
            gc_circuit_free(plain);
            return 1;
        }
    }

    gc_garbled_free(gc);
    gc_circuit_free(plain);
    return 0;
}

static int test_garbled_eq_2bit(void) {
    gc_circuit *plain = gc_circuit_eq_2bit();
    if (!plain) {
        fprintf(stderr, "garbled_eq_2bit: plain circuit NULL\n");
        return 1;
    }

    gc_garbled_circuit *gc = NULL;
    if (gc_garble(plain, &gc) != 0 || !gc) {
        fprintf(stderr, "garbled_eq_2bit: gc_garble failed\n");
        gc_circuit_free(plain);
        return 1;
    }

    uint8_t in_bits[4];
    uint8_t out_bits_clear[1];
    gc_label in_labels[4];
    gc_label out_labels[1];
    uint8_t out_bits_garbled[1];

    // exhaustive test over all 2-bit pairs (a,b)
    for (uint8_t a = 0; a < 4; ++a) {
        for (uint8_t b = 0; b < 4; ++b) {
            in_bits[0] = (a >> 0) & 1u;
            in_bits[1] = (a >> 1) & 1u;
            in_bits[2] = (b >> 0) & 1u;
            in_bits[3] = (b >> 1) & 1u;

            if (gc_eval_clear(plain, in_bits, out_bits_clear) != 0) {
                fprintf(stderr, "garbled_eq_2bit: gc_eval_clear failed a=%u,b=%u\n", a, b);
                gc_garbled_free(gc);
                gc_circuit_free(plain);
                return 1;
            }

            for (uint16_t j = 0; j < gc->n_inputs; ++j) {
                uint16_t w = gc->input_wires[j];
                uint8_t bit = in_bits[j] & 1u;
                in_labels[j] = (bit == 0)
                    ? gc->wire_labels0[w]
                    : gc->wire_labels1[w];
            }

            if (gc_eval_garbled(gc, in_labels, out_labels) != 0) {
                fprintf(stderr, "garbled_eq_2bit: gc_eval_garbled failed a=%u,b=%u\n", a, b);
                gc_garbled_free(gc);
                gc_circuit_free(plain);
                return 1;
            }

            if (gc_decode_outputs(gc, out_labels, out_bits_garbled) != 0) {
                fprintf(stderr, "garbled_eq_2bit: gc_decode_outputs failed a=%u,b=%u\n", a, b);
                gc_garbled_free(gc);
                gc_circuit_free(plain);
                return 1;
            }

            if (out_bits_garbled[0] != out_bits_clear[0]) {
                fprintf(stderr, "garbled_eq_2bit: mismatch a=%u,b=%u: gc=%u, clear=%u\n", a, b, out_bits_garbled[0], out_bits_clear[0]);
                gc_garbled_free(gc);
                gc_circuit_free(plain);
                return 1;
            }
        }
    }

    gc_garbled_free(gc);
    gc_circuit_free(plain);
    return 0;
}

static int test_stats_eq_2bit(void) {
    gc_circuit *plain = gc_circuit_eq_2bit();
    if (!plain) {
        fprintf(stderr, "stats_eq_2bit: plain circuit NULL\n");
        return 1;
    }

    gc_garbled_circuit *gc = NULL;
    if (gc_garble(plain, &gc) != 0 || !gc) {
        fprintf(stderr, "stats_eq_2bit: gc_garble failed\n");
        gc_circuit_free(plain);
        return 1;
    }

    gc_stats st;
    gc_compute_stats(gc, &st);

    if (st.num_gates != 5 ||
        st.num_xor_gates != 2 ||
        st.num_not_gates != 2 ||
        st.num_and_gates != 1 ||
        st.num_ciphertexts != 12 ||
        st.ciphertext_bytes != 12 * GC_LABEL_BYTES) {
        fprintf(stderr, "stats_eq_2bit: unexpected stats: " 
            "gates=%zu (AND=%zu,XOR=%zu,NOT=%zu) ciphertexts=%zu bytes=%zu\n", 
            st.num_gates, st.num_and_gates, st.num_xor_gates, st.num_not_gates, st.num_ciphertexts, st.ciphertext_bytes);
        gc_garbled_free(gc);
        gc_circuit_free(plain);
        return 1;
    }

    gc_garbled_free(gc);
    gc_circuit_free(plain);
    return 0;
}

int main(void) {
    int failed = 0;
    if (test_garbled_and_2() != 0) failed = 1;
    if (test_garbled_xor_2() != 0) failed = 1;
    if (test_garbled_eq_2bit() != 0) failed = 1;
    if (test_stats_eq_2bit() != 0) failed = 1;

    if (failed) {
        fprintf(stderr, "gc_garbled tests FAILED\n");
        return 1;
    }
    printf("gc_garbled tests PASSED\n");
    return 0;
}
