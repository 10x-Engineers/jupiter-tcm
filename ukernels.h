#ifndef __UKERNELS_H__
#define __UKERNELS_H__

#include <riscv_vector.h>

// Operates on Tile Size: M0=4, N0=16, K0=1
void mmt4d_s32s32s32_tcm(int* lhs_full_dram, int* rhs_full_dma, int* res_full_dram, int* rhs_tcm, size_t M1, size_t N1, size_t K1, size_t M0, size_t N0, size_t K0, size_t total_rhs_panels_to_prefetch) {
    for (int j = 0; j < N1; j++) {
        if (j % total_rhs_panels_to_prefetch == 0) {
            aimm_memcpy(rhs_tcm, &rhs_full_dma[j * K1 * N0 * K0], N0 * K0 * K1 * sizeof(int) * total_rhs_panels_to_prefetch);
        }

        int* rhs_panel_base = rhs_tcm + (j % total_rhs_panels_to_prefetch) * K1 * N0 * K0;

        for (int i = 0; i < M1; i++) {
            int* lhs_panel = &lhs_full_dram[i * K1 * M0 * K0];
            int* out_panel = &res_full_dram[i * N1 * M0 * N0 + j * M0 * N0];

            int *rhs_panel = rhs_panel_base;

            vint32m2_t acc0, acc1, acc2, acc3;
            size_t vl = N0;

            acc0 = __riscv_vle32_v_i32m2(out_panel, vl);
            acc1 = __riscv_vle32_v_i32m2(out_panel + N0, vl);
            acc2 = __riscv_vle32_v_i32m2(out_panel + N0 * 2, vl);
            acc3 = __riscv_vle32_v_i32m2(out_panel + N0 * 3, vl);

            int *lhs_ptr = lhs_panel;

            for (int k = 0; k < K1; ++k) {
                vint32m2_t rhs = __riscv_vle32_v_i32m2(rhs_panel, vl);
                rhs_panel += N0;

                acc0 = __riscv_vmacc_vx_i32m2(acc0, *lhs_ptr++, rhs, vl);
                acc1 = __riscv_vmacc_vx_i32m2(acc1, *lhs_ptr++, rhs, vl);
                acc2 = __riscv_vmacc_vx_i32m2(acc2, *lhs_ptr++, rhs, vl);
                acc3 = __riscv_vmacc_vx_i32m2(acc3, *lhs_ptr++, rhs, vl);
            }
            __riscv_vse32_v_i32m2(out_panel, acc0, vl);
            __riscv_vse32_v_i32m2(out_panel + N0, acc1, vl);
            __riscv_vse32_v_i32m2(out_panel + N0 * 2, acc2, vl);
            __riscv_vse32_v_i32m2(out_panel + N0 * 3, acc3, vl);
        }
    }
}

// Operates on Tile Size: M0=4, N0=32, K0=1
void mmt4d_s8s8s32_tcm(int8_t* lhs_full_dram, int8_t* rhs_full_dma, int* res_full_dram, int8_t* rhs_tcm, size_t M1, size_t N1, size_t K1, size_t M0, size_t N0, size_t K0, size_t total_rhs_panels_to_prefetch) {
    for (int j = 0; j < N1; j++) {
        if (j % total_rhs_panels_to_prefetch == 0) {
            aimm_memcpy(rhs_tcm, &rhs_full_dma[j * K1 * N0 * K0], N0 * K0 * K1 * sizeof(int8_t) * total_rhs_panels_to_prefetch);
        }

        int8_t* rhs_panel_base = rhs_tcm + (j % total_rhs_panels_to_prefetch) * K1 * N0 * K0;

        for (int i = 0; i < M1; i++) {
            int8_t* lhs_panel = &lhs_full_dram[i * K1 * M0 * K0];
            int* out_panel = &res_full_dram[i * N1 * M0 * N0 + j * M0 * N0];

            int8_t *rhs_panel = rhs_panel_base;

            vint32m4_t acc0, acc1, acc2, acc3;
            size_t vl = N0;

            acc0 = __riscv_vle32_v_i32m4(out_panel, vl);
            acc1 = __riscv_vle32_v_i32m4(out_panel + N0, vl);
            acc2 = __riscv_vle32_v_i32m4(out_panel + N0 * 2, vl);
            acc3 = __riscv_vle32_v_i32m4(out_panel + N0 * 3, vl);

            int8_t* lhs_ptr = lhs_panel;

            for (int k = 0; k < K1; ++k) {
                vint8m1_t rhs_s8 = __riscv_vle8_v_i8m1(rhs_panel, vl);
                vint16m2_t rhs_s16 = __riscv_vwcvt_x_x_v_i16m2(rhs_s8, vl);
                rhs_panel += N0;

                acc0 = __riscv_vwmacc_vx_i32m4(acc0, *lhs_ptr++, rhs_s16, vl);
                acc1 = __riscv_vwmacc_vx_i32m4(acc1, *lhs_ptr++, rhs_s16, vl);
                acc2 = __riscv_vwmacc_vx_i32m4(acc2, *lhs_ptr++, rhs_s16, vl);
                acc3 = __riscv_vwmacc_vx_i32m4(acc3, *lhs_ptr++, rhs_s16, vl);
            }
            __riscv_vse32_v_i32m4(out_panel, acc0, vl);
            __riscv_vse32_v_i32m4(out_panel + N0, acc1, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 2, acc2, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 3, acc3, vl);
        }
    }
}

// Operates on Tile Size: M0=4, N0=16, K0=1
void mmt4d_s32s32s32(int* lhs_packed, int* rhs_packed, int* res_packed, size_t M1, size_t N1, size_t K1, size_t M0, size_t N0, size_t K0) {
    for (int i = 0; i < M1; i++) {
        int* lhs_panel = &lhs_packed[i * K1 * M0 * K0];
        for (int j = 0; j < N1; j++) {
            int* rhs_panel = &rhs_packed[j * K1 * N0 * K0];
            int* out_panel = &res_packed[i * N1 * M0 * N0 + j * M0 * N0];

            vint32m2_t acc0, acc1, acc2, acc3;
            size_t vl = N0;

            acc0 = __riscv_vle32_v_i32m2(out_panel, vl);
            acc1 = __riscv_vle32_v_i32m2(out_panel + N0, vl);
            acc2 = __riscv_vle32_v_i32m2(out_panel + N0 * 2, vl);
            acc3 = __riscv_vle32_v_i32m2(out_panel + N0 * 3, vl);

            int *lhs_ptr = lhs_panel;

            for (int k = 0; k < K1; ++k) {
                vint32m2_t rhs = __riscv_vle32_v_i32m2(rhs_panel, vl);

                rhs_panel += N0;

                int lhs[4];
                for (int x = 0; x < M0; ++x) {
                    lhs[x] = *lhs_ptr++;
                }

                acc0 = __riscv_vmacc_vx_i32m2(acc0, lhs[0], rhs, vl);
                acc1 = __riscv_vmacc_vx_i32m2(acc1, lhs[1], rhs, vl);
                acc2 = __riscv_vmacc_vx_i32m2(acc2, lhs[2], rhs, vl);
                acc3 = __riscv_vmacc_vx_i32m2(acc3, lhs[3], rhs, vl);
            }
            __riscv_vse32_v_i32m2(out_panel, acc0, vl);
            __riscv_vse32_v_i32m2(out_panel + N0, acc1, vl);
            __riscv_vse32_v_i32m2(out_panel + N0 * 2, acc2, vl);
            __riscv_vse32_v_i32m2(out_panel + N0 * 3, acc3, vl);
        }
    }
}

// Operates on Tile Size: M0=4, N0=32, K0=1
void mmt4d_s8s8s32(int8_t* lhs_packed, int8_t* rhs_packed, int* res_packed, size_t M1, size_t N1, size_t K1, size_t M0, size_t N0, size_t K0) {
    for (int i = 0; i < M1; i++) {
        int8_t* lhs_panel = &lhs_packed[i * K1 * M0 * K0];
        for (int j = 0; j < N1; j++) {
            int8_t* rhs_panel = &rhs_packed[j * K1 * N0 * K0];
            int* out_panel = &res_packed[i * N1 * M0 * N0 + j * M0 * N0];

            vint32m4_t acc0, acc1, acc2, acc3;
            size_t vl = N0;

            acc0 = __riscv_vle32_v_i32m4(out_panel, vl);
            acc1 = __riscv_vle32_v_i32m4(out_panel + N0, vl);
            acc2 = __riscv_vle32_v_i32m4(out_panel + N0 * 2, vl);
            acc3 = __riscv_vle32_v_i32m4(out_panel + N0 * 3, vl);

            int8_t* lhs_ptr = lhs_panel;

            for (int k = 0; k < K1; ++k) {
                vint8m1_t rhs_s8 = __riscv_vle8_v_i8m1(rhs_panel, vl);
                vint16m2_t rhs_s16 = __riscv_vwcvt_x_x_v_i16m2(rhs_s8, vl);
                rhs_panel += N0;

                acc0 = __riscv_vwmacc_vx_i32m4(acc0, *lhs_ptr++, rhs_s16, vl);
                acc1 = __riscv_vwmacc_vx_i32m4(acc1, *lhs_ptr++, rhs_s16, vl);
                acc2 = __riscv_vwmacc_vx_i32m4(acc2, *lhs_ptr++, rhs_s16, vl);
                acc3 = __riscv_vwmacc_vx_i32m4(acc3, *lhs_ptr++, rhs_s16, vl);
            }
            __riscv_vse32_v_i32m4(out_panel, acc0, vl);
            __riscv_vse32_v_i32m4(out_panel + N0, acc1, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 2, acc2, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 3, acc3, vl);
        }
    }
}

void matmul_s32s32s32(int** lhs, int** rhs, int** res, size_t M, size_t N, size_t K) {
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            for (int k = 0; k < K; k++) {
                res[i][j] += lhs[i][k] * rhs[k][j];
            }
        }
    }
}

void matmul_s8s8s32(int8_t** lhs, int8_t** rhs, int** res, size_t M, size_t N, size_t K) {
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            for (int k = 0; k < K; k++) {
                res[i][j] += lhs[i][k] * rhs[k][j];
            }
        }
    }
}

#endif
