#include "doublespend.h"
#include <iostream>       
#include <cmath>    

double factorial (const int k) {
  double f = 1.0;
  for (int i = 1; i <= k; ++i) {
    f *= i;
  }
  return f;
}

/*
 * q = attacker hash power
 * z = number of blocks for embargo period
 */

double satoshi(double q, int z) {
  double p = 1.0 - q;
  double satoshi_sum = 0.0;

  for (int k = 0; k < z + 1; ++k) {
    double crowbar = (z * q) / p;

    // Calculate for Poisson Val
    double numerator = std::pow(crowbar, k) * std::exp(-crowbar);
    double denominator = factorial(k);

    // Parantheses values in double spend equation
    double qip = q / p;
    int zk = z + 1 - k;

    double poisson_term = numerator / denominator;
    double parentheses_term = 1.0 - std::pow(qip, zk);
    double summation_val = poisson_term * parantheses_term;
    satoshi_sum += summation_val;
  }

  return 1.0 - satoshi_sum;
}
