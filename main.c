#include <stdlib.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>

struct matrix {
	int **matrix;
	int rows;
	int cols;
};
typedef struct matrix * Matrix;
// initalizes a matrix struct to contain all zero's
Matrix init_matrix(int rows, int cols) {
	Matrix C = malloc(sizeof(struct matrix));
	C->matrix = malloc(sizeof(int *) * rows);
	for(int row = 0; row < rows; row++) {
		C->matrix[row] = malloc(sizeof(int) * cols);
		for(int col = 0; col < cols; col++) {
			C->matrix[row][col] = 0;
		}
	}
	return C;
}
// initializes a matrix with the contents of a two dimensional array.
void fill_matrix(Matrix C, int **a, int rows, int cols) {
	for(int row = 0; row < rows; row++) {
		for(int col = 0; col < cols; col++) {
			C->matrix[row][col] = a[row][col];
		}
	}
}
// multiply two matrices of arbitrary size, and return a new matrix result. cannot multiply matrices where the number of columns of the first operand does not equal the number of rows in the second operand. 
Matrix multiply(Matrix A, int rowsA, int colsA, Matrix B, int rowsB, int colsB) {
	if(colsA != rowsB) {
		printf("Cannot multiply\nA is %d x %d and B is %d x %d\n", rowsA, colsA, rowsB, colsB);
		return NULL;
	}
	Matrix C = init_matrix(rowsA, colsA);
	for(int row = 0; row < rowsA; row++) {
		int total = 0;
		for(int col = 0; col < colsB; col++) {
			for(int oth = 0; oth < colsA; oth++) {
				int a = A->matrix[row][oth];
				int b = B->matrix[oth][col];
				total += a * b;
			}
			C->matrix[row][col] = total;
			total = 0;
		}
	}
	return C;
}
// prints the contents of a matrix. uncomment the print line to get its dimensions.
void print_matrix(Matrix m, int rows, int cols) {
	if(m == NULL) return;
	//printf("Rows: %d Cols: %d\n", rows, cols);
	for(int row = 0; row < rows; row++) {	
		for(int col = 0; col < cols; col++) {
			printf("%d ", m->matrix[row][col]);
		}
		printf("\n");
	}	
	printf("\n");
}
// returns a requested row for a given matrix (must be in bounds).
int * get_row(Matrix M, int row, int rows, int cols) {
	if(row >= rows) {
		printf("Out of bounds\n");
		return NULL;
	}	
	int *temp = malloc(sizeof(int) * cols);
	for(int i = 0; i < cols; i++) {
		temp[i] = M->matrix[row][i];
	}

	return temp;
}
// returns a requested col for a given matrix (must be in bounds).
int * get_col(Matrix M, int col, int rows, int cols) {
	if(col >= cols) {
		printf("Out of bounds\n");
		return NULL;
	}	
	int *temp = malloc(sizeof(int) * rows);
	for(int i = 0; i < rows; i++) {
		temp[i] = M->matrix[i][col];
	}
	return temp;
}
// reads a matrix from a file and returns it
Matrix read(char *name) {
	FILE *file = fopen ( name, "r" );
	Matrix A = NULL;
	if ( file != NULL ) {
		char line [ 256 ];
		int row = -1;
		int col = 0;
		int rows, cols;
		while ( fgets ( line, sizeof line, file ) != NULL ) { 
			if(row == -1) {
				char *tok= strtok(line, " ");
				rows = atoi(tok);
				tok = strtok(NULL, " ");
				cols = atoi(tok);
				A = init_matrix(rows, cols);
				A->rows = rows;
				A->cols = cols;
				row++;
				continue;
			}
			char *tok= strtok(line, " ");
			while (tok && col < cols && row < rows) {
				A->matrix[row][col] = atoi(tok); 
				tok = strtok(NULL, " ");
				col++;
			}
			row++;
			col = 0;
		}
		fclose ( file );
	}
	else {
		perror ( name); 
	}
	return A;
}
// sets a given row in a matrix, given it is in bounds.
void set_row(Matrix A, int *temp, int row, int rows, int cols) {
	if(row >= rows) {
		printf("Out of bounds\n");
		return;
	}	
	for(int i = 0; i < cols; i++) {
		A->matrix[row][i] = temp[i];
	}
}
int main(int argc, char **argv) {
	MPI_Init(&argc, &argv);
	int num_proc, rank, name_length;
	char process_name[MPI_MAX_PROCESSOR_NAME];
	MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Status status;
	
	int rowsA, colsA, rowsB, colsB, rowsC, colsC;
	int actualRows;

	// if there's only one machine, no sense in over complicating things -- just calculate using the one machine. (plus it's extremely readable!)
	if(num_proc == 1) {
		Matrix A = read("A.txt");
		rowsA = A->rows;
		colsA = A->cols;
		Matrix B = read("B.txt");
		rowsB = B->rows;
		colsB = B->cols;
		Matrix C = multiply(A, rowsA, colsA, B, rowsB, colsB);
		printf("A = \n");
		print_matrix(A, rowsA, colsA);
		printf("B = \n");
		print_matrix(B, rowsB, colsB);
		printf("A * B = \n");
		print_matrix(C, rowsA, colsB);
		MPI_Finalize();
		return 0;
	}

	// read matrices from text file
	// only create A on root, and send each row to different machines
	if(rank == 0) {
		Matrix A = read("A.txt");
		rowsA = A->rows;
		colsA = A->cols;
		// "actualRows" represent the total number of rows in matrix A. I want to hold onto this because rowA is overwritten on the slave machines. Since the root is also doing some work, it is overwritten there too.
		// I hold onto actualRows on the root for when the calculations are joined into a single matrix later.
		actualRows = rowsA;
		for(int i = 0; i < num_proc; i++) {
			// send one individual row to each slave. 
			MPI_Send(get_row(A, i, rowsA, colsA), colsA, MPI_INT, i, 666, MPI_COMM_WORLD);
			MPI_Send(&colsA, 1, MPI_INT, i, 667, MPI_COMM_WORLD);
		}
		printf("A = \n");
		print_matrix(A, rowsA, colsA);
	}
	// every slave needs a copy of B
	Matrix B = read("B.txt");
	rowsB = B->rows;
	colsB = B->cols;
	// receive rows of A from root
	int *temp = malloc(sizeof(int) * rowsB);
	MPI_Recv(temp, rowsB, MPI_INT, 0, 666, MPI_COMM_WORLD, &status);
	MPI_Recv(&colsA, 1, MPI_INT, 0, 667, MPI_COMM_WORLD, &status);
	rowsA = 1;
	Matrix A = init_matrix(rowsA, colsA);
	set_row(A, temp, 0, rowsA, colsA);

	rowsC = rowsA;
	colsC = colsB;
	// slave calculates one individual row, and sends it back to the root.
	Matrix C = multiply(A, rowsA, colsA, B, rowsB, colsB);	
	MPI_Send(get_row(C, 0, rowsC, colsC), colsC, MPI_INT, 0, 668, MPI_COMM_WORLD);

	if(rank == 0) {
		printf("B = \n");
		print_matrix(B, rowsB, colsB);
		Matrix result = init_matrix(actualRows, colsB);
		// accept from slaves
		for(int i = 0; i < num_proc; i++) {
			int *temp = malloc(sizeof(int) * colsB);
			MPI_Recv(temp, colsB, MPI_INT, i, 668, MPI_COMM_WORLD, &status);
			set_row(result, temp, i, actualRows, colsB);
		}
		printf("A * B = \n");
		print_matrix(result, actualRows, colsB);
	}
	
// manual testing
/*
	rowsA = 3;
	colsA = 3;
	Matrix A = init_matrix(rowsA, colsA);
	fill_matrix(A, (int *[]){(int []){1,2,3}, (int []){4,5,6}, (int []){7,8,9}}, rowsA, colsA);
	print_matrix(A, rowsA, colsA);

	rowsB = 3;
	colsB = 4;
	Matrix B = init_matrix(rowsB, colsB);
	fill_matrix(B, (int *[]){(int []){1,2,3,4}, (int []){5,6,7,8}, (int []){9,10,11,12}}, rowsB, colsB);
	print_matrix(B, rowsB, colsB);

	//  rowsA x colsA  |  rowsB x colsB
	rowsC = rowsA;
	colsC = colsB;
	Matrix C = multiply(A, rowsA, colsA, B, rowsB, colsB);	
	print_matrix(C, rowsC, colsC);
*/
	MPI_Finalize();
	return 0;
}
