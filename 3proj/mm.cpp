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


void Matrix::load(std::string file_name)
{
    std::size_t read_dim;

    /* Open file and check for errors. */
    std::ifstream is(file_name);
    if (!is) {
        throw std::invalid_argument(file_name + ": " + std::strerror(errno));
    }

    std::string line;
    std::getline(is, line);
    read_dim = std::stoul(line);

    while (std::getline(is, line)) {
	std::stringstream line_ss(line);
	std::size_t read_cols = 0;

	rows++;
	for (int number; line_ss >> number; ) {
	    data.push_back(number);
	    read_cols++;
	}
	if (rows == 1) {
	    cols = read_cols;
	}

	if (!line_ss.eof() && line_ss.fail()) {
	    throw std::invalid_argument("Invalid value on row " +
		    std::to_string(rows) + " of "+ file_name);
	}
	if (read_cols == 0 || read_cols != cols) {
	    throw std::invalid_argument("Invalid column count on row " +
		    std::to_string(rows) + " of "+ file_name);
	}
    }

    if (!is.eof()) {
        throw std::invalid_argument(file_name + ": " + std::strerror(errno));
    }
    is.close();

    switch (type) {
	case MULTIPLICAND:
	    if (read_dim != rows) {
		throw std::domain_error(file_name + ": specified and read rows count mismatch");
	    }
	    break;
	case MULTIPLIER:
	    if (read_dim != cols) {
		throw std::domain_error(file_name + ": specified and read columns count mismatch");
	    }
	    break;
	default:
	    throw std::invalid_argument("Invalid matrix type");
	    break;
    }
}

Matrix Matrix::operator*(const Matrix &rhs)
{
    Matrix res(rows, rhs.cols, Matrix::PRODUCT);

    if (cols != rhs.rows) {
	return res;
    }

    for (std::size_t i = 0; i < rows; ++i) {
	for (std::size_t j = 0; j < rhs.cols; ++j) {
	    int product = 0;
	    for (std::size_t k = 0; k < cols; ++k) {
		product += data[i * cols + k] * rhs.data[k * rhs.cols + j];
	    }
	    res.data.push_back(product);
	}
    }

    return res;
}

void Matrix::print(void)
{
    for (std::size_t i = 0; i < rows; ++i) {
	for (std::size_t j = 0; j < cols; ++j) {
	    std::cout << data[i * cols + j] << ' ';
	}
	std::cout << std::endl;
    }
}

enum {
    FIRST_COL,
    FIRST_ROW,
    LAST_COL,
    LAST_ROW,
    PROC_ROLES_COUNT
};

int main(int argc, char *argv[])
{
    MPI::Init(argc, argv);
    const int num_procs = MPI::COMM_WORLD.Get_size();
    const int proc_rank = MPI::COMM_WORLD.Get_rank();
    std::size_t shared_dim, prod_rows, prod_cols;
    std::bitset<PROC_ROLES_COUNT> proc_roles;
    std::vector<int> multiplicand_rows, multiplier_cols;

    Matrix multiplicand(Matrix::MULTIPLICAND);
    Matrix multiplier(Matrix::MULTIPLIER);
    if (proc_rank == ROOT_PROC) {
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

    /* Assign roles to processors. */
    if (proc_rank % prod_cols == 0) {
	proc_roles.set(FIRST_COL);
    }
    if (proc_rank / prod_cols == 0) {
	proc_roles.set(FIRST_ROW);
    }
    if (proc_rank % prod_cols == prod_cols - 1) {
	proc_roles.set(LAST_COL);
    }
    if (proc_rank / prod_cols == prod_rows - 1) {
	proc_roles.set(LAST_ROW);
    }

    /* Create intra row and intra column comunicators. */
    auto row_comm = MPI::COMM_WORLD.Split(proc_rank / prod_cols, proc_rank % prod_cols);
    auto col_comm = MPI::COMM_WORLD.Split(proc_rank % prod_cols, proc_rank / prod_cols);

    /* Distribute multiplicand rows among processors in the first column. */
    if (proc_roles[FIRST_COL]) {
	multiplicand_rows.reserve(shared_dim); //avoid future reallocations
	multiplicand_rows.resize(shared_dim); //set correct vector size

	col_comm.Scatter(multiplicand.get_data(), shared_dim, MPI::INT,
		multiplicand_rows.data(), shared_dim, MPI::INT, ROOT_PROC);
    }

    /* Distribute multiplier columns among processors in the first row. */
    if (proc_roles[FIRST_ROW]) {
	multiplier_cols.reserve(shared_dim); //avoid future reallocations
	multiplier_cols.resize(shared_dim); //set correct vector size

	/* Create column data type. */
	auto mpi_column_t = MPI::INT.Create_vector(shared_dim, 1, prod_cols);
	mpi_column_t.Commit();
	mpi_column_t = mpi_column_t.Create_resized(0, sizeof(int));
	mpi_column_t.Commit();

	row_comm.Scatter(multiplier.get_data(), 1, mpi_column_t,
		multiplier_cols.data(), shared_dim, MPI::INT, ROOT_PROC);
    }
    //multiplier_cols.reserve(shared_dim);
    //row_comm.Scatter(multiplicand.get_data(), shared_dim, MPI::INT,
    //	multiplicand_rows.data(), prod_cols, MPI::INT, proc_rank);

    //auto product = multiplicand * multiplier;
    //multiplicand.print();
    //std::cout << std::endl;
    //multiplier.print();
    //std::cout << std::endl;
    //product.print();


//    std::cout << "WORLD: " << proc_rank << "/" << num_procs << '\t'
//	<< "COL: " <<col_comm.Get_rank() << "/" << col_comm.Get_size()  << '\t'
//	<< "ROW: " << row_comm.Get_rank() << "/" << row_comm.Get_size()  << std::endl;
//
    //for (std::size_t proc = 0; proc < col_comm.Get_size(); ++proc) {
    //    if (col_comm.Get_rank() == proc) {
    //        std::cout << "ROW: " << col_comm.Get_rank() << "/" << col_comm.Get_size() << std::endl;
    //        for (auto n: multiplicand_rows) {
    //    	std::cout << n << " ";
    //        }
    //        std::cout << std::endl;
    //    }
    //    col_comm.Barrier();
    //}

    MPI::Finalize();
    return EXIT_SUCCESS;
}
