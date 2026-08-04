#include "matrix.hpp"
#include <stdexcept>
#include <iostream>

using namespace std;


void showMatrix(const matrix &D) {
    // Prints out a given matrix 

    cout << "Matrix (" << D.rows << "x" << D.cols << "):" << endl;
    for (int i = 0; i < D.rows; i++) {
        for (int j = 0; j < D.cols; j++) {
            cout << D.x[i * D.cols + j] << " ";
        }
        cout << endl;
    }
}

void showVector(const vector<double> &v) {
    // prints out the given vector

    cout << "Vector: " << endl;

    for (unsigned int i = 0; i < v.size(); i++) {
        cout << v[i] << endl;
    }
}

matrix transpose(const matrix &A) {
    // returns the transpose of A
    
    matrix B(A.rows, A.cols);

    for (int i = 0; i < B.rows; i++) {
        for (int j = 0; j < B.cols; j++) {
            B.x[i*B.cols+j] = A.x[j*A.cols+i];
        }
    }

    return B;
}

matrix multiply(const matrix &A, const matrix &B) {
    // matrix multiplication operation on matrices A (nxm) and B (mxp)
    // returns D = AB (nxp)

    if (A.cols != B.rows) {
        throw std::invalid_argument("Invalid dimensions");
    }

    matrix D(A.rows, B.cols);

    for (int i = 0; i < A.rows; i++) {
        for (int j = 0; j < B.cols; j++) {
            double temp = 0;
            for (int k = 0; k < B.rows; k++) {
                temp += A.x[i*A.cols + k] * B.x[k*B.cols + j];
            }
            D.x[i*D.cols+j] = temp;
        }
    }
    return D;

}

matrix cholesky(const matrix &A) {
    // uses row-wise cholesky decomposition to split A (nxn) into L, Lt
    // returns L as the lower triangular matrix
    // 4 5
    // 5 9

    for (size_t i = 0; i < A.rows; i++) {
        if (A.x[i*A.cols+i] < 0) {
            throw std::invalid_argument("Covariance matrix is not all non-negative");
        }
    }

    matrix L(A.rows, A.cols);

    for (int i = 0; i < A.rows; i++) {
        for (int j = 0; j <= i; j++) {
            // L_ii = sqrt(A_ii - sum)
            double sum = 0.0;

            if (j == i) {
                // diagonal case
                // accumulate sum of squares
                for (int k = 0; k < i; k++) {
                    sum += L.x[i*L.cols+k] * L.x[i*L.cols+k];
                }

                double diff = A.x[i*A.cols+i] - sum;

                if (diff < -1e-12) {
                    throw std::runtime_error("Matrix is not positive semi-definite (significant negative diagonal).");
                }
                L.x[i*L.cols+i] = sqrt(std::max(0.0, A.x[i*A.cols+i] - sum));
            } else {
                // below diagonal case

                // find temporary dot product overlap
                double product = 0.0;
                for (int k = 0; k < j; k++) {
                    product += L.x[i*L.cols+k] * L.x[j*L.cols+k];
                }

                if (std::abs(L.x[j*L.cols +j]) < 1e-15) {
                    L.x[i*L.cols+j] = 0.0;
                } else{
                    L.x[i*L.cols+j] = (A.x[i*A.cols+j] - product) / (L.x[j*L.cols+j]);
                }
            }

        }

    }
    return L;
}

matrix test_cholesky(const matrix &L) {
    // tests the cholesky algorithm by recomposing L and Lt to check if they match A
    // L is lower triangular
    // Lt is L transpose

    matrix B(L.rows, L.cols);

    for (int i = 0; i < L.rows; i++) {
        for (int j = 0; j < L.rows; j++) {
            double temp = 0;
            for (int k = 0; k < L.cols; k++) {
                temp += L.x[i*L.cols+k] * L.x[j*L.cols+k];
            }
            B.x[i*B.cols+j] = temp;
        }
    }

    return B;
}

vector<double> forward_sub(const matrix &L, const vector<double> &b) {
    // solves for y in L y = b
    // L is lower triangular
    // b is a vector

    vector<double> y = b;

    for (int i = 0; i < L.rows - 1; i++) {
        for (int j = i + 1; j < L.rows; j++) {
            double ratio = L.x[j*L.cols+i] / L.x[i*L.cols+i];
            y[j] -= ratio * y[i];
        }
    }

    for (unsigned int i = 0; i < y.size(); i++) {
        y[i] /= L.x[i*L.cols+i];
    }

   return y;


}

vector<double> backward_sub(const matrix &L, const std::vector<double> &y) {
    // solves for x in L_t x = y
    // L_t is L transpose
    // L_t is upper diagonal
    // y is a vector

    vector<double> x = y;

    for (int i = L.rows - 1; i >= 1; i--) {
        for (int j = i - 1; j >= 0; j--) {
            double ratio = L.x[i*L.cols+j] / L.x[i*L.cols+i];
            x[j] -= ratio * x[i];

        }
    }

    for (unsigned int i = 0; i < x.size(); i++) {
        x[i] /= L.x[i*L.cols+i];
    }

    return x;
}