/*
 * author: Jan Wrona
 * email: <xwrona00@stud.fit.vutbr.cz>
 */

#ifndef MM_H
#define MM_H

#include <string> /* std::string */
#include <vector> /* std::vector */
#include <cstdlib> /* std::size_of */

class Matrix {
public:
    enum Type { MULTIPLICAND, MULTIPLIER, PRODUCT};

    /* Constructors. */
    Matrix(Type type): type(type) { ; };
    Matrix(std::size_t rows, std::size_t cols, Type type): rows(rows), cols(cols), type(type) { ; };

    /* Methods. */
    void load(std::string file_name);
    void print(void);

    /* Operators. */
    int operator()(std::size_t row, std::size_t col) { return data[cols * row + col]; };
    Matrix operator*(const Matrix& rhs);
    
    /* Getters, setters. */
    std::size_t const& get_rows() const { return rows; };
    std::size_t const& get_cols() const { return cols; };
    const int *get_data() const { return data.data(); };

private:
    std::size_t rows = 0, cols = 0;
    std::vector<int> data;
    const Type type;
};

#endif /* MM_H */
