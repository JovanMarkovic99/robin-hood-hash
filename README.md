# jvn::unordered_map - Robin Hood Hashing Implementation in C++
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

`jvn::unordered_map` is a C++ library that provides a fast and efficient hash table implementation based on the Robin Hood Hashing algorithm. It is designed to be highly performant while minimizing memory usage, making it an ideal choice for applications that require fast lookups and insertions.
<br>
<div style="display:flex; justify-content:space-between;">
    <img src="/benchmark/results/figure_find.png" alt="Performance graph of insert" style="width:48%;">
    <img src="/benchmark/results/figure_insert.png" alt="Performance graph of find" style="width:48%;">
</div>

## Features

* Robin Hood Hashing algorithm for high-performance hash table operations.
* Flat memory layout that efficiently utilizes memory by packing structs containing a key-value pair and hash distance.
* Size of the hash table is a power of two for fast hash trimming.
* Optimized code that uses compiler-specific commands to denote unlikely branches.
* Fixed-size memory pool `jvn::AlternatingFixedMemoryAllocator` for quick allocation.
* Benchmarking suite for automated testing and visualization.

## Getting Started

### Prerequisites

* A C++ compiler that supports C++17 or later.
* Python 2.7+ with Matplotlib for automated benchmarking and data visualization
* No external libraries or dependencies are required.

### Installation

`jvn::unordered_map` is a header-only library, so there is no installation process required. Simply copy the header files to your project directory and include them in your source code.

### Usage

```cpp
#include "map.h"

jvn::unordered_map<int, std::string> map;

map.insert({1, "one"});
map.insert({2, "two"});
map.insert({3, "three"});

auto value = map.find(2);

if (value != map.end())
    std::cout << "Value found: " << value->second << std::endl;
else
    std::cout << "Value not found" << std::endl;
```
### Benchmarking

The `benchmark` directory contains benchmarking scripts that can be used to measure the performance of `jvn::unordered_map` or other `unordered_map`'s. To run the automated benchmarks, follow these steps:

1. Compile the `benchmark.cpp` file with your preferred C++ compiler and flags, such as: `g++ -O3 benchmark.cpp -o benchmark.`
2. Run the `auto_benchmark.py` script with the following command: `python auto_benchmark.py [--output-file OUTPUT_FILE]`. This script will run a series of benchmarks with varying map sizes and record the execution times in JSON format. The optional --output-file argument can be used to specify the output file path (default is `results/results.json`).
5. Run the `graph_results.py` script with the following command: `python graph_results.py [--input-files INPUT_FILES [INPUT FILES ...]]`. This script will read the JSON data files produced by the `auto_benchmark.py` script and plot the execution times using Matplotlib. The optional --input-files argument can be used to specify the input files you wish to graph (default is `results/results.json`).

##### Custom Benchmarks

To run custom benchmarks, you can modify the `benchmark.cpp` file to change the map or key/value types, or pass a different test suite as an argument with this command: `benchmark.exe INPUT_FILE OUTPUT_FILE.`<br>
After compiling, run the modified file using similar commands as above.<br> 
<br>
Note that the custom benchmarks may have specific requirements or restrictions, such as the need for a certain C++ version or a certain input type format. Please refer to the documentation for more information.

## Acknowledgements

* [Flat Hash Table](https://github.com/skarupke/flat_hash_map) by Malte Skarupke
* [Robin Hood Map](https://github.com/Tessil/robin-map "Robin-hood Map") by Tessil
