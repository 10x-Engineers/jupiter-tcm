#ifndef __HELPERS_H__
#define __HELPERS_H__

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

void intialize_matrix(int8_t** matrix, size_t shape0, size_t shape1, int8_t value) {
    for (size_t i = 0; i < shape0; i++) {
        for (size_t j = 0; j < shape1; j++) {
            matrix[i][j] = value++;
        }
    }
}

void intialize_to_zero(int** matrix, size_t shape0, size_t shape1) {
    for (size_t i = 0; i < shape0; i++) {
        for (size_t j = 0; j < shape1; j++) {
            matrix[i][j] = 0;
        }
    }
}

void pack(int8_t** matrix, int8_t* array, size_t shape0, size_t shape1, size_t inner_parallel, size_t inner_reduction) {
    size_t index = 0;
    for (size_t i = 0; i < shape0; i+=inner_parallel) {
        for (size_t k = 0; k < shape1; k+=inner_reduction) {
            for (size_t i0 = 0; i0 < inner_parallel; ++i0) {
                for (size_t k0 = 0; k0 < inner_reduction; ++k0) {
                    array[index++] = matrix[i + i0][k + k0];
                }
            }
        }
    }
}

void transpose(int8_t** matrix, int8_t** tranposed, size_t shape0, size_t shape1) {
    for (size_t i = 0; i < shape0; i++) {
        for (size_t j = 0; j < shape1; j++) {
            tranposed[j][i] = matrix[i][j];
        }
    }
}

void unpack(int* array, int** matrix, size_t shape0, size_t shape1, size_t inner_parallel, size_t inner_reduction) {
    size_t index = 0;
    for (size_t i = 0; i < shape0; i+=inner_parallel) {
        for (size_t k = 0; k < shape1; k+=inner_reduction) {
            for (size_t i0 = 0; i0 < inner_parallel; ++i0) {
                for (size_t k0 = 0; k0 < inner_reduction; ++k0) {
                    matrix[i + i0][k + k0] = array[index++];
                }
            }
        }
    }
}

void compare(int **matrix1, int **matrix2, size_t shape0, size_t shape1) {
    for (size_t i = 0; i < shape0; ++i) {
        for (size_t j = 0; j < shape1; ++j) {
            if (matrix1[i][j] != matrix2[i][j]) {
                printf("Error at [%zu], [%zu] ---> %d, %d\n",
                       i, j, (int)matrix1[i][j], (int)matrix2[i][j]);
                return;
            }
        }
    }
}

void free2D(int8_t** matrix, size_t shape0) {
    for (size_t i = 0; i < shape0; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

void free2D_s32(int** matrix, size_t shape0) {
    for (size_t i = 0; i < shape0; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

#endif
