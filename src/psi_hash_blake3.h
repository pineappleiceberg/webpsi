#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PSI_BLAKE3_DIGEST_LEN 16
#define PSI_BLAKE3_KEY_LEN    32

void psi_blake3_hash_strings_to_flat(
    const char **strings,
    size_t       count,
    uint8_t     *flat_out,
    const uint8_t key[PSI_BLAKE3_KEY_LEN]
);

void psi_blake3_hash_bytes(const uint8_t *data,
                           size_t         len,
                           uint8_t       *out);

#ifdef __cplusplus
}
#endif
