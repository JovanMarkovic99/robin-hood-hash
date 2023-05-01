
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <array>
#include <functional>
#include <numeric>
#include <cmath>

#include "../map.h"


// USER DEFINED --------------------------------------------------------------------------


#define NUM_ITERATIONS 100


using KeyType = int;
using ValueType = int;
using MapType = jvn::unordered_map<KeyType, ValueType>;

KeyType getKey(std::string input) {   
    return std::stoi(input);
}

ValueType getValue(std::string input) {
    return std::stoi(input);
}


// END USER DEFINED ----------------------------------------------------------------------


using KeyValueType = std::pair<KeyType, ValueType>;
using Clock = std::chrono::high_resolution_clock;

uint64_t timeDifference(std::chrono::time_point<Clock> start, std::chrono::time_point<Clock> stop) {
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count());
}

void printData(const std::vector<KeyValueType>& data_vec, uint64_t avrg, uint64_t stddev, std::string function_name) {
    std::cout << "Finished benchmarking " << function_name << " after " << NUM_ITERATIONS << " iterations.\n";
    std::cout << "Average total time: " << avrg / 1000.0L << "ms +/- " << stddev / 1000.0L << "ms\n";
    std::cout << "Average per-element time: " << avrg / data_vec.size() << "ns\n\n";
}

std::pair<uint64_t, uint64_t> calcStats(const std::array<uint64_t, NUM_ITERATIONS>& measurements) {
    // Overflow-free average calculation
    uint64_t avrg = 0,
        reminder = 0;
    for (uint64_t time: measurements) {
        avrg += time / NUM_ITERATIONS;
        reminder += time % NUM_ITERATIONS;
    }
    avrg += reminder / NUM_ITERATIONS;

    uint64_t sum_of_squared_diff = 0;
    for (uint64_t time: measurements)
        sum_of_squared_diff += (time - avrg) * (time - avrg);

    return {avrg, sqrt(double(sum_of_squared_diff) / NUM_ITERATIONS)};
}

uint64_t measureErase(const std::vector<KeyValueType>& data_vec) {
    MapType map;
    for (auto key_value: data_vec)
        map.insert(key_value);

    auto start = Clock::now();
    for (auto [key, value]: data_vec)
        map.erase(key);
    auto stop = Clock::now();

    return timeDifference(start, stop);
}

uint64_t measureFind(const std::vector<KeyValueType>& data_vec) {
    MapType map;
    for (auto key_value: data_vec)
        map.insert(key_value);

    auto start = Clock::now();
    for (auto [key, value]: data_vec) {
        auto iter = map.find(key);
        if (iter == map.end())
            std::cout << "ERROR! Key not found in map!\n";
    }
    auto stop = Clock::now();

    return timeDifference(start, stop);
}

uint64_t measureInsertion(const std::vector<KeyValueType>& data_vec) {
    MapType map;

    auto start = Clock::now();
    for (auto key_value: data_vec)
        map.insert(key_value);
    auto stop = Clock::now();

    return timeDifference(start, stop);
}

std::pair<uint64_t, uint64_t> measure(const std::vector<KeyValueType>& data_vec, 
                                    std::function<uint64_t(const std::vector<KeyValueType>&)> measuring_func, 
                                    std::string function_name) {
    std::cout << "Benchmarking " << function_name << "...\n";

    std::array<uint64_t, NUM_ITERATIONS> measurements;
    std::cout << "Iteration ";
    for (uint64_t i = 0; i < NUM_ITERATIONS; ++i) {
        std::cout << i + 1;

        measurements[i] = measuring_func(data_vec);
        
        // Replace iteration number
        std::cout << std::string(std::to_string(i + 1).size(), '\b');
    }

    std::cout << "\bs finished succesfully.\n";
    return calcStats(measurements);
}

void runBenchmark(const std::vector<KeyValueType>& data_vec) {
    auto [avrg_insertion, dev_insertion] =  measure(data_vec, measureInsertion, "insertions");
    printData(data_vec, avrg_insertion, dev_insertion, "insertions");

    auto [avrg_find, dev_find] =  measure(data_vec, measureFind, "finds");
    printData(data_vec, avrg_find, dev_find, "finds");

    auto [avrg_erase, dev_erase] =  measure(data_vec, measureErase, "erases");
    printData(data_vec, avrg_erase, dev_erase, "erases");
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return 1;
    }

    std::ifstream input_file(argv[1]);
    if (!input_file.is_open()) {
        std::cerr << "Could not open file: " << argv[1] << "\n";
        return 1;
    }

    std::vector<KeyValueType> data_vec;
    std::string line;
    while (getline(input_file, line)) {
        auto comma_pos = line.find(',');
        if (comma_pos == std::string::npos) {
            std::cerr << "Error: invalid CSV line: " << line << '\n';
            continue;
        }

        try {
            KeyType key = getKey(line.substr(0, comma_pos));
            ValueType value = getKey(line.substr(comma_pos + 1));

            data_vec.emplace_back(key, value);
        }
        catch (const std::exception& e) {
            std::cerr << "Error while parsing CSV line: " << line << '\n';
        }
    }
    input_file.close();

    std::cout << "Benchmarking data-set of " << data_vec.size() << " size\n\n";
    runBenchmark(data_vec);

    return 0;
}