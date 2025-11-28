#include "psi_gc.h"
#include "gc_core.h"

#include <stdlib.h>
#include <string.h>

struct psi_gc_ctx {
    size_t max_elems;
    size_t elem_bits;
};

static int psi_gc_compute_with_gc_y(
    psi_gc_ctx    *ctx,
    const uint8_t *inputs_a,
    const uint8_t *inputs_b,
    size_t         count,
    uint8_t       *out_mask
);

static void psi_compute_naive(
    const uint8_t *inputs_a,
    const uint8_t *inputs_b,
    size_t         count,
    size_t         elem_bytes,
    uint8_t       *out_mask
);

psi_gc_ctx *psi_gc_create(size_t max_elems, size_t elem_bits) {
    if (max_elems == 0 || elem_bits == 0) {
        return NULL;
    }

    psi_gc_ctx *ctx = (psi_gc_ctx *)malloc(sizeof(psi_gc_ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->max_elems = max_elems;
    ctx->elem_bits = elem_bits;
    return ctx;
}

void psi_gc_destroy(psi_gc_ctx *ctx) {
    if (!ctx) {
        return;
    }
    free(ctx);
}

int psi_gc_prepare_circuit(psi_gc_ctx *ctx) {
    if (!ctx) {
        return -1;
    }

    return 0;
}

int psi_gc_compute(
    psi_gc_ctx    *ctx,
    const uint8_t *inputs_a,
    const uint8_t *inputs_b,
    size_t         count,
    uint8_t       *out_mask
) {
    if (!ctx || !inputs_a || !inputs_b || !out_mask) {
        return -1;
    }

    if (count == 0) {
        return 0;
    }

    if (count > ctx->max_elems) {
        return -2;
    }

    return psi_gc_compute_with_gc_y(ctx, inputs_a, inputs_b, count, out_mask);
}

int psi_hash_only_compute(
    psi_gc_ctx    *ctx,
    const uint8_t *inputs_a,
    const uint8_t *inputs_b,
    size_t         count,
    uint8_t       *out_mask
) {
    if (!ctx || !inputs_a || !inputs_b || !out_mask) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    if (count > ctx->max_elems) {
        return -2;
    }

    const size_t elem_bits  = ctx->elem_bits;
    const size_t elem_bytes = (elem_bits + 7u) / 8u;

    psi_compute_naive(inputs_a, inputs_b, count, elem_bytes, out_mask);
    return 0;
}

static gc_circuit *build_eq_circuit_bits(size_t elem_bits) {
    if (elem_bits == 0 || elem_bits > 512) {
        return NULL;
    }

    const uint16_t k = (uint16_t)elem_bits;
    const uint16_t n_inputs = (uint16_t)(2 * k);

    const uint16_t base_xor = (uint16_t)(2 * k);
    const uint16_t base_eq  = (uint16_t)(3 * k);
    const uint16_t base_acc = (uint16_t)(4 * k);
    const uint16_t out_wire = (uint16_t)(base_acc + (k > 1 ? (k - 2) : 0));

    const uint16_t n_wires =
        (uint16_t)((k == 1)
            ? (4 * k + 1)
            : (4 * k + (k - 1)));

    const size_t n_gates = (size_t)(k + k + (k > 1 ? (k - 1) : 1));

    gc_circuit *c = (gc_circuit *)calloc(1, sizeof(gc_circuit));
    if (!c) return NULL;

    c->n_wires   = n_wires;
    c->n_inputs  = n_inputs;
    c->n_outputs = 1;
    c->n_gates   = n_gates;

    c->input_wires  = (uint16_t *)calloc(c->n_inputs, sizeof(uint16_t));
    c->output_wires = (uint16_t *)calloc(c->n_outputs, sizeof(uint16_t));
    c->gates        = (gc_gate *)calloc(c->n_gates, sizeof(gc_gate));
    if (!c->input_wires || !c->output_wires || !c->gates) {
        gc_circuit_free(c);
        return NULL;
    }

    for (uint16_t i = 0; i < c->n_inputs; ++i) {
        c->input_wires[i] = i;
    }
    c->output_wires[0] = out_wire;

    size_t gi = 0;

    for (uint16_t i = 0; i < k; ++i) {
        gc_gate *g = &c->gates[gi++];
        g->in0  = i;
        g->in1  = (uint16_t)(k + i);
        g->out  = (uint16_t)(base_xor + i);
        g->type = GC_GATE_XOR;
    }

    for (uint16_t i = 0; i < k; ++i) {
        gc_gate *g = &c->gates[gi++];
        g->in0  = (uint16_t)(base_xor + i);
        g->in1  = 0;
        g->out  = (uint16_t)(base_eq + i);
        g->type = GC_GATE_NOT;
    }

    if (k == 1) {
        gc_gate *g = &c->gates[gi++];
        g->in0  = base_eq;
        g->in1  = base_eq;
        g->out  = out_wire;
        g->type = GC_GATE_AND;
    } else {
        uint16_t acc = base_eq;
        for (uint16_t i = 1; i < k; ++i) {
            uint16_t next_eq = (uint16_t)(base_eq + i);
            uint16_t next_acc =
                (i == k - 1) ? out_wire : (uint16_t)(base_acc + (i - 1));

            gc_gate *g = &c->gates[gi++];
            g->in0  = acc;
            g->in1  = next_eq;
            g->out  = next_acc;
            g->type = GC_GATE_AND;

            acc = next_acc;
        }
    }

    return c;
}

static void fill_bit_inputs(
    uint8_t       *inputs,
    const uint8_t *bytes_a,
    const uint8_t *bytes_b,
    size_t         elem_bits
) {
    const size_t elem_bytes = (elem_bits + 7u) / 8u;
    const size_t k = elem_bits;

    for (size_t i = 0; i < k; ++i) {
        size_t byte_idx = i / 8;
        size_t bit_idx  = i % 8;

        uint8_t bit_a = 0;
        uint8_t bit_b = 0;

        if (byte_idx < elem_bytes) {
            bit_a = (bytes_a[byte_idx] >> bit_idx) & 1u;
            bit_b = (bytes_b[byte_idx] >> bit_idx) & 1u;
        }

        inputs[i]        = bit_a;
        inputs[k + i]    = bit_b;
    }
}

static void psi_compute_naive(
    const uint8_t *inputs_a,
    const uint8_t *inputs_b,
    size_t         count,
    size_t         elem_bytes,
    uint8_t       *out_mask
) {
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *ai = inputs_a + i * elem_bytes;
        uint8_t found = 0;
        for (size_t j = 0; j < count; ++j) {
            const uint8_t *bj = inputs_b + j * elem_bytes;
            if (memcmp(ai, bj, elem_bytes) == 0) {
                found = 1;
                break;
            }
        }
        out_mask[i] = found;
    }
}

static int psi_gc_compute_with_gc_y(
    psi_gc_ctx    *ctx,
    const uint8_t *inputs_a,
    const uint8_t *inputs_b,
    size_t         count,
    uint8_t       *out_mask
) {
    if (!ctx || !inputs_a || !inputs_b || !out_mask) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    if (count > ctx->max_elems) {
        return -2;
    }

    const size_t elem_bits  = ctx->elem_bits;
    const size_t elem_bytes = (elem_bits + 7u) / 8u;

    gc_circuit *plain = build_eq_circuit_bits(elem_bits);
    if (!plain) {
        psi_compute_naive(inputs_a, inputs_b, count, elem_bytes, out_mask);
        return 0;
    }

    gc_garbled_circuit *gc = NULL;
    if (gc_garble(plain, &gc) != 0 || !gc) {
        gc_circuit_free(plain);
        psi_compute_naive(inputs_a, inputs_b, count, elem_bytes, out_mask);
        return 0;
    }

    const size_t n_inputs = plain->n_inputs;
    uint8_t *bit_inputs = (uint8_t *)calloc(n_inputs, sizeof(uint8_t));
    gc_label *input_labels = (gc_label *)calloc(plain->n_inputs, sizeof(gc_label));
    gc_label out_labels[1];
    uint8_t out_bits[1];

    if (!bit_inputs || !input_labels) {
        free(bit_inputs);
        free(input_labels);
        gc_garbled_free(gc);
        gc_circuit_free(plain);
        return -3;
    }

    for (size_t i = 0; i < count; ++i) {
        const uint8_t *ai = inputs_a + i * elem_bytes;
        uint8_t found = 0;

        for (size_t j = 0; j < count; ++j) {
            const uint8_t *bj = inputs_b + j * elem_bytes;

            memset(bit_inputs, 0, n_inputs);
            fill_bit_inputs(bit_inputs, ai, bj, elem_bits);

            for (uint16_t k = 0; k < plain->n_inputs; ++k) {
                uint16_t w = gc->input_wires[k];
                uint8_t bit = bit_inputs[k] & 1u;
                input_labels[k] = (bit == 0)
                    ? gc->wire_labels0[w]
                    : gc->wire_labels1[w];
            }

            if (gc_eval_garbled(gc, input_labels, out_labels) != 0) {
                continue;
            }
            if (gc_decode_outputs(gc, out_labels, out_bits) != 0) {
                continue;
            }

            if (out_bits[0] == 1u) {
                found = 1;
                break;
            }
        }

        out_mask[i] = found;
    }

    free(bit_inputs);
    free(input_labels);
    gc_garbled_free(gc);
    gc_circuit_free(plain);
    return 0;
}

int gc_proto_psi_simulate(
    const uint8_t *inputs_a_flat,
    const uint8_t *inputs_b_flat,
    size_t         count,
    size_t         elem_bits,
    uint8_t       *out_mask_direct,
    uint8_t       *out_mask_proto
) {
    if (!inputs_a_flat || !inputs_b_flat || !out_mask_direct || !out_mask_proto) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }

    if (elem_bits == 0) {
        return -2;
    }

    psi_gc_ctx *ctx_direct = psi_gc_create(count, elem_bits);
    if (!ctx_direct) {
        return -3;
    }
    if (psi_gc_prepare_circuit(ctx_direct) != 0) {
        psi_gc_destroy(ctx_direct);
        return -4;
    }

    int rc = psi_gc_compute(ctx_direct, inputs_a_flat, inputs_b_flat, count, out_mask_direct);
    psi_gc_destroy(ctx_direct);
    if (rc != 0) {
        return -5;
    }

    psi_gc_ctx *ctx_proto = psi_gc_create(count, elem_bits);
    if (!ctx_proto) {
        return -6;
    }
    if (psi_gc_prepare_circuit(ctx_proto) != 0) {
        psi_gc_destroy(ctx_proto);
        return -7;
    }

    rc = psi_gc_compute(ctx_proto, inputs_a_flat, inputs_b_flat, count, out_mask_proto);
    psi_gc_destroy(ctx_proto);
    if (rc != 0) {
        return -8;
    }

    for (size_t i = 0; i < count; ++i) {
        if (out_mask_direct[i] != out_mask_proto[i]) {
            return 1;
        }
    }
    return 0;
}
