/*
 * author: Jan Wrona
 * email: <xwrona00@stud.fit.vutbr.cz>
 */

#ifndef MM_H
#define MM_H

#include <iostream>
#include <string> /* std::string */
#include <vector> /* std::vector */
#include <cstdlib> /* std::size_of */
#include <cstring>
#include <cerrno>

template <typename T>
class Matrix {
public:
    enum Type { MULTIPLICAND, MULTIPLIER, PRODUCT };

    /* Constructors. */
    Matrix(Type type): type(type) { ; };
    Matrix(std::size_t rows, std::size_t cols, Type type);

    /* Methods. */
    void load(std::string file_name);
    void stretch(void) { data.resize(rows * cols); };
    void print(void) const;

    /* Operators. */
    Matrix operator*(const Matrix& rhs) const;
    
    /* Getters, setters. */
    std::size_t const& get_rows() const { return rows; };
    std::size_t const& get_cols() const { return cols; };
    T* get_data() { return data.data(); };
    const T* get_data() const { return data.data(); };

private:
    std::size_t rows = 0, cols = 0;
    std::vector<T> data;
    const Type type;
};

template <typename T>
Matrix<T>::Matrix(std::size_t rows, std::size_t cols, Type type): rows(rows),
	cols(cols), type(type)
{
    data.reserve(rows * cols); //avoid future reallocations
}

template <typename T>
void Matrix<T>::load(std::string file_name)
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
	for (T number; line_ss >> number; ) {
	    data.push_back(number);
	    read_cols++;
	}
	if (rows == 1) {
	    cols = read_cols;
	}

	if (!line_ss.eof() && line_ss.fail()) {
	    throw std::invalid_argument("Invalid value on row " +
		    std::to_string(static_cast<unsigned long long>(rows)) + " of "+ file_name);
	}
	if (read_cols == 0 || read_cols != cols) {
	    throw std::invalid_argument("Invalid column count on row " +
		    std::to_string(static_cast<unsigned long long>(rows)) + " of "+ file_name);
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

template <typename T>
Matrix<T> Matrix<T>::operator*(const Matrix &rhs) const
{
    Matrix res(rows, rhs.cols, Matrix::PRODUCT);

    if (cols != rhs.rows) {
	exit(EXIT_FAILURE);
	return res;
    }

    for (std::size_t i = 0; i < rows; ++i) {
	for (std::size_t j = 0; j < rhs.cols; ++j) {
	    T product = 0;

	    for (std::size_t k = 0; k < cols; ++k) {
		product += data[i * cols + k] * rhs.data[k * rhs.cols + j];
	    }
	    res.data.push_back(product);
	}
    }

    return res;
}

template <typename T>
void Matrix<T>::print(void) const
{
    switch (type) {
	case MULTIPLICAND:
	    std::cout << rows << std::endl;
	    break;
	case MULTIPLIER:
	    std::cout << cols << std::endl;
	    break;
	case PRODUCT:
	    std::cout << rows << ':' << cols << std::endl;
	    break;
    }

    if (data.empty()) {
	return;
    }

    for (std::size_t i = 0; i < rows; ++i) {
	std::cout << data[i * cols];
	for (std::size_t j = 1; j < cols; ++j) {
	    std::cout << ' ' << data[i * cols + j];
	}
	std::cout << std::endl;
    }
}

#endif /* MM_H */
