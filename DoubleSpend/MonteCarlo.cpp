#include "doublespend.h"
#include <random>        
#include <vector>        
#include <utility>       
#include <cstdlib>       


constexpr int MAX_LEAD = 35;

// Generate random values in range for doublespend
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<double> dis(0.0, 1.0);



std::pair<int, int> embargo_period(double q, int z) {
  int honest_blocks = 0;
  int attacker_blocks = 0;

  while (1) {
    if (honest_blocks == z) {
      break;
    }

    double block_num = dis(gen);
    if (block_num <= q) {
      attacker_blocks+=1;
    } 
    else {
      honest_blocks+=1;
    }

  }

  if (attacker_blocks > honest_blocks) {
    return {1, 1};
  } else {
    return {honest_blocks, attacker_blocks};
  }

}

int monteCarloSim(double q, int z) {
  std::pair<int, int> embargo_check = embargo_period(q, z);

  if (embargo_check.first == 1 && embargo_check.second == 1) {
    return 1;
  } else {
    int honest_blocks = embargo_check.first;
    int attacker_blocks = embargo_check.second;

    while (1) {
      if (attacker_blocks >= honest_blocks) {
        return 1;
      } else if (honest_blocks - attacker_blocks >= MAX_LEAD){
        return 0;
      }


      double block_num = dis(gen);
      if (block_num <= q) {
        attacker_blocks+=1;
      } 
      else {
        honest_blocks+=1;
      }
    }
  }
}

double monteCarlo(double q, int z, int numTrials = 50000) {
  double numAttackerSuccess = 0;

  for (int i = 0; i < numTrials; ++i) {
    double val = monteCarloSim(q, z);
    numAttackerSuccess += val;
  }

  return numAttackerSuccess / numTrials;

}
