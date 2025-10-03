#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sched.h>
#include <assert.h>

#include <riscv_vector.h>

#include "aimm.h"

#define TCM_ALLOCATION_SIZE (128 * 1024)

void intialize_matrix(int** matrix, int shape0, int shape1, int value) {
    for (int i = 0; i < shape0; i++) {
        for (int j = 0; j < shape1; j++) {
            matrix[i][j] = value++;
        }
    }
}

void intialize_to_zero(int** matrix, int shape0, int shape1) {
    for (int i = 0; i < shape0; i++) {
        for (int j = 0; j < shape1; j++) {
            matrix[i][j] = 0;
        }
    }
}

void pack(int** matrix, int* array, int shape0, int shape1, int inner_parallel, int inner_reduction) {
    int index = 0;
    for (int i = 0; i < shape0; i+=inner_parallel) {
        for (int k = 0; k < shape1; k+=inner_reduction) {
            for (int i0 = 0; i0 < inner_parallel; ++i0) {
                for (int k0 = 0; k0 < inner_reduction; ++k0) {
                    array[index++] = matrix[i + i0][k + k0];
                }
            }
        }
    }
}

void transpose(int** matrix, int** tranposed, int shape0, int shape1) {
    for (int i = 0; i < shape0; i++) {
        for (int j = 0; j < shape1; j++) {
            tranposed[j][i] = matrix[i][j];
        }
    }
}

void unpack(int *array, int** matrix, int shape0, int shape1, int inner_parallel, int inner_reduction) {
    int index = 0;
    for (int i = 0; i < shape0; i+=inner_parallel) {
        for (int k = 0; k < shape1; k+=inner_reduction) {
            for (int i0 = 0; i0 < inner_parallel; ++i0) {
                for (int k0 = 0; k0 < inner_reduction; ++k0) {
                    matrix[i + i0][k + k0] = array[index++];
                }
            }
        }
    }
}

void compare(int **matrix1, int **matrix2, int shape0, int shape1) {
    for (int i = 0; i < shape0; ++i) {
        for (int j = 0; j < shape1; ++j) {
            if (matrix1[i][j] != matrix2[i][j]) {
                printf("Error at [%d], [%d] ---> %d, %d\n", i, j, matrix1[i][j], matrix2[i][j]);
                return;
            }
        }
    }
}

void free2D(int** matrix, int shape0) {
    for (int i = 0; i < shape0; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

void mmt4d_rvv_tcm(int* lhs_full_dram, int* rhs_full_dma, int* res_full_dram, int* rhs_tcm, int M, int N, int K, int M0, int N0, int K0, int total_rhs_panels_to_prefetch) {
    for (int j = 0; j < N; j++) {
        if (j % total_rhs_panels_to_prefetch == 0) {
            aimm_memcpy(rhs_tcm, &rhs_full_dma[j * K * N0 * K0], N0 * K0 * K * sizeof(int) * total_rhs_panels_to_prefetch);
        }

        int* rhs_panel_base = rhs_tcm + (j % total_rhs_panels_to_prefetch) * K * N0 * K0;

        for (int i = 0; i < M; i++) {
            int* lhs_panel = &lhs_full_dram[i * K * M0 * K0];
            int* out_panel = &res_full_dram[i * N * M0 * N0 + j * M0 * N0];

            int *rhs_panel = rhs_panel_base;

            vint32m2_t acc0, acc1, acc2, acc3;
            size_t vl = N0;

            acc0 = __riscv_vle32_v_i32m2(out_panel, vl);
            acc1 = __riscv_vle32_v_i32m2(out_panel + N0, vl);
            acc2 = __riscv_vle32_v_i32m2(out_panel + N0 * 2, vl);
            acc3 = __riscv_vle32_v_i32m2(out_panel + N0 * 3, vl);

            int *lhs_ptr = lhs_panel;

            for (int k = 0; k < K; ++k) {
                vint32m2_t rhs = __riscv_vle32_v_i32m2(rhs_panel, vl);
                rhs_panel += N0; // Move to the next K0 x N0 block in RHS

                acc0 = __riscv_vmacc_vx_i32m2(acc0, *lhs_ptr++, rhs, vl);
                acc1 = __riscv_vmacc_vx_i32m2(acc1, *lhs_ptr++, rhs, vl);
                acc2 = __riscv_vmacc_vx_i32m2(acc2, *lhs_ptr++, rhs, vl);
                acc3 = __riscv_vmacc_vx_i32m2(acc3, *lhs_ptr++, rhs, vl);
            }

            // Store the accumulated results back to DRAM
            __riscv_vse32_v_i32m2(out_panel, acc0, vl);
            __riscv_vse32_v_i32m2(out_panel + N0, acc1, vl);
            __riscv_vse32_v_i32m2(out_panel + N0 * 2, acc2, vl);
            __riscv_vse32_v_i32m2(out_panel + N0 * 3, acc3, vl);
        }
    }
}

void test_mmt4d_rvv_tcm(int** result, int** lhs, int** rhs, int M, int N, int K, int M0, int N0, int K0) {
    int M1 = M / M0;
    int N1 = N / N0;
    int K1 = K / K0;

    if (aimm_init() < 0) {
        printf("Failed to initialize AIMM/TCM/UDMA system.\n");
        return;
    }

    int** rhs_t = (int**)malloc(N * sizeof(int*));
    for (int i = 0; i < N; i++) {
        rhs_t[i] = (int*)malloc(K * sizeof(int));
    }

    int* lhs_packed = (int*)malloc(M * K * sizeof(int));
    int* rhs_t_packed = (int*)aimm_dram_malloc(N * K * sizeof(int));
    int* res_packed = (int*)malloc(M * N * sizeof(int));

    size_t size_of_rhs_panel = N0 * K0 * K1 * sizeof(int);
	// int total_rhs_panels_to_prefetch = TCM_ALLOCATION_SIZE / size_of_rhs_panel;
	int total_rhs_panels_to_prefetch = 2;

    size_t rhs_tcm_space = size_of_rhs_panel * total_rhs_panels_to_prefetch;
    assert(rhs_tcm_space <= TCM_ALLOCATION_SIZE);

	printf("Size of RHS panels in TCM: %.2f KB x %d\n", size_of_rhs_panel / 1024.0, total_rhs_panels_to_prefetch);

    void* tcm_base = aimm_tcm_malloc_sync(TCM_ALLOCATION_SIZE, 0);
    if (tcm_base == NULL) {
        printf("Failed to allocate TCM memory for tiles.\n");
        free(lhs_packed);
        aimm_dram_free(rhs_t_packed);
        free(res_packed);
        free2D(rhs_t, N);
        aimm_deinit();
        return;
    }
    int* rhs_t_packed_tcm = (int*)tcm_base;

    struct timespec start, end;
    pack(lhs, lhs_packed, M, K, M0, K0);
    transpose(rhs, rhs_t, K, N);
    pack(rhs_t, rhs_t_packed, N, K, N0, K0);

    clock_gettime(CLOCK_MONOTONIC, &start);
    mmt4d_rvv_tcm(lhs_packed, rhs_t_packed, res_packed, rhs_t_packed_tcm, M1, N1, K1, M0, N0, K0, total_rhs_panels_to_prefetch);
    clock_gettime(CLOCK_MONOTONIC, &end);

    unpack(res_packed, result, M, N, M0, N0);
    double mmt4d_rvv_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time for mmt4d: %f seconds\n", mmt4d_rvv_time);

    aimm_tcm_free(rhs_t_packed_tcm);
    free(lhs_packed);
    aimm_dram_free(rhs_t_packed);
    free(res_packed);
    free2D(rhs_t, N);
    aimm_deinit();
}

void mmt4d_rvv(int* lhs_packed, int* rhs_packed, int* res_packed, int M1, int N1, int K1, int M0, int N0, int K0) {
    struct timespec start, end;

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

void test_mmt4d_rvv(int** result, int** lhs, int** rhs, int M, int N, int K, int M0, int N0, int K0) {
    int M1 = M / M0;
    int N1 = N / N0;
    int K1 = K / K0;

    int** rhs_t = (int**)malloc(N * sizeof(int*));
    for (int i = 0; i < N; i++) {
        rhs_t[i] = (int*)malloc(K * sizeof(int));
    }

    int* lhs_packed = (int*)malloc(M * K * sizeof(int));
    int* rhs_t_packed = (int*)malloc(N * K * sizeof(int));
    int* res_packed = (int*)malloc(M * N * sizeof(int));

    struct timespec start, end;
    pack(lhs, lhs_packed, M, K, M0, K0);
    transpose(rhs, rhs_t, K, N);
    pack(rhs_t, rhs_t_packed, N, K, N0, K0);

    clock_gettime(CLOCK_MONOTONIC, &start);
    mmt4d_rvv(lhs_packed, rhs_t_packed, res_packed, M1, N1, K1, M0, N0, K0);
    clock_gettime(CLOCK_MONOTONIC, &end);

    unpack(res_packed, result, M, N, M0, N0);
    double mmt4d_rvv_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time for mmt4d: %f seconds\n", mmt4d_rvv_time);

    free2D(rhs_t, N);
    free(lhs_packed);
    free(rhs_t_packed);
    free(res_packed);
}

void matmul(int** lhs, int** rhs, int** res, int M, int N, int K) {
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            for (int k = 0; k < K; k++) {
                res[i][j] += lhs[i][k] * rhs[k][j];
            }
        }
    }
}

void test_matmul(int** result, int** lhs, int** rhs, int M, int N, int K) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    matmul(lhs, rhs, result, M, N, K);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double matmul_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time for matmul: %f seconds\n", matmul_time);
}

int main(int agrc, char* argv[]) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    pid_t pid = getpid();
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_setaffinity");
        return 1;
    }

	int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    int K = atoi(argv[3]);
    int M0 = atoi(argv[4]);
    int N0 = atoi(argv[5]);
    int K0 = atoi(argv[6]);

    int** lhs = (int**)malloc(M * sizeof(int*));
    for (int i = 0; i < M; i++) {
        lhs[i] = (int*)malloc(K * sizeof(int));
    }
    int** rhs = (int**)malloc(K * sizeof(int*));
    for (int i = 0; i < K; i++) {
        rhs[i] = (int*)malloc(N * sizeof(int));
    }
    int** res_matmul = (int**)malloc(M * sizeof(int*));
    for (int i = 0; i < M; i++) {
        res_matmul[i] = (int*)malloc(N * sizeof(int));
    }
    int** res_mmt4d = (int**)malloc(M * sizeof(int*));
    for (int i = 0; i < M; i++) {
        res_mmt4d[i] = (int*)malloc(N * sizeof(int));
    }
    int** res_mmt4d_tcm = (int**)malloc(M * sizeof(int*));
    for (int i = 0; i < M; i++) {
        res_mmt4d_tcm[i] = (int*)malloc(N * sizeof(int));
    }

    intialize_matrix(lhs, M, K, 1);
    intialize_matrix(rhs, K, N, 1);
    intialize_to_zero(res_matmul, M, N);
    intialize_to_zero(res_mmt4d, M, N);
    intialize_to_zero(res_mmt4d_tcm, M, N);

    test_matmul(res_matmul, lhs, rhs, M, N, K);
    test_mmt4d_rvv(res_mmt4d, lhs, rhs, M, N, K, M0, N0, K0);
    test_mmt4d_rvv_tcm(res_mmt4d_tcm, lhs, rhs, M, N, K, M0, N0, K0);

    compare(res_matmul, res_mmt4d, M, N);
    compare(res_mmt4d, res_mmt4d_tcm, M, N);

    printf("All results match!\n");

    free2D(lhs, M);
    free2D(rhs, K);
    free2D(res_matmul, M);
    free2D(res_mmt4d, M);
    free2D(res_mmt4d_tcm, M);

    return 0;
}
