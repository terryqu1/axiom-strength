#pragma once
#include <vector>

struct matrix {
    std::vector<double> x;
    int rows;
    int cols;
    double& operator()(int i, int j) {
        return x[i*cols+j];
    }
    double* get_row(int i) {
        return &x[i*cols];
    }
    matrix(int rows, int cols) : x(rows * cols,0), rows(rows), cols(cols) {
    }
};


matrix transpose(const matrix &A);
// returns the transpose of A
matrix multiply(const matrix &A, const matrix &B);
// multiplies matrices A and B
matrix cholesky(const matrix &A);
// performs cholesky decomposition on A
matrix test_cholesky(const matrix &L);
// multiplies L and Lt
std::vector<double> forward_sub(const matrix &L, const std::vector<double> &b);
// solves for y in L y = b
std::vector<double> backward_sub(const matrix &L, const std::vector<double> &y);
// solves for x in L_t x = y
void showMatrix(const matrix &D);
void showVector(const std::vector<double> &v);