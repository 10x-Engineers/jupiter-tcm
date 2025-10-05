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
#include "helpers.h"
#include "ukernels.h"

#define TCM_ALLOCATION_SIZE (128 * 1024)

void test_mmt4d_rvv_tcm(int** result, int** lhs, int** rhs, size_t M, size_t N, size_t K, size_t M0, size_t N0, size_t K0) {
    size_t M1 = M / M0;
    size_t N1 = N / N0;
    size_t K1 = K / K0;

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
	size_t total_rhs_panels_to_prefetch = 2;

    size_t rhs_tcm_space = size_of_rhs_panel * total_rhs_panels_to_prefetch;
    assert(rhs_tcm_space <= TCM_ALLOCATION_SIZE);

	printf("Size of RHS panels in TCM: %.2f KB x %zu\n", size_of_rhs_panel / 1024.0, total_rhs_panels_to_prefetch);

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
    mmt4d_s32s32s32_tcm(lhs_packed, rhs_t_packed, res_packed, rhs_t_packed_tcm, M1, N1, K1, M0, N0, K0, total_rhs_panels_to_prefetch);
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


void test_mmt4d_rvv(int** result, int** lhs, int** rhs, size_t M, size_t N, size_t K, size_t M0, size_t N0, size_t K0) {
    size_t M1 = M / M0;
    size_t N1 = N / N0;
    size_t K1 = K / K0;

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
    mmt4d_s32s32s32(lhs_packed, rhs_t_packed, res_packed, M1, N1, K1, M0, N0, K0);
    clock_gettime(CLOCK_MONOTONIC, &end);

    unpack(res_packed, result, M, N, M0, N0);
    double mmt4d_rvv_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time for mmt4d: %f seconds\n", mmt4d_rvv_time);

    free2D(rhs_t, N);
    free(lhs_packed);
    free(rhs_t_packed);
    free(res_packed);
}

void test_matmul(int** result, int** lhs, int** rhs, size_t M, size_t N, size_t K) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    matmul_s32s32s32(lhs, rhs, result, M, N, K);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double matmul_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time for matmul: %f seconds\n", matmul_time);
}

int main(int agrc, char* argv[]) {
    // setting process affinity to AI Cores (0-3)
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    pid_t pid = getpid();
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_setaffinity");
        return 1;
    }

	size_t M = atoi(argv[1]);
    size_t N = atoi(argv[2]);
    size_t K = atoi(argv[3]);
    size_t M0 = atoi(argv[4]);
    size_t N0 = atoi(argv[5]);
    size_t K0 = atoi(argv[6]);

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
