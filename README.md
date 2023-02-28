# jvn::unordered_map - Robin Hood Hashing Implementation in C++
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

`jvn::unordered_map` is a C++ library that provides a fast and efficient hash table implementation based on the Robin Hood Hashing algorithm. It is designed to be highly performant while minimizing memory usage, making it an ideal choice for applications that require fast lookups and insertions.

## Features

* Robin Hood Hashing algorithm for high-performance hash table operations.
* Flat memory layout that efficiently utilizes memory by packing structs containing a key-value pair and hash distance.
* Size of the hash table is a power of two for fast hash trimming.
* Optimized code that uses compiler-specific commands to denote unlikely branches.
* Fixed-size memory pool `jvn::AlternatingFixedMemoryAllocator` for quick allocation.
* Benchmarking functions for measuring performance.

## Getting Started

### Prerequisites

* A C++ compiler that supports C++11 or later.
* No external libraries or dependencies are required.

### Installation

`jvn::unordered_map` is a header-only library, so there is no installation process required. Simply copy the header files to your project directory and include them in your source code.

### Usage

```cpp
#include "map.h"

jvn::unordered_map<int, std::string> map;

p.insert(std::make_pair<int, std::string>(1, "one"));
p.insert(std::make_pair<int, std::string>(2, "two"));
p.insert(std::make_pair<int, std::string>(3, "three"));

auto value = map.find(2);

if (value != map.end())
    std::cout << "Value found: " << value->second << std::endl;
else
    std::cout << "Value not found" << std::endl;
```
### Benchmarking

The `benchmark` directory contains benchmarking functions that can be used to measure the performance of `jvn::unordered_map`. To run the benchmarks, follow these steps:
1. Generate a file containing test data. A Python script is provided in the `benchmark` directory to generate test data. Run the script with the following command: `python generate_test_data.py <size> <file>`
2. Compile and run the benchmark executable. Use the following commands: `g++ benchmark.cpp -o benchmark` to compile and `./benchmark <file>` to run the benchmark, where `<file>` is the name of the file containing the CSV test data.
