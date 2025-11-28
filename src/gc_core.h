#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GC_LABEL_BYTES 16

typedef struct {
    uint8_t b[GC_LABEL_BYTES];
} gc_label;

typedef enum {
    GC_GATE_AND = 0,
    GC_GATE_XOR = 1,
    GC_GATE_NOT = 2
} gc_gate_type;

typedef struct {
    uint16_t in0;
    uint16_t in1;
    uint16_t out;
    gc_gate_type type;
} gc_gate;

typedef struct {
    uint16_t n_wires;
    uint16_t n_inputs;
    uint16_t n_outputs;
    uint16_t *input_wires;
    uint16_t *output_wires;
    size_t n_gates;
    gc_gate *gates;
} gc_circuit;

typedef struct {
    uint16_t in0;
    uint16_t in1;
    uint16_t out;
    gc_gate_type type;
    gc_label table[4];
} gc_garbled_gate;

typedef struct {
    uint16_t n_wires;
    uint16_t n_inputs;
    uint16_t n_outputs;
    uint16_t *input_wires;
    uint16_t *output_wires;
    size_t n_gates;
    gc_garbled_gate *gates;
    gc_label *wire_labels0;
    gc_label *wire_labels1;
} gc_garbled_circuit;

typedef struct {
    size_t num_gates;
    size_t num_and_gates;
    size_t num_xor_gates;
    size_t num_not_gates;
    size_t num_ciphertexts;
    size_t ciphertext_bytes;
} gc_stats;

int gc_eval_clear(
    const gc_circuit *c,
    const uint8_t    *inputs,
    uint8_t          *outputs
);

int gc_garble(
    const gc_circuit    *plain,
    gc_garbled_circuit **out_gc
);

int gc_eval_garbled(
    const gc_garbled_circuit *gc,
    const gc_label           *input_labels,
    gc_label                 *output_labels
);

int gc_decode_outputs(
    const gc_garbled_circuit *gc,
    const gc_label           *output_labels,
    uint8_t                  *outputs_bits
);

void gc_garbled_free(gc_garbled_circuit *gc);

void gc_compute_stats(const gc_garbled_circuit *gc, gc_stats *stats);

gc_circuit *gc_circuit_and_2();

gc_circuit *gc_circuit_xor_2();

gc_circuit *gc_circuit_eq_2bit();

void gc_circuit_free(gc_circuit *c);

#ifdef __cplusplus
}
#endif
