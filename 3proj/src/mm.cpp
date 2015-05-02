/*
 * author: Jan Wrona
 * email: <xwrona00@stud.fit.vutbr.cz>
 */

#include <mpi.h>

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <bitset>
#include <chrono>

#include "mm.h"

#define TAG 0
#define ROOT_PROC 0
#define MULTIPLICAND_FILE_NAME "mat1"
#define MULTIPLIER_FILE_NAME "mat2"

//#define MEASURE_TIME

typedef int src_t;
#define MPI_SRC_T MPI::INT
typedef int long res_t;
#define MPI_RES_T MPI::LONG

/* Processor position enumeration. */
enum {
    FIRST_ROW,
    FIRST_COL,
    LAST_ROW,
    LAST_COL,
    PROC_ROLES_COUNT
};

int main(int argc, char *argv[])
{
    MPI::Init(argc, argv);
    const int world_procs = MPI::COMM_WORLD.Get_size();
    const int world_rank = MPI::COMM_WORLD.Get_rank();
    std::size_t shared_dim, prod_rows, prod_cols;

    /* Load both matrices by root processor. */
    Matrix<src_t> multiplicand(Matrix<src_t>::MULTIPLICAND);
    Matrix<src_t> multiplier(Matrix<src_t>::MULTIPLIER);
    if (world_rank == ROOT_PROC) {
	try {
	    multiplicand.load(MULTIPLICAND_FILE_NAME);
	    multiplier.load(MULTIPLIER_FILE_NAME);
	} catch (std::exception& e) {
	    std::cerr << e.what() << std::endl;
	    MPI::COMM_WORLD.Abort(EXIT_FAILURE);
	}

	prod_rows = multiplicand.get_rows();
	prod_cols = multiplier.get_cols();
	shared_dim = multiplicand.get_cols();
    }

    /* Distribute dimensions among all processors. */
    MPI::COMM_WORLD.Bcast(&prod_rows, 1, MPI::UNSIGNED_LONG, ROOT_PROC);
    MPI::COMM_WORLD.Bcast(&prod_cols, 1, MPI::UNSIGNED_LONG, ROOT_PROC);
    MPI::COMM_WORLD.Bcast(&shared_dim, 1, MPI::UNSIGNED_LONG, ROOT_PROC);

    /* Create intra row and intra column comunicators. */
    auto row_comm = MPI::COMM_WORLD.Split(world_rank / prod_cols, world_rank % prod_cols);
    const int row_procs = row_comm.Get_size();
    const int row_rank = row_comm.Get_rank();
    auto col_comm = MPI::COMM_WORLD.Split(world_rank % prod_cols, world_rank / prod_cols);
    const int col_procs = col_comm.Get_size();
    const int col_rank = col_comm.Get_rank();

    /* Assign positions to processors. */
    std::bitset<PROC_ROLES_COUNT> proc_pos;
    proc_pos.set(FIRST_ROW, col_rank == 0);
    proc_pos.set(FIRST_COL, row_rank == 0);
    proc_pos.set(LAST_ROW, col_rank == col_procs - 1);
    proc_pos.set(LAST_COL, row_rank == row_procs - 1);

    /* Distribute multiplicand rows among processors in the first column. */
    std::vector<src_t> multiplicand_rows;
    if (proc_pos[FIRST_COL]) {
	multiplicand_rows.reserve(shared_dim); //avoid future reallocations
	multiplicand_rows.resize(shared_dim); //set correct vector size

	col_comm.Scatter(multiplicand.get_data(), shared_dim, MPI_SRC_T,
		multiplicand_rows.data(), shared_dim, MPI_SRC_T, ROOT_PROC);
    }

    /* Distribute multiplier columns among processors in the first row. */
    std::vector<src_t> multiplier_cols;
    if (proc_pos[FIRST_ROW]) {
	multiplier_cols.reserve(shared_dim); //avoid future reallocations
	multiplier_cols.resize(shared_dim); //set correct vector size

	/* Create column data type. */
	auto mpi_column_t = MPI_SRC_T.Create_vector(shared_dim, 1, prod_cols);
	mpi_column_t.Commit();
	mpi_column_t = mpi_column_t.Create_resized(0, sizeof(src_t));
	mpi_column_t.Commit();

	row_comm.Scatter(multiplier.get_data(), 1, mpi_column_t,
		multiplier_cols.data(), shared_dim, MPI_SRC_T, ROOT_PROC);
    }

#ifdef MEASURE_TIME
    MPI::COMM_WORLD.Barrier();
    auto start = std::chrono::high_resolution_clock::now();
#endif /* MEASURE_TIME */

    res_t acc_res = 0;
    bool overflow_detected = false;

    /* For each element do actions based on processor position. */
    for (std::size_t i = 0; i < shared_dim; ++i) {
	src_t left, upper;
	res_t res;

	/* Processors in first column/row will read multiplicand/multiplier
	 * from memory, processors in other columns/rows will receive
	 * multiplicand/multiplier in message.
	 */
	if (proc_pos[FIRST_COL]) {
	    left = multiplicand_rows.back();
	    multiplicand_rows.pop_back();
	} else {
	    row_comm.Recv(&left, 1, MPI_SRC_T, row_rank - 1, TAG);
	}
	if (proc_pos[FIRST_ROW]) {
	    upper = multiplier_cols.back();
	    multiplier_cols.pop_back();
	} else {
	    col_comm.Recv(&upper, 1, MPI_SRC_T, col_rank - 1, TAG);
	}

	/* Processors not in last column/row will send operands futher. */
	if (!proc_pos[LAST_COL]) {
	    row_comm.Send(&left, 1, MPI_SRC_T, row_rank + 1, TAG);
	}
	if (!proc_pos[LAST_ROW]) {
	    col_comm.Send(&upper, 1, MPI_SRC_T, col_rank + 1, TAG);
	}

	/* Multiplication and accumulation. */
	acc_res += res = left * static_cast<res_t>(upper);
	if (!overflow_detected && left != 0 && res / left != upper) {
	    std::cerr << "WARNING: possible integer overflow detected" << std::endl;
	    overflow_detected = true;
	}
    }

#ifdef MEASURE_TIME
    MPI::COMM_WORLD.Barrier();
    auto end = std::chrono::high_resolution_clock::now();
    if (world_rank == ROOT_PROC) {
       std::chrono::duration<double> diff = end - start;
       std::cout << diff.count() << std::endl;
    }
#else
    Matrix<res_t> product(prod_rows, prod_cols, Matrix<res_t>::PRODUCT);
    product.stretch();

    /* Gather data from all processors into root processor. */
    MPI::COMM_WORLD.Gather(&acc_res, 1, MPI_RES_T, product.get_data(), 1, MPI_RES_T,
	    ROOT_PROC);

    if (world_rank == ROOT_PROC) {
	product.print();
    }
#endif /* MEASURE_TIME */

    MPI::Finalize();
    return EXIT_SUCCESS;
}
