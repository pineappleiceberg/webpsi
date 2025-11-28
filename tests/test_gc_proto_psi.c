#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "psi_gc.h"
#include "psi_hash_blake3.h"

static int test_proto_small(void) {
    const char *set_a[] = { "alice", "bob", "carol" };
    const char *set_b[] = { "bob", "dave", "carol" };
    const size_t count = 3;
    const size_t elem_bits = PSI_BLAKE3_DIGEST_LEN * 8u;

    uint8_t flat_a[count * PSI_BLAKE3_DIGEST_LEN];
    uint8_t flat_b[count * PSI_BLAKE3_DIGEST_LEN];

    psi_blake3_hash_strings_to_flat(set_a, count, flat_a, NULL);
    psi_blake3_hash_strings_to_flat(set_b, count, flat_b, NULL);

    uint8_t mask_direct[count];
    uint8_t mask_proto[count];

    int rc = gc_proto_psi_simulate(flat_a, flat_b, count, elem_bits,
                                   mask_direct, mask_proto);
    if (rc != 0) {
        fprintf(stderr, "test_proto_small: gc_proto_psi_simulate rc=%d\n", rc);
        return 1;
    }

    const uint8_t expected[3] = {0, 1, 1};
    for (size_t i = 0; i < count; ++i) {
        if (mask_direct[i] != expected[i] || mask_proto[i] != expected[i]) {
            fprintf(stderr,
                    "test_proto_small: mismatch idx=%zu direct=%u proto=%u expected=%u\n",
                    i, mask_direct[i], mask_proto[i], expected[i]);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    int failed = 0;
    if (test_proto_small() != 0) failed = 1;

    if (failed) {
        fprintf(stderr, "gc_proto_psi tests FAILED\n");
        return 1;
    }
    printf("gc_proto_psi tests PASSED\n");
    return 0;
}
