/*
 * author: Jan Wrona
 * email: <xwrona00@stud.fit.vutbr.cz>
 */

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <bitset>
#include <cstring>
#include <cerrno>
//#include <ctime>

#include <mpi.h>

#include "mm.h"

#define TAG 0
#define ROOT_PROC 0
#define MULTIPLICAND_FILE_NAME "mat1"
#define MULTIPLIER_FILE_NAME "mat2"

typedef int src_t;
#define MPI_SRC_T MPI::INT
typedef long int res_t;
#define MPI_RES_T MPI::LONG

#define DEBUG_PRINT(x) \
    std::cout << "WORLD: " << world_rank << "/" << world_procs << '\t' \
	<< "COL: " <<col_comm.Get_rank() << "/" << col_comm.Get_size()  << '\t' \
	<< "ROW: " << row_comm.Get_rank() << "/" << row_comm.Get_size()  << '\t' \
        << x << std::endl;

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

	multiplicand.print();
	std::cout << std::endl;
	multiplier.print();
	std::cout << std::endl;
	auto product = multiplicand * multiplier;
	product.print();
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

    /* Assign roles to processors. */
    std::bitset<PROC_ROLES_COUNT> proc_roles;
    proc_roles.set(FIRST_ROW, col_rank == 0);
    proc_roles.set(FIRST_COL, row_rank == 0);
    proc_roles.set(LAST_ROW, col_rank == col_procs - 1);
    proc_roles.set(LAST_COL, row_rank == row_procs - 1);

    /* Distribute multiplicand rows among processors in the first column. */
    std::vector<src_t> multiplicand_rows;
    if (proc_roles[FIRST_COL]) {
	multiplicand_rows.reserve(shared_dim); //avoid future reallocations
	multiplicand_rows.resize(shared_dim); //set correct vector size

	col_comm.Scatter(multiplicand.get_data(), shared_dim, MPI_SRC_T,
		multiplicand_rows.data(), shared_dim, MPI_SRC_T, ROOT_PROC);
    }

    /* Distribute multiplier columns among processors in the first row. */
    std::vector<src_t> multiplier_cols;
    if (proc_roles[FIRST_ROW]) {
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

    res_t acc_res = 0;
    bool overflow_detected = false;
    for (std::size_t i = 0; i < shared_dim; ++i) {
	src_t left, up;
	res_t res;

	if (proc_roles[FIRST_COL]) {
	    //DEBUG_PRINT("FIRST COLUMN => reading form memory");
	    left = multiplicand_rows.back();
	    multiplicand_rows.pop_back();
	} else {
	    //DEBUG_PRINT("NOT FIRST COLUMN => receiving from column " << row_rank - 1);
	    row_comm.Recv(&left, 1, MPI_SRC_T, row_rank - 1, TAG);
	}
	if (proc_roles[FIRST_ROW]) {
	    //DEBUG_PRINT("FIRST ROW => reading form memory");
	    up = multiplier_cols.back();
	    multiplier_cols.pop_back();
	} else {
	    //DEBUG_PRINT("NOT FIRST ROW => receiving from row " << col_rank - 1);
	    col_comm.Recv(&up, 1, MPI_SRC_T, col_rank - 1, TAG);
	}
	if (!proc_roles[LAST_COL]) {
	    //DEBUG_PRINT("NOT LAST COLUMN => sending to column " << row_rank + 1);
	    row_comm.Send(&left, 1, MPI_SRC_T, row_rank + 1, TAG);
	}
	if (!proc_roles[LAST_ROW]) {
	    //DEBUG_PRINT("NOT LAST ROW => sending to row " << col_rank + 1);
	    col_comm.Send(&up, 1, MPI_SRC_T, col_rank + 1, TAG);
	}

	acc_res += res = left * static_cast<res_t>(up);
	if (!overflow_detected && left != 0 && res / left != up) {
	    std::cerr << "WARNING: possible integer overflow detected" << std::endl;
	    overflow_detected = true;
	}
    }

    Matrix<res_t> product(prod_rows, prod_cols, Matrix<res_t>::PRODUCT);
    product.stretch();

    MPI::COMM_WORLD.Gather(&acc_res, 1, MPI_RES_T, product.get_data(), 1, MPI_RES_T,
	    ROOT_PROC);

    if (world_rank == ROOT_PROC) {
	product.print();
    }

    MPI::Finalize();
    return EXIT_SUCCESS;
}
