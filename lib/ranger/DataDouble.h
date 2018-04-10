/*-------------------------------------------------------------------------------
This file is part of ranger.

Copyright (c) [2014-2018] [Marvin N. Wright]

This software may be modified and distributed under the terms of the MIT license.

Please note that the C++ core of ranger is distributed under MIT license and the
R package "ranger" under GPL3 license.
#-------------------------------------------------------------------------------*/

#ifndef DATADOUBLE_H_
#define DATADOUBLE_H_

#include "globals.h"
#include "utility.h"
#include "Data.h"

namespace ranger {

class DataDouble: public Data {
public:
  DataDouble();
  DataDouble(double* data, std::vector<std::string> variable_names, size_t num_rows, size_t num_cols) :
      data(data) {
    this->variable_names = variable_names;
    this->num_rows = num_rows;
    this->num_cols = num_cols;
    this->num_cols_no_snp = num_cols;
  }

  DataDouble(const DataDouble&)            = delete;
  DataDouble& operator=(const DataDouble&) = delete;
  virtual ~DataDouble() override;

  double get(size_t row, size_t col) const override {
    // Use permuted data for corrected impurity importance
    if (col >= num_cols) {
      col = getUnpermutedVarID(col);
      row = getPermutedSampleID(row);
    }

    if (col < num_cols_no_snp) {
      return data[col * num_rows + row];
    } else {
      // Get data out of snp storage. -1 because of GenABEL coding.
      size_t idx = (col - num_cols_no_snp) * num_rows_rounded + row;
      double result = (((snp_data[idx / 4] & mask[idx % 4]) >> offset[idx % 4]) - 1);
      return result;
    }
  }

  void reserveMemory() override {
    data = new double[num_cols * num_rows];
  }

  void set(size_t col, size_t row, double value, bool& error) override {
    data[col * num_rows + row] = value;
  }

private:
  double* data;
};

} // namespace ranger

#endif /* DATADOUBLE_H_ */
