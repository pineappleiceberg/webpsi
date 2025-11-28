#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "psi_gc.h"

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

static void fill_random(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        buf[i] = (uint8_t)rand();
    }
}

int main(void) {
    const size_t elem_bytes = 16;
    const size_t elem_bits = elem_bytes * 8;
    const size_t count = 1024;

    uint8_t *A = (uint8_t *)malloc(count * elem_bytes);
    uint8_t *B = (uint8_t *)malloc(count * elem_bytes);
    uint8_t *mask = (uint8_t *)malloc(count);
    if (!A || !B || !mask) {
        fprintf(stderr, "psi_bench: malloc failed\n");
        free(A);
        free(B);
        free(mask);
        return 1;
    }

    srand(12345);
    fill_random(A, count * elem_bytes);
    fill_random(B, count * elem_bytes);

    psi_gc_ctx *ctx = psi_gc_create(count, elem_bits);
    if (!ctx) {
        fprintf(stderr, "psi_bench: psi_gc_create returned NULL\n");
        free(A);
        free(B);
        free(mask);
        return 1;
    }
    if (psi_gc_prepare_circuit(ctx) != 0) {
        fprintf(stderr, "psi_bench: psi_gc_prepare_circuit failed\n");
        psi_gc_destroy(ctx);
        free(A);
        free(B);
        free(mask);
        return 1;
    }

    double t0 = now_ms();
    int rc = psi_gc_compute(ctx, A, B, count, mask);
    double t1 = now_ms();
    psi_gc_destroy(ctx);

    if (rc != 0) {
        fprintf(stderr, "psi_bench: psi_gc_compute rc=%d\n", rc);
        free(A);
        free(B);
        free(mask);
        return 1;
    }

    size_t inter = 0;
    for (size_t i = 0; i < count; ++i) {
        if (mask[i]) {
            inter++;
        }
    }

    printf("PSI benchmark:\n");
    printf("  count       = %zu\n", count);
    printf("  elem_bytes  = %zu\n", elem_bytes);
    printf("  time_ms     = %.3f\n", t1 - t0);
    printf("  intersection= %zu\n", inter);

    free(A);
    free(B);
    free(mask);
    return 0;
}
