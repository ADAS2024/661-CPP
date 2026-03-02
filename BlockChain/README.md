## To Build

```bash
# Direct compile
g++ -std=c++17 -O2 -o blockchain_test main.cpp

# Or with CMake
mkdir build && cd build
cmake .. && make
```
## Run the test suite in main.cpp

```bash
./blockchain_test
```

## Key Design Differences from an actual BlockChain

- Each block stores their own UTXO set for easy work calculation for getting the tip of a block chain for reorginzation. In real blockchains, only the tip's UTXO state to save memory space.
- In our blockchain this prevents needing to recalculate each of the UTXO states for the chain and comparing that to a fork. 
- This didn't pass all the tests in the gradescope for the assignment but i got 75/90.
