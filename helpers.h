#ifndef __HELPERS_H__
#define __HELPERS_H__

void intialize_matrix(int** matrix, size_t shape0, size_t shape1, int value) {
    for (int i = 0; i < shape0; i++) {
        for (int j = 0; j < shape1; j++) {
            matrix[i][j] = value++;
        }
    }
}

void intialize_to_zero(int** matrix, size_t shape0, size_t shape1) {
    for (int i = 0; i < shape0; i++) {
        for (int j = 0; j < shape1; j++) {
            matrix[i][j] = 0;
        }
    }
}

void pack(int** matrix, int* array, size_t shape0, size_t shape1, size_t inner_parallel, size_t inner_reduction) {
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

void transpose(int** matrix, int** tranposed, size_t shape0, size_t shape1) {
    for (int i = 0; i < shape0; i++) {
        for (int j = 0; j < shape1; j++) {
            tranposed[j][i] = matrix[i][j];
        }
    }
}

void unpack(int *array, int** matrix, size_t shape0, size_t shape1, size_t inner_parallel, size_t inner_reduction) {
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

void compare(int **matrix1, int **matrix2, size_t shape0, size_t shape1) {
    for (int i = 0; i < shape0; ++i) {
        for (int j = 0; j < shape1; ++j) {
            if (matrix1[i][j] != matrix2[i][j]) {
                printf("Error at [%d], [%d] ---> %d, %d\n", i, j, matrix1[i][j], matrix2[i][j]);
                return;
            }
        }
    }
}

void free2D(int** matrix, size_t shape0) {
    for (int i = 0; i < shape0; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

#endif
