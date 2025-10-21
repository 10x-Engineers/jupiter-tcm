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
#include <pthread.h>

#include "aimm.h"
#include "helpers.h"
#include "ukernels.h"

#define TCM_ALLOCATION_SIZE (128 * 1024)
#define NUM_WORKER_THREADS 4 // Cores 0, 1, 2, 3

#define NUM_WORKER_THREADS 4

typedef struct {
    int thread_id; // Core ID 0, 1, 2, 3
    size_t M1, N1, K1;
    size_t M0, N0, K0;
    int8_t* lhs_packed;
    int8_t* rhs_t_packed;
    int* res_packed;
    size_t start_N1_idx;
    size_t end_N1_idx;
    size_t total_rhs_panels_to_prefetch;
} mmt4d_tcm_thread_arg_t;


void* mmt4d_tcm_worker_thread(void* arg) {
    mmt4d_tcm_thread_arg_t* t_arg = (mmt4d_tcm_thread_arg_t*)arg;
    int core_id = t_arg->thread_id;

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core_id, &mask);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask) == -1) {
        perror("pthread_setaffinity_np failed");
        return NULL;
    }

    void* tcm_base = aimm_tcm_malloc_sync(TCM_ALLOCATION_SIZE, 0);
    if (tcm_base == NULL) {
        printf("Failed to allocate TCM memory for tiles.\n");
        return NULL;
    }
    int8_t* rhs_t_packed_tcm = (int8_t*)tcm_base;

    mmt4d_s8s8s32_tcm(t_arg->lhs_packed, t_arg->rhs_t_packed, t_arg->res_packed, rhs_t_packed_tcm, t_arg->M1, t_arg->N1, t_arg->start_N1_idx, t_arg->end_N1_idx, t_arg->K1, t_arg->M0, t_arg->N0, t_arg->K0, t_arg->total_rhs_panels_to_prefetch);

    aimm_tcm_free(tcm_base);
    return NULL;
}


void test_mmt4d_tcm_parallel(int** result, int8_t** lhs, int8_t** rhs, size_t M, size_t N, size_t K, size_t M0, size_t N0, size_t K0) {
    size_t M1 = M / M0;
    size_t N1 = N / N0;
    size_t K1 = K / K0;

    if (aimm_init() < 0) {
        printf("Failed to initialize AIMM/TCM/UDMA system.\n");
        return;
    }

    int8_t** rhs_t = (int8_t**)malloc(N * sizeof(int8_t*));
    for (size_t i = 0; i < N; i++) {
        rhs_t[i] = (int8_t*)malloc(K * sizeof(int8_t));
    }

    int8_t* lhs_packed = (int8_t*)malloc(M * K * sizeof(int8_t));
    int8_t* rhs_t_packed = (int8_t*)malloc(N * K * sizeof(int8_t));
    int* res_packed = (int*)malloc(M * N * sizeof(int));

    size_t size_of_rhs_panel = N0 * K0 * K1 * sizeof(int8_t);
    size_t total_rhs_panels_to_prefetch = TCM_ALLOCATION_SIZE / size_of_rhs_panel;
    size_t rhs_tcm_space = size_of_rhs_panel * total_rhs_panels_to_prefetch;
    assert(rhs_tcm_space <= TCM_ALLOCATION_SIZE);

    printf("Size of RHS panels in TCM: %.2f KB x %zu\n", size_of_rhs_panel / 1024.0, total_rhs_panels_to_prefetch);

    struct timespec start, end;
    pack(lhs, lhs_packed, M, K, M0, K0);
    transpose(rhs, rhs_t, K, N);
    pack(rhs_t, rhs_t_packed, N, K, N0, K0);

    // --- Thread Setup and Execution ---
    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_t threads[NUM_WORKER_THREADS];
    mmt4d_tcm_thread_arg_t args[NUM_WORKER_THREADS];

    size_t N1_per_thread = N1 / NUM_WORKER_THREADS;
    size_t N1_remainder = N1 % NUM_WORKER_THREADS;
    size_t current_N1_start = 0;

    for (int i = 0; i < NUM_WORKER_THREADS; ++i) {
        size_t N1_slice = N1_per_thread + (i < N1_remainder ? 1 : 0);

        args[i].thread_id = i; // Core ID 0, 1, 2, 3
        args[i].M1 = M1;
        args[i].N1 = N1;
        args[i].K1 = K1;
        args[i].M0 = M0;
        args[i].N0 = N0;
        args[i].K0 = K0;
        args[i].lhs_packed = lhs_packed;
        args[i].rhs_t_packed = rhs_t_packed;
        args[i].res_packed = res_packed;
        args[i].start_N1_idx = current_N1_start;
        args[i].end_N1_idx = current_N1_start + N1_slice;
        args[i].total_rhs_panels_to_prefetch = total_rhs_panels_to_prefetch;

        current_N1_start = args[i].end_N1_idx;

        if (pthread_create(&threads[i], NULL, mmt4d_tcm_worker_thread, &args[i]) != 0) {
            perror("pthread_create failed");
            free(lhs_packed);
            free(rhs_t_packed);
            free(res_packed);
            FREE2D(rhs_t, N);
            aimm_deinit();
        }
    }

    // Wait for all worker threads to complete
    for (int i = 0; i < NUM_WORKER_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    // --- End of Threading ---

    unpack(res_packed, result, M, N, M0, N0);
    double mmt4d_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time for parallel mmt4d with TCM: %f seconds\n", mmt4d_time);

    free(lhs_packed);
    free(rhs_t_packed);
    free(res_packed);
    FREE2D(rhs_t, N);
    aimm_deinit();
}

typedef struct {
    int thread_id;          // 0, 1, 2, 3
    size_t M1, N1, K1;
    size_t M0, N0, K0;
    int8_t* lhs_packed;
    int8_t* rhs_t_packed;
    int* res_packed;
    size_t start_N1_idx;
    size_t end_N1_idx;
} mmt4d_thread_arg_t;

void* mmt4d_worker_thread(void* arg) {
    mmt4d_thread_arg_t* t_arg = (mmt4d_thread_arg_t*)arg;
    int core_id = t_arg->thread_id;

    // Bind thread to its designated core (0, 1, 2, or 3)
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core_id, &mask);

    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask) == -1) {
        perror("pthread_setaffinity_np failed");
        return NULL;
    }

    mmt4d_s8s8s32(t_arg->lhs_packed, t_arg->rhs_t_packed, t_arg->res_packed, t_arg->M1, t_arg->N1, t_arg->start_N1_idx, t_arg->end_N1_idx, t_arg->K1, t_arg->M0, t_arg->N0, t_arg->K0);

    return NULL;
}

void test_mmt4d_parallel(int** result, int8_t** lhs, int8_t** rhs, size_t M, size_t N, size_t K, size_t M0, size_t N0, size_t K0) {
    size_t M1 = M / M0;
    size_t N1 = N / N0;
    size_t K1 = K / K0;

    int8_t** rhs_t = (int8_t**)malloc(N * sizeof(int8_t*));
    for (size_t i = 0; i < N; i++) {
        rhs_t[i] = (int8_t*)malloc(K * sizeof(int8_t));
    }

    int8_t* lhs_packed = (int8_t*)malloc(M * K * sizeof(int8_t));
    int8_t* rhs_t_packed = (int8_t*)malloc(N * K * sizeof(int8_t));
    int* res_packed = (int*)malloc(M * N * sizeof(int));

    struct timespec start, end;
    pack(lhs, lhs_packed, M, K, M0, K0);
    transpose(rhs, rhs_t, K, N);
    pack(rhs_t, rhs_t_packed, N, K, N0, K0);

    // --- Thread Setup and Execution ---
    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_t threads[NUM_WORKER_THREADS];
    mmt4d_thread_arg_t args[NUM_WORKER_THREADS];

    size_t N1_per_thread = N1 / NUM_WORKER_THREADS;
    size_t N1_remainder = N1 % NUM_WORKER_THREADS;
    size_t current_N1_start = 0;

    for (int i = 0; i < NUM_WORKER_THREADS; ++i) {
        size_t N1_slice = N1_per_thread + (i < N1_remainder ? 1 : 0);

        args[i].thread_id = i; // Core ID 0, 1, 2, 3
        args[i].M1 = M1;
        args[i].N1 = N1;
        args[i].K1 = K1;
        args[i].M0 = M0;
        args[i].N0 = N0;
        args[i].K0 = K0;
        args[i].lhs_packed = lhs_packed;
        args[i].rhs_t_packed = rhs_t_packed;
        args[i].res_packed = res_packed;
        args[i].start_N1_idx = current_N1_start;
        args[i].end_N1_idx = current_N1_start + N1_slice;

        current_N1_start = args[i].end_N1_idx;

        if (pthread_create(&threads[i], NULL, mmt4d_worker_thread, &args[i]) != 0) {
            perror("pthread_create failed");
            free(lhs_packed);
            free(rhs_t_packed);
            free(res_packed);
            FREE2D(rhs_t, N);
            return;
        }
    }

    // Wait for all worker threads to complete
    for (int i = 0; i < NUM_WORKER_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    // --- End of Threading ---

    unpack(res_packed, result, M, N, M0, N0);
    double mmt4d_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time for parallel mmt4d: %f seconds\n", mmt4d_time);

    // --- Cleanup ---
    free(lhs_packed);
    free(rhs_t_packed);
    free(res_packed);
    FREE2D(rhs_t, N);
}

void test_matmul(int** result, int8_t** lhs, int8_t** rhs, size_t M, size_t N, size_t K) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    matmul_s8s8s32(lhs, rhs, result, M, N, K);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double matmul_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time for matmul: %f seconds\n", matmul_time);
}

int main(int agrc, char* argv[]) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(4, &mask); // MAIN THREAD bound to Core 4

    pid_t pid = getpid();
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_setaffinity failed on Core 4");
        return 1;
    }
    printf("Main thread (setup/cleanup) bound to Core 4.\n");

    size_t M = atoi(argv[1]);
    size_t N = atoi(argv[2]);
    size_t K = atoi(argv[3]);
    size_t M0 = atoi(argv[4]);
    size_t N0 = atoi(argv[5]);
    size_t K0 = atoi(argv[6]);

    int8_t** lhs = (int8_t**)malloc(M * sizeof(int8_t*));
    for (size_t i = 0; i < M; i++) { lhs[i] = (int8_t*)malloc(K * sizeof(int8_t)); }
    int8_t** rhs = (int8_t**)malloc(K * sizeof(int8_t*));
    for (size_t i = 0; i < K; i++) { rhs[i] = (int8_t*)malloc(N * sizeof(int8_t)); }
    int** res_mmt4d = (int**)malloc(M * sizeof(int*));
    for (size_t i = 0; i < M; i++) { res_mmt4d[i] = (int*)malloc(N * sizeof(int)); }
    int** res_mmt4d_tcm = (int**)malloc(M * sizeof(int*));
    for (size_t i = 0; i < M; i++) { res_mmt4d_tcm[i] = (int*)malloc(N * sizeof(int)); }
    int** res_matmul = (int**)malloc(M * sizeof(int*));
    for (size_t i = 0; i < M; i++) { res_matmul[i] = (int*)malloc(N * sizeof(int)); }

    intialize_matrix(lhs, M, K, 1);
    intialize_matrix(rhs, K, N, 1);
    intialize_to_zero(res_mmt4d, M, N);
    intialize_to_zero(res_mmt4d_tcm, M, N);

    // test_matmul(res_matmul, lhs, rhs, M, N, K);
    test_mmt4d_parallel(res_mmt4d, lhs, rhs, M, N, K, M0, N0, K0);
    test_mmt4d_tcm_parallel(res_mmt4d_tcm, lhs, rhs, M, N, K, M0, N0, K0);

    compare(res_mmt4d_tcm, res_mmt4d, M, N);
    printf("All results match!\n");

    FREE2D(lhs, M);
    FREE2D(rhs, K);
    FREE2D(res_mmt4d_tcm, M);
    FREE2D(res_mmt4d, M);
    FREE2D(res_matmul, M);

    return 0;
}
