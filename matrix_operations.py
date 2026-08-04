# matrices operations

def show(matrix_A):
    """
    Prints out matrix A
    """
    for row in matrix_A:
        print(row)

def add(matrix_A, matrix_B):
    """
    Adds two matrices A (n*m) and B (n*m)
    """
    n = len(matrix_A) # n rows
    m = len(matrix_A[0]) # m cols

    matrix = [[0 for _ in range(m)] for _ in range(n)]

    for i in range(n):
        for j in range(m):
            matrix[i][j] = matrix_A[i][j] + matrix_B[i][j]

    return matrix

def dotProduct(vector_A, vector_B):
    """
    Takes dot product of vectors A and B, both of length n
    """
    product = 0

    for i in range(len(vector_A)):
        product += vector_A[i] * vector_B[i]

    return product

def multiply(matrix_A, matrix_B):
    """
    Multiplies two matrices A (n*m) and B (m*p)
    """
    n = len(matrix_A)
    m = len(matrix_B)
    p = len(matrix_B[0])

    matrix = [[0 for _ in range(p)] for _ in range(n)]

    for row_A in range(n):
        for col_B in range(p):
            matrix_B_col = [matrix_B[k][col_B] for k in range(m)]
            prod = dotProduct(matrix_A[row_A], matrix_B_col)
            matrix[row_A][col_B] = prod

    show(matrix)

    return matrix

def forwardSubstitution(matrix_A, vector_B):
    """
    solves for Ly = b
    """

def backwardSubstitution(matrix_A, vector_B):
    """
    solves L_t * x = y
    """

def choleskyDecomp(matrix_A):
    """
    Performs the row-wise cholesky decomposition to split A into L and L_t
    """

def main():
    matrix_A = [[1,4],[3,-9]]
    matrix_B = [[5,3],[-2,1]]

    print("matrix A:")
    show(matrix_A)
    print("matrix B:")
    show(matrix_B)
    print("product: ")
    multiply(matrix_A, matrix_B)

if __name__ == "__main__":
    main()