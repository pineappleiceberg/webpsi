#include "psi_hash_blake3.h"

#include <string.h>

#include "blake3.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define PSI_EMS_KEEPALIVE EMSCRIPTEN_KEEPALIVE
#else
#define PSI_EMS_KEEPALIVE
#endif

static const uint8_t PSI_BLAKE3_DEFAULT_KEY[PSI_BLAKE3_KEY_LEN] = {
    0x42, 0x6c, 0x61, 0x6b, 0x65, 0x33, 0x2d, 0x50,
    0x53, 0x49, 0x2d, 0x44, 0x65, 0x6d, 0x6f, 0x2d,
    0x4b, 0x65, 0x79, 0x2d, 0x31, 0x32, 0x33, 0x34,
    0xaa, 0xbb, 0xcc, 0xdd, 0x55, 0x66, 0x77, 0x88
};

void psi_blake3_hash_strings_to_flat(
    const char **strings,
    size_t       count,
    uint8_t     *flat_out,
    const uint8_t key[PSI_BLAKE3_KEY_LEN]
) {
    if (!strings || !flat_out) {
        return;
    }

    const uint8_t *k = key ? key : PSI_BLAKE3_DEFAULT_KEY;

    for (size_t i = 0; i < count; ++i) {
        const char *s = strings[i];
        const size_t out_off = i * PSI_BLAKE3_DIGEST_LEN;
        uint8_t *out_i = flat_out + out_off;

        blake3_hasher hasher;
        blake3_hasher_init_keyed(&hasher, k);

        if (s) {
            const size_t len = strlen(s);
            if (len > 0) {
                blake3_hasher_update(&hasher, s, len);
            }
        }

        blake3_hasher_finalize(&hasher, out_i, PSI_BLAKE3_DIGEST_LEN);
    }
}

PSI_EMS_KEEPALIVE
void psi_blake3_hash_bytes(const uint8_t *data,
                           size_t         len,
                           uint8_t       *out) {
    const uint8_t *key = PSI_BLAKE3_DEFAULT_KEY;

    blake3_hasher hasher;
    blake3_hasher_init_keyed(&hasher, key);

    if (data && len > 0) {
        blake3_hasher_update(&hasher, data, len);
    }

    blake3_hasher_finalize(&hasher, out, PSI_BLAKE3_DIGEST_LEN);
}
