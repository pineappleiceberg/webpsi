#include "gc_core.h"

#include <stdlib.h>
#include <string.h>

#include "psi_hash_blake3.h"
#include "blake3.h"

static void secure_memzero(void *p, size_t len) {
    if (!p || len == 0) {
        return;
    }
    volatile uint8_t *v = (volatile uint8_t *)p;
    while (len--) {
        *v++ = 0;
    }
}

int gc_eval_clear(
    const gc_circuit *c,
    const uint8_t    *inputs,
    uint8_t          *outputs
) {
    if (!c || !inputs || !outputs) {
        return -1;
    }
    if (c->n_inputs == 0 || c->n_outputs == 0 || c->n_wires == 0) {
        return -2;
    }

    uint8_t *wire_vals = (uint8_t *)calloc(c->n_wires, sizeof(uint8_t));
    if (!wire_vals) {
        return -3;
    }

    for (uint16_t i = 0; i < c->n_inputs; ++i) {
        uint16_t w = c->input_wires[i];
        if (w >= c->n_wires) {
            free(wire_vals);
            return -4;
        }
        wire_vals[w] = inputs[i] ? 1u : 0u;
    }

    for (size_t gi = 0; gi < c->n_gates; ++gi) {
        const gc_gate *g = &c->gates[gi];
        if (g->out >= c->n_wires) {
            free(wire_vals);
            return -5;
        }

        uint8_t out = 0;
        switch (g->type) {
        case GC_GATE_AND:
            if (g->in0 >= c->n_wires || g->in1 >= c->n_wires) {
                free(wire_vals);
                return -6;
            }
            out = (wire_vals[g->in0] & wire_vals[g->in1]) ? 1u : 0u;
            break;
        case GC_GATE_XOR:
            if (g->in0 >= c->n_wires || g->in1 >= c->n_wires) {
                free(wire_vals);
                return -7;
            }
            out = (wire_vals[g->in0] ^ wire_vals[g->in1]) ? 1u : 0u;
            break;
        case GC_GATE_NOT:
            if (g->in0 >= c->n_wires) {
                free(wire_vals);
                return -8;
            }
            out = wire_vals[g->in0] ? 0u : 1u;
            break;
        default:
            free(wire_vals);
            return -9;
        }

        wire_vals[g->out] = out;
    }

    for (uint16_t i = 0; i < c->n_outputs; ++i) {
        uint16_t w = c->output_wires[i];
        if (w >= c->n_wires) {
            free(wire_vals);
            return -10;
        }
        outputs[i] = wire_vals[w] ? 1u : 0u;
    }

    free(wire_vals);
    return 0;
}

static gc_circuit *gc_circuit_alloc(
    uint16_t n_wires,
    uint16_t n_inputs,
    uint16_t n_outputs,
    size_t   n_gates
) {
    gc_circuit *c = (gc_circuit *)calloc(1, sizeof(gc_circuit));
    if (!c) {
        return NULL;
    }

    c->n_wires   = n_wires;
    c->n_inputs  = n_inputs;
    c->n_outputs = n_outputs;
    c->n_gates   = n_gates;

    c->input_wires  = (uint16_t *)calloc(n_inputs, sizeof(uint16_t));
    c->output_wires = (uint16_t *)calloc(n_outputs, sizeof(uint16_t));
    c->gates        = (gc_gate *)calloc(n_gates, sizeof(gc_gate));

    if (!c->input_wires || !c->output_wires || !c->gates) {
        gc_circuit_free(c);
        return NULL;
    }
    return c;
}

gc_circuit *gc_circuit_and_2() {
    gc_circuit *c = gc_circuit_alloc(3, 2, 1, 1);
    if (!c) return NULL;

    c->input_wires[0] = 0;
    c->input_wires[1] = 1;

    c->output_wires[0] = 2;

    c->gates[0].in0  = 0;
    c->gates[0].in1  = 1;
    c->gates[0].out  = 2;
    c->gates[0].type = GC_GATE_AND;

    return c;
}

gc_circuit *gc_circuit_xor_2() {
    gc_circuit *c = gc_circuit_alloc(3, 2, 1, 1);
    if (!c) return NULL;

    c->input_wires[0] = 0;
    c->input_wires[1] = 1;
    c->output_wires[0] = 2;

    c->gates[0].in0  = 0;
    c->gates[0].in1  = 1;
    c->gates[0].out  = 2;
    c->gates[0].type = GC_GATE_XOR;

    return c;
}

gc_circuit *gc_circuit_eq_2bit() {
    gc_circuit *c = gc_circuit_alloc(9, 4, 1, 5);
    if (!c) return NULL;

    c->input_wires[0] = 0;
    c->input_wires[1] = 1;
    c->input_wires[2] = 2;
    c->input_wires[3] = 3;

    c->output_wires[0] = 8;

    c->gates[0].in0  = 0;
    c->gates[0].in1  = 2;
    c->gates[0].out  = 4;
    c->gates[0].type = GC_GATE_XOR;

    c->gates[1].in0  = 1;
    c->gates[1].in1  = 3;
    c->gates[1].out  = 5;
    c->gates[1].type = GC_GATE_XOR;

    c->gates[2].in0  = 4;
    c->gates[2].in1  = 0;
    c->gates[2].out  = 6;
    c->gates[2].type = GC_GATE_NOT;

    c->gates[3].in0  = 5;
    c->gates[3].in1  = 0;
    c->gates[3].out  = 7;
    c->gates[3].type = GC_GATE_NOT;

    c->gates[4].in0  = 6;
    c->gates[4].in1  = 7;
    c->gates[4].out  = 8;
    c->gates[4].type = GC_GATE_AND;

    return c;
}

void gc_circuit_free(gc_circuit *c) {
    if (!c) return;
    free(c->input_wires);
    free(c->output_wires);
    free(c->gates);
    free(c);
}

static const uint8_t GC_PRF_KEY[PSI_BLAKE3_KEY_LEN] = {
    0x47, 0x43, 0x2d, 0x50, 0x52, 0x46, 0x2d, 0x4b,
    0x65, 0x79, 0x2d, 0x31, 0x32, 0x33, 0x34, 0x56,
    0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x11, 0x22,
    0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa
};

static gc_label GC_DELTA;
static int GC_DELTA_INITIALIZED = 0;

static void gc_init_delta(void) {
    if (GC_DELTA_INITIALIZED) {
        return;
    }

    uint8_t input[4] = { 0x44, 0x45, 0x4c, 0x54 };

    blake3_hasher hasher;
    blake3_hasher_init_keyed(&hasher, GC_PRF_KEY);
    blake3_hasher_update(&hasher, input, sizeof(input));
    blake3_hasher_finalize(&hasher, GC_DELTA.b, GC_LABEL_BYTES);

    GC_DELTA.b[0] |= 0x01;
    GC_DELTA_INITIALIZED = 1;
}

static void gc_derive_label0(uint16_t wire, gc_label *l0) {
    uint8_t input[4];
    input[0] = (uint8_t)(wire & 0xff);
    input[1] = (uint8_t)((wire >> 8) & 0xff);
    input[2] = 0;
    input[3] = 0xA5;

    blake3_hasher hasher;
    blake3_hasher_init_keyed(&hasher, GC_PRF_KEY);
    blake3_hasher_update(&hasher, input, sizeof(input));
    blake3_hasher_finalize(&hasher, l0->b, GC_LABEL_BYTES);

    l0->b[0] &= 0xFE;
}

static inline uint8_t gc_permute_bit(const gc_label *lab) {
    return lab->b[0] & 1u;
}

static void gc_gate_prf(
    const gc_label *ka,
    const gc_label *kb,
    uint16_t        gate_index,
    uint8_t         row,
    uint8_t        *out_keystream
) {
    uint8_t buf[GC_LABEL_BYTES * 2 + 4];
    size_t off = 0;

    memcpy(buf + off, ka->b, GC_LABEL_BYTES); off += GC_LABEL_BYTES;
    memcpy(buf + off, kb->b, GC_LABEL_BYTES); off += GC_LABEL_BYTES;
    buf[off++] = (uint8_t)(gate_index & 0xff);
    buf[off++] = (uint8_t)((gate_index >> 8) & 0xff);
    buf[off++] = row;
    buf[off++] = 0x3C;

    blake3_hasher hasher;
    blake3_hasher_init_keyed(&hasher, GC_PRF_KEY);
    blake3_hasher_update(&hasher, buf, off);
    blake3_hasher_finalize(&hasher, out_keystream, GC_LABEL_BYTES);
}

static void gc_label_xor(const gc_label *a, const gc_label *b, gc_label *out) {
    for (size_t i = 0; i < GC_LABEL_BYTES; ++i) {
        out->b[i] = (uint8_t)(a->b[i] ^ b->b[i]);
    }
}

static int gc_label_equal_ct(const gc_label *a, const gc_label *b) {
    uint8_t diff = 0;
    for (size_t i = 0; i < GC_LABEL_BYTES; ++i) {
        diff |= (uint8_t)(a->b[i] ^ b->b[i]);
    }
    return diff == 0;
}

int gc_garble(
    const gc_circuit    *plain,
    gc_garbled_circuit **out_gc
) {
    if (!plain || !out_gc) {
        return -1;
    }

    gc_garbled_circuit *gc = (gc_garbled_circuit *)calloc(1, sizeof(gc_garbled_circuit));
    if (!gc) {
        return -2;
    }

    gc->n_wires   = plain->n_wires;
    gc->n_inputs  = plain->n_inputs;
    gc->n_outputs = plain->n_outputs;
    gc->n_gates   = plain->n_gates;

    gc->input_wires  = (uint16_t *)calloc(gc->n_inputs, sizeof(uint16_t));
    gc->output_wires = (uint16_t *)calloc(gc->n_outputs, sizeof(uint16_t));
    gc->gates        = (gc_garbled_gate *)calloc(gc->n_gates, sizeof(gc_garbled_gate));
    gc->wire_labels0 = (gc_label *)calloc(gc->n_wires, sizeof(gc_label));
    gc->wire_labels1 = (gc_label *)calloc(gc->n_wires, sizeof(gc_label));

    if (!gc->input_wires || !gc->output_wires || !gc->gates ||
        !gc->wire_labels0 || !gc->wire_labels1) {
        gc_garbled_free(gc);
        return -3;
    }

    memcpy(gc->input_wires,  plain->input_wires,  gc->n_inputs  * sizeof(uint16_t));
    memcpy(gc->output_wires, plain->output_wires, gc->n_outputs * sizeof(uint16_t));

    gc_init_delta();
    for (uint16_t w = 0; w < gc->n_wires; ++w) {
        gc_label l0;
        gc_derive_label0(w, &l0);
        gc->wire_labels0[w] = l0;
        gc_label_xor(&l0, &GC_DELTA, &gc->wire_labels1[w]);
    }

    for (size_t gi = 0; gi < gc->n_gates; ++gi) {
        const gc_gate *pg = &plain->gates[gi];
        if (pg->type != GC_GATE_XOR) {
            continue;
        }

        const gc_label *L0_in0 = &gc->wire_labels0[pg->in0];
        const gc_label *L0_in1 = &gc->wire_labels0[pg->in1];

        gc_label L0_out;
        gc_label_xor(L0_in0, L0_in1, &L0_out);

        gc->wire_labels0[pg->out] = L0_out;
        gc_label_xor(&L0_out, &GC_DELTA, &gc->wire_labels1[pg->out]);
    }

    for (size_t gi = 0; gi < gc->n_gates; ++gi) {
        const gc_gate *pg = &plain->gates[gi];
        gc_garbled_gate *gg = &gc->gates[gi];

        gg->in0  = pg->in0;
        gg->in1  = pg->in1;
        gg->out  = pg->out;
        gg->type = pg->type;

        if (pg->type == GC_GATE_XOR) {
            continue;
        }

        for (uint8_t a = 0; a < 2; ++a) {
            for (uint8_t b = 0; b < 2; ++b) {
                const gc_label *la = &gc->wire_labels0[pg->in0];
                const gc_label *la1 = &gc->wire_labels1[pg->in0];
                const gc_label *lb = &gc->wire_labels0[pg->in1];
                const gc_label *lb1 = &gc->wire_labels1[pg->in1];

                const gc_label *Ka = (a == 0) ? la : la1;
                const gc_label *Kb = (b == 0) ? lb : lb1;

                uint8_t bit_out = 0;
                switch (pg->type) {
                case GC_GATE_AND:
                    bit_out = (uint8_t)((a & b) & 1u);
                    break;
                case GC_GATE_XOR:
                    bit_out = (uint8_t)((a ^ b) & 1u);
                    break;
                case GC_GATE_NOT:
                    bit_out = (uint8_t)((a ? 0u : 1u) & 1u);
                    break;
                default:
                    return -4;
                }

                const gc_label *Lout0 = &gc->wire_labels0[pg->out];
                const gc_label *Lout1 = &gc->wire_labels1[pg->out];
                const gc_label *Kout  = (bit_out == 0) ? Lout0 : Lout1;

                uint8_t color_a = gc_permute_bit(Ka);
                uint8_t color_b = gc_permute_bit(Kb);
                uint8_t row = (uint8_t)((color_a << 1) | color_b);

                uint8_t keystream[GC_LABEL_BYTES];
                gc_gate_prf(Ka, Kb, (uint16_t)gi, row, keystream);

                gc_label *ct = &gg->table[row];
                for (size_t i = 0; i < GC_LABEL_BYTES; ++i) {
                    ct->b[i] = (uint8_t)(Kout->b[i] ^ keystream[i]);
                }
            }
        }
    }

    *out_gc = gc;
    return 0;
}

int gc_eval_garbled(
    const gc_garbled_circuit *gc,
    const gc_label           *input_labels,
    gc_label                 *output_labels
) {
    if (!gc || !input_labels || !output_labels) {
        return -1;
    }

    gc_label *wire_vals = (gc_label *)calloc(gc->n_wires, sizeof(gc_label));
    if (!wire_vals) {
        return -2;
    }
    int rc = 0;

    for (uint16_t i = 0; i < gc->n_inputs; ++i) {
        uint16_t w = gc->input_wires[i];
        if (w >= gc->n_wires) {
            rc = -3;
            goto cleanup;
        }
        wire_vals[w] = input_labels[i];
    }

    for (size_t gi = 0; gi < gc->n_gates; ++gi) {
        const gc_garbled_gate *gg = &gc->gates[gi];

        if (gg->out >= gc->n_wires) {
            rc = -4;
            goto cleanup;
        }

        if (gg->type == GC_GATE_XOR) {
            gc_label Kout;
            gc_label_xor(&wire_vals[gg->in0], &wire_vals[gg->in1], &Kout);
            wire_vals[gg->out] = Kout;
            continue;
        }

        const gc_label *Ka = &wire_vals[gg->in0];
        const gc_label *Kb = &wire_vals[gg->in1];

        uint8_t color_a = gc_permute_bit(Ka);
        uint8_t color_b = gc_permute_bit(Kb);
        uint8_t row = (uint8_t)((color_a << 1) | color_b);

        const gc_label *ct = &gg->table[row];

        uint8_t keystream[GC_LABEL_BYTES];
        gc_gate_prf(Ka, Kb, (uint16_t)gi, row, keystream);

        gc_label Kout;
        for (size_t i = 0; i < GC_LABEL_BYTES; ++i) {
            Kout.b[i] = (uint8_t)(ct->b[i] ^ keystream[i]);
        }

        wire_vals[gg->out] = Kout;
    }

    for (uint16_t i = 0; i < gc->n_outputs; ++i) {
        uint16_t w = gc->output_wires[i];
        if (w >= gc->n_wires) {
            rc = -5;
            goto cleanup;
        }
        output_labels[i] = wire_vals[w];
    }

    rc = 0;

cleanup:
    if (wire_vals) {
        secure_memzero(wire_vals, gc->n_wires * sizeof(gc_label));
        free(wire_vals);
    }
    return rc;
}

int gc_decode_outputs(
    const gc_garbled_circuit *gc,
    const gc_label           *output_labels,
    uint8_t                  *outputs_bits
) {
    if (!gc || !output_labels || !outputs_bits) {
        return -1;
    }

    for (uint16_t i = 0; i < gc->n_outputs; ++i) {
        uint16_t w = gc->output_wires[i];
        const gc_label *L0 = &gc->wire_labels0[w];
        const gc_label *L1 = &gc->wire_labels1[w];
        const gc_label *Lo = &output_labels[i];

        if (gc_label_equal_ct(Lo, L0)) {
            outputs_bits[i] = 0;
        } else if (gc_label_equal_ct(Lo, L1)) {
            outputs_bits[i] = 1;
        } else {
            return -2;
        }
    }
    return 0;
}

void gc_garbled_free(gc_garbled_circuit *gc) {
    if (!gc) return;
    if (gc->wire_labels0) {
        secure_memzero(gc->wire_labels0, gc->n_wires * sizeof(gc_label));
    }
    if (gc->wire_labels1) {
        secure_memzero(gc->wire_labels1, gc->n_wires * sizeof(gc_label));
    }
    if (gc->gates) {
        secure_memzero(gc->gates, gc->n_gates * sizeof(gc_garbled_gate));
    }
    free(gc->input_wires);
    free(gc->output_wires);
    free(gc->gates);
    free(gc->wire_labels0);
    free(gc->wire_labels1);
    free(gc);
}

void gc_compute_stats(const gc_garbled_circuit *gc, gc_stats *stats) {
    if (!gc || !stats) {
        return;
    }

    memset(stats, 0, sizeof(*stats));
    stats->num_gates = gc->n_gates;

    for (size_t gi = 0; gi < gc->n_gates; ++gi) {
        const gc_garbled_gate *gg = &gc->gates[gi];
        switch (gg->type) {
        case GC_GATE_AND:
            stats->num_and_gates++;
            stats->num_ciphertexts += 4;
            break;
        case GC_GATE_XOR:
            stats->num_xor_gates++;
            break;
        case GC_GATE_NOT:
            stats->num_not_gates++;
            stats->num_ciphertexts += 4;
            break;
        default:
            break;
        }
    }

    stats->ciphertext_bytes = stats->num_ciphertexts * GC_LABEL_BYTES;
}
