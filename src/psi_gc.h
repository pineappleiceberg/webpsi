#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct psi_gc_ctx psi_gc_ctx;

psi_gc_ctx *psi_gc_create(size_t max_elems, size_t elem_bits);

void psi_gc_destroy(psi_gc_ctx *ctx);

int psi_gc_prepare_circuit(psi_gc_ctx *ctx);

int psi_gc_compute(
    psi_gc_ctx    *ctx,
    const uint8_t *inputs_a,
    const uint8_t *inputs_b,
    size_t         count,
    uint8_t       *out_mask
);

int psi_hash_only_compute(
    psi_gc_ctx    *ctx,
    const uint8_t *inputs_a,
    const uint8_t *inputs_b,
    size_t         count,
    uint8_t       *out_mask
);

int gc_proto_psi_simulate(
    const uint8_t *inputs_a_flat,
    const uint8_t *inputs_b_flat,
    size_t         count,
    size_t         elem_bits,
    uint8_t       *out_mask_direct,
    uint8_t       *out_mask_proto
);

#ifdef __cplusplus
}
#endif
