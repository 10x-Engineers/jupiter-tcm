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
#include <pthread.h> // Included for pthreads

#include "aimm.h"
#include "helpers.h"
#include "ukernels.h"

#define TCM_ALLOCATION_SIZE (128 * 1024)
#define NUM_WORKER_THREADS 4 // Cores 0, 1, 2, 3

#define NUM_WORKER_THREADS 4

// Thread argument structure (similar to the non-TCM version, plus TCM pointers)
typedef struct {
    int thread_id; // Core ID 0, 1, 2, 3
    size_t M1, N1, K1;
    size_t M0, N0, K0;
    int8_t* lhs_packed;
    int8_t* rhs_t_packed; // DMA memory address
    int* res_packed;
    int8_t* rhs_t_packed_tcm; // TCM base address
    size_t start_N1_idx;
    size_t end_N1_idx;
    size_t total_rhs_panels_to_prefetch;
} mmt4d_tcm_thread_arg_t;


// Worker function for the TCM kernel
void* mmt4d_tcm_worker_thread(void* arg) {
    mmt4d_tcm_thread_arg_t* t_arg = (mmt4d_tcm_thread_arg_t*)arg;
    int core_id = t_arg->thread_id;

    // 1. Bind thread to its designated core
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core_id, &mask);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask) == -1) {
        perror("pthread_setaffinity_np failed");
        return NULL;
    }

    // TCM ALLOCATION (Shared by all threads)
    void* tcm_base = aimm_tcm_malloc_sync(TCM_ALLOCATION_SIZE, 0);
    if (tcm_base == NULL) {
        printf("Failed to allocate TCM memory for tiles.\n");
    }
    int8_t* rhs_t_packed_tcm = (int8_t*)tcm_base;

    // We must replicate the core logic of mmt4d_s8s8s32_tcm, but restrict the j-loop.
    // NOTE: This assumes 'aimm_memcpy' is thread-safe or is configured for concurrent use
    // from multiple cores targeting the same TCM area (with non-overlapping ranges).

    size_t M1 = t_arg->M1;
    size_t N1 = t_arg->N1;
    size_t K1 = t_arg->K1;
    size_t M0 = t_arg->M0;
    size_t N0 = t_arg->N0;
    size_t K0 = t_arg->K0;
    size_t total_rhs_panels_to_prefetch = t_arg->total_rhs_panels_to_prefetch;

    mmt4d_s8s8s32_tcm(t_arg->lhs_packed, t_arg->rhs_t_packed, t_arg->res_packed, rhs_t_packed_tcm, t_arg->M1, t_arg->N1, t_arg->start_N1_idx, t_arg->end_N1_idx, t_arg->K1, t_arg->M0, t_arg->N0, t_arg->K0, t_arg->total_rhs_panels_to_prefetch);

    aimm_tcm_free(tcm_base); // Free the shared TCM memory
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

    // --- Data Allocation and Packing ---
    int8_t** rhs_t = (int8_t**)malloc(N * sizeof(int8_t*));
    for (size_t i = 0; i < N; i++) {
        rhs_t[i] = (int8_t*)malloc(K * sizeof(int8_t));
    }

    // LHS is in standard malloc (DRAM/Cache)
    int8_t* lhs_packed = (int8_t*)malloc(M * K * sizeof(int8_t));
    // RHS is in DMA-accessible DRAM
    int8_t* rhs_t_packed = (int8_t*)aimm_dram_malloc(N * K * sizeof(int8_t));
    // Result is in standard malloc
    int* res_packed = (int*)malloc(M * N * sizeof(int));

    // Calculate TCM/Prefetch constants
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
        // args[i].rhs_t_packed_tcm = rhs_t_packed_tcm;
        args[i].start_N1_idx = current_N1_start;
        args[i].end_N1_idx = current_N1_start + N1_slice;
        args[i].total_rhs_panels_to_prefetch = total_rhs_panels_to_prefetch;

        current_N1_start = args[i].end_N1_idx;

        if (pthread_create(&threads[i], NULL, mmt4d_tcm_worker_thread, &args[i]) != 0) {
            perror("pthread_create failed");
            free(lhs_packed);
            aimm_dram_free(rhs_t_packed);
            free(res_packed);
            free2D(rhs_t, N);
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
    aimm_dram_free(rhs_t_packed);
    free(res_packed);
    free2D(rhs_t, N);
    aimm_deinit();
}

typedef struct {
    int thread_id;          // 0, 1, 2, 3
    size_t M1, N1, K1;      // Overall tiled dimensions
    size_t M0, N0, K0;      // Tile sizes
    int8_t* lhs_packed;     // Full LHS matrix
    int8_t* rhs_t_packed;   // Full RHS_T matrix
    int* res_packed;        // Full result matrix (packed)
    size_t start_N1_idx;    // Start index of N1 to process (thread's slice)
    size_t end_N1_idx;      // End index (exclusive) of N1 to process
} mmt4d_thread_arg_t;

// The thread function that runs the mmt4d kernel on a slice of N1
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

// New function to run MMT4D in parallel
void test_mmt4d_parallel(int** result, int8_t** lhs, int8_t** rhs, size_t M, size_t N, size_t K, size_t M0, size_t N0, size_t K0) {
    size_t M1 = M / M0;
    size_t N1 = N / N0;
    size_t K1 = K / K0;

    // --- Data Preparation (Same as original test_mmt4d) ---
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
            // ... (Handle cleanup if needed) ...
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
    free2D(rhs_t, N);
    free(lhs_packed);
    free(rhs_t_packed);
    free(res_packed);
}

void test_mmt4d(int** result, int8_t** lhs, int8_t** rhs, size_t M, size_t N, size_t K, size_t M0, size_t N0, size_t K0) {
    size_t M1 = M / M0;
    size_t N1 = N / N0;
    size_t K1 = K / K0;

    int8_t** rhs_t = (int8_t**)malloc(N * sizeof(int8_t*));
    for (int i = 0; i < N; i++) {
        rhs_t[i] = (int8_t*)malloc(K * sizeof(int8_t));
    }

    int8_t* lhs_packed = (int8_t*)malloc(M * K * sizeof(int8_t));
    int8_t* rhs_t_packed = (int8_t*)malloc(N * K * sizeof(int8_t));
    int* res_packed = (int*)malloc(M * N * sizeof(int));

    struct timespec start, end;
    pack(lhs, lhs_packed, M, K, M0, K0);
    transpose(rhs, rhs_t, K, N);
    pack(rhs_t, rhs_t_packed, N, K, N0, K0);

    flush_cache();
    clock_gettime(CLOCK_MONOTONIC, &start);
    mmt4d_s8s8s32(lhs_packed, rhs_t_packed, res_packed, M1, N1, 0, N1, K1, M0, N0, K0);
    clock_gettime(CLOCK_MONOTONIC, &end);

    unpack(res_packed, result, M, N, M0, N0);
    double mmt4d_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Time for mmt4d: %f seconds\n", mmt4d_time);

    free2D(rhs_t, N);
    free(lhs_packed);
    free(rhs_t_packed);
    free(res_packed);
}


int main(int agrc, char* argv[]) {
    // -----------------------------------------------------------------
    // STEP 1: Bind the MAIN process (and implicitly the main thread) to Core 4
    // -----------------------------------------------------------------
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(4, &mask); // MAIN THREAD bound to Core 4

    pid_t pid = getpid();
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_setaffinity failed on Core 4");
        return 1;
    }
    printf("Main thread (setup/cleanup) bound to Core 4.\n");

    // ... (Argument parsing and memory allocation/initialization remain the same) ...
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

    intialize_matrix(lhs, M, K, 1);
    intialize_matrix(rhs, K, N, 1);
    intialize_to_zero(res_mmt4d, M, N);
    intialize_to_zero(res_mmt4d_tcm, M, N);

    // test_matmul(res_matmul, lhs, rhs, M, N, K);
    test_mmt4d_parallel(res_mmt4d, lhs, rhs, M, N, K, M0, N0, K0); // <-- New parallel call
    test_mmt4d_tcm_parallel(res_mmt4d_tcm, lhs, rhs, M, N, K, M0, N0, K0);

    compare(res_mmt4d_tcm, res_mmt4d, M, N);

    printf("All results match!\n");

    // ... (Free memory remains the same) ...
    free2D(lhs, M);
    free2D(rhs, K);
    free2D_s32(res_mmt4d_tcm, M);
    free2D_s32(res_mmt4d, M);

    return 0;
}
