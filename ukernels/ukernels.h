#ifndef __UKERNELS_H__
#define __UKERNELS_H__

#include <riscv_vector.h>

// Operates on Tile Size: M0=7, N0=32, K0=1
void mmt4d_s8s8s32_tcm(int8_t* lhs, int8_t* rhs, int* res, int8_t* tcm, size_t M1, size_t N1, size_t N1s, size_t N1e, size_t K1, size_t M0, size_t N0, size_t K0, size_t total_rhs_panels_to_prefetch) {
    for (int j = N1s; j < N1e; j++) {
        if (j % total_rhs_panels_to_prefetch == 0) {
            // memcpy
            size_t copy_size_bytes = N0 * K0 * K1 * sizeof(int8_t) * total_rhs_panels_to_prefetch;

            size_t total_elements = copy_size_bytes;

            int8_t* src = &rhs[j * K1 * N0 * K0];
            int8_t* dst = tcm;

            size_t vl;
            size_t offset = 0;
            while (offset < total_elements) {
                vl = __riscv_vsetvl_e8m4(total_elements - offset);
                vint8m4_t vec = __riscv_vle8_v_i8m4(src + offset, vl);
                __riscv_vse8_v_i8m4(dst + offset, vec, vl);
                offset += vl;
            }
        }

        int8_t* rhs_panel_base = tcm + (j % total_rhs_panels_to_prefetch) * K1 * N0 * K0;

        for (int i = 0; i < M1; i++) {
            int8_t* lhs_panel = &lhs[i * K1 * M0 * K0];
            int* out_panel = &res[i * N1 * M0 * N0 + j * M0 * N0];

            int8_t *rhs_panel = rhs_panel_base;

            vint32m4_t acc0, acc1, acc2, acc3, acc4, acc5, acc6;
            size_t vl = N0;

            acc0 = __riscv_vle32_v_i32m4(out_panel, vl);
            acc1 = __riscv_vle32_v_i32m4(out_panel + N0, vl);
            acc2 = __riscv_vle32_v_i32m4(out_panel + N0 * 2, vl);
            acc3 = __riscv_vle32_v_i32m4(out_panel + N0 * 3, vl);
            acc4 = __riscv_vle32_v_i32m4(out_panel + N0 * 4, vl);
            acc5 = __riscv_vle32_v_i32m4(out_panel + N0 * 5, vl);
            acc6 = __riscv_vle32_v_i32m4(out_panel + N0 * 6, vl);

            int8_t* lhs_ptr = lhs_panel;

            for (int k = 0; k < K1; ++k) {
                vint8m1_t rhs_s8 = __riscv_vle8_v_i8m1(rhs_panel, vl);
                vint16m2_t rhs_s16 = __riscv_vwcvt_x_x_v_i16m2(rhs_s8, vl);
                rhs_panel += N0;

                acc0 = __riscv_vwmacc_vx_i32m4(acc0, *lhs_ptr++, rhs_s16, vl);
                acc1 = __riscv_vwmacc_vx_i32m4(acc1, *lhs_ptr++, rhs_s16, vl);
                acc2 = __riscv_vwmacc_vx_i32m4(acc2, *lhs_ptr++, rhs_s16, vl);
                acc3 = __riscv_vwmacc_vx_i32m4(acc3, *lhs_ptr++, rhs_s16, vl);
                acc4 = __riscv_vwmacc_vx_i32m4(acc4, *lhs_ptr++, rhs_s16, vl);
                acc5 = __riscv_vwmacc_vx_i32m4(acc5, *lhs_ptr++, rhs_s16, vl);
                acc6 = __riscv_vwmacc_vx_i32m4(acc6, *lhs_ptr++, rhs_s16, vl);
            }
            __riscv_vse32_v_i32m4(out_panel, acc0, vl);
            __riscv_vse32_v_i32m4(out_panel + N0, acc1, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 2, acc2, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 3, acc3, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 4, acc4, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 5, acc5, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 6, acc6, vl);
        }
    }
}

// Operates on Tile Size: M0=1, N0=32, K0=1
void mmt4d_s8s8s32_tcm_narrow(int8_t* lhs, int8_t* rhs, int* res, int8_t* tcm, size_t M1, size_t N1, size_t N1s, size_t N1e, size_t K1, size_t M0, size_t N0, size_t K0, size_t total_rhs_panels_to_prefetch) {
    for (int j = N1s; j < N1e; j++) {
        if (j % total_rhs_panels_to_prefetch == 0) {
            // memcpy
            size_t copy_size_bytes = N0 * K0 * K1 * sizeof(int8_t) * total_rhs_panels_to_prefetch;

            size_t total_elements = copy_size_bytes;

            int8_t* src = &rhs[j * K1 * N0 * K0];
            int8_t* dst = tcm;

            size_t vl;
            size_t offset = 0;
            while (offset < total_elements) {
                vl = __riscv_vsetvl_e8m4(total_elements - offset);
                vint8m4_t vec = __riscv_vle8_v_i8m4(src + offset, vl);
                __riscv_vse8_v_i8m4(dst + offset, vec, vl);
                offset += vl;
            }
        }

        int8_t* rhs_panel_base = tcm + (j % total_rhs_panels_to_prefetch) * K1 * N0 * K0;

        for (int i = 0; i < M1; i++) {
            int8_t* lhs_panel = &lhs[i * K1 * M0 * K0];
            int* out_panel = &res[i * N1 * M0 * N0 + j * M0 * N0];

            int8_t *rhs_panel = rhs_panel_base;

            vint32m4_t acc0;
            size_t vl = N0;

            acc0 = __riscv_vle32_v_i32m4(out_panel, vl);

            int8_t* lhs_ptr = lhs_panel;

            for (int k = 0; k < K1; ++k) {
                vint8m1_t rhs_s8 = __riscv_vle8_v_i8m1(rhs_panel, vl);
                vint16m2_t rhs_s16 = __riscv_vwcvt_x_x_v_i16m2(rhs_s8, vl);
                rhs_panel += N0;

                acc0 = __riscv_vwmacc_vx_i32m4(acc0, *lhs_ptr++, rhs_s16, vl);
            }
            __riscv_vse32_v_i32m4(out_panel, acc0, vl);
        }
    }
}

// Operates on Tile Size: M0=7, N0=32, K0=1
void mmt4d_s8s8s32(int8_t* lhs, int8_t* rhs, int* res, size_t M1, size_t N1, size_t N1s, size_t N1e, size_t K1, size_t M0, size_t N0, size_t K0) {
    for (int i = 0; i < M1; i++) {
        int8_t* lhs_panel = &lhs[i * K1 * M0 * K0];
        for (int j = N1s; j < N1e; j++) {
            int8_t* rhs_panel = &rhs[j * K1 * N0 * K0];
            int* out_panel = &res[i * N1 * M0 * N0 + j * M0 * N0];

            vint32m4_t acc0, acc1, acc2, acc3, acc4, acc5, acc6;
            size_t vl = N0;

            acc0 = __riscv_vle32_v_i32m4(out_panel, vl);
            acc1 = __riscv_vle32_v_i32m4(out_panel + N0, vl);
            acc2 = __riscv_vle32_v_i32m4(out_panel + N0 * 2, vl);
            acc3 = __riscv_vle32_v_i32m4(out_panel + N0 * 3, vl);
            acc4 = __riscv_vle32_v_i32m4(out_panel + N0 * 4, vl);
            acc5 = __riscv_vle32_v_i32m4(out_panel + N0 * 5, vl);
            acc6 = __riscv_vle32_v_i32m4(out_panel + N0 * 6, vl);

            int8_t* lhs_ptr = lhs_panel;

            for (int k = 0; k < K1; ++k) {
                vint8m1_t rhs_s8 = __riscv_vle8_v_i8m1(rhs_panel, vl);
                vint16m2_t rhs_s16 = __riscv_vwcvt_x_x_v_i16m2(rhs_s8, vl);
                rhs_panel += N0;

                acc0 = __riscv_vwmacc_vx_i32m4(acc0, *lhs_ptr++, rhs_s16, vl);
                acc1 = __riscv_vwmacc_vx_i32m4(acc1, *lhs_ptr++, rhs_s16, vl);
                acc2 = __riscv_vwmacc_vx_i32m4(acc2, *lhs_ptr++, rhs_s16, vl);
                acc3 = __riscv_vwmacc_vx_i32m4(acc3, *lhs_ptr++, rhs_s16, vl);
                acc4 = __riscv_vwmacc_vx_i32m4(acc4, *lhs_ptr++, rhs_s16, vl);
                acc5 = __riscv_vwmacc_vx_i32m4(acc5, *lhs_ptr++, rhs_s16, vl);
                acc6 = __riscv_vwmacc_vx_i32m4(acc6, *lhs_ptr++, rhs_s16, vl);
            }
            __riscv_vse32_v_i32m4(out_panel, acc0, vl);
            __riscv_vse32_v_i32m4(out_panel + N0, acc1, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 2, acc2, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 3, acc3, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 4, acc4, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 5, acc5, vl);
            __riscv_vse32_v_i32m4(out_panel + N0 * 6, acc6, vl);
        }
    }
}

// Operates on Tile Size: M0=1, N0=32, K0=1
void mmt4d_s8s8s32_narrow(int8_t* lhs, int8_t* rhs, int* res, size_t M1, size_t N1, size_t N1s, size_t N1e, size_t K1, size_t M0, size_t N0, size_t K0) {
    for (int i = 0; i < M1; i++) {
        int8_t* lhs_panel = &lhs[i * K1 * M0 * K0];
        for (int j = N1s; j < N1e; j++) {
            int8_t* rhs_panel = &rhs[j * K1 * N0 * K0];
            int* out_panel = &res[i * N1 * M0 * N0 + j * M0 * N0];

            vint32m4_t acc0;
            size_t vl = N0;

            acc0 = __riscv_vle32_v_i32m4(out_panel, vl);

            int8_t* lhs_ptr = lhs_panel;

            for (int k = 0; k < K1; ++k) {
                vint8m1_t rhs_s8 = __riscv_vle8_v_i8m1(rhs_panel, vl);
                vint16m2_t rhs_s16 = __riscv_vwcvt_x_x_v_i16m2(rhs_s8, vl);
                rhs_panel += N0;

                acc0 = __riscv_vwmacc_vx_i32m4(acc0, *lhs_ptr++, rhs_s16, vl);
            }
            __riscv_vse32_v_i32m4(out_panel, acc0, vl);
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
