
#include <iostream>
#include <fstream>
#include <typeindex>
#include <chrono>
#include <vector>
#include <array>
#include <unordered_map>
#include <functional>
#include <numeric>
#include <cmath>

#include "../map.h"


// USER DEFINED --------------------------------------------------------------------------


const size_t NUM_ITERATIONS = 100;

using KeyType = int;
using ValueType = int;
using MapType = jvn::unordered_map<KeyType, ValueType>;

KeyType getKey(std::string input) {   
    return std::stoi(input);
}

ValueType getValue(std::string input) {
    return std::stoi(input);
}

// Type naming
static std::unordered_map<std::type_index, const char*> type_names = {
    {typeid(char), "char"},
    {typeid(short), "short"},
    {typeid(int), "int"},
    {typeid(long), "long"},
    {typeid(long long), "long long"},
    {typeid(float), "float"},
    {typeid(double), "doable"},
    {typeid(std::string), "std::string"},
    {typeid(MapType), "jvn::unordered_map"}
};


// END USER DEFINED ----------------------------------------------------------------------


// Shared map for fast copy-constructor creation
static MapType filled_map;

using KeyValueType = std::pair<KeyType, ValueType>;
using Clock = std::chrono::high_resolution_clock;
using ClkNano = std::chrono::nanoseconds;
using ClkMicro = std::chrono::microseconds;
using ClkMili = std::chrono::milliseconds;
using ClkSec = std::chrono::seconds;


ClkNano timeDifference(std::chrono::time_point<Clock> start, std::chrono::time_point<Clock> stop) {
    return std::chrono::duration_cast<ClkNano>(stop - start);
}

ClkNano measureErase(const std::vector<KeyValueType>& data_vec) {
    MapType map = filled_map;

    auto start = Clock::now();
    for (auto [key, value]: data_vec)
        map.erase(key);
    auto stop = Clock::now();

    return timeDifference(start, stop);
}

ClkNano measureFind(const std::vector<KeyValueType>& data_vec) {
    const MapType& map = filled_map;

    auto start = Clock::now();
    for (auto [key, value]: data_vec) {
        auto iter = map.find(key);
        if (iter == map.end())
            std::cerr << "ERROR! Key not found in map!\n";
    }
    auto stop = Clock::now();

    return timeDifference(start, stop);
}

ClkNano measureInsertion(const std::vector<KeyValueType>& data_vec) {
    MapType map;

    auto start = Clock::now();
    for (auto key_value: data_vec)
        map.insert(key_value);
    auto stop = Clock::now();

    return timeDifference(start, stop);
}

std::tuple<ClkNano, ClkNano, ClkNano> calcStats(const std::array<ClkNano, NUM_ITERATIONS>& measurements) {
    ClkNano total_time{0};
    for (auto time: measurements)
        total_time += time;

    ClkNano avrg_time = total_time / NUM_ITERATIONS,
        sum_of_squared_diff{0};
    for (auto time: measurements) {
        ClkNano diff = time - avrg_time;
        ClkNano square_diff = ClkNano(diff.count() * diff.count());
        sum_of_squared_diff += square_diff;
    }

    return {
        total_time, 
        avrg_time, 
        std::chrono::duration_cast<ClkNano>(
            std::chrono::duration<double, std::nano>(
                sqrt(double(sum_of_squared_diff.count()) / NUM_ITERATIONS)
            )
        )
    };
}

// TODO: Remove potential further influence of std::cout
std::tuple<ClkNano, ClkNano, ClkNano>  measure(const std::vector<KeyValueType>& data_vec, 
                                    std::function<ClkNano(const std::vector<KeyValueType>&)> measuring_func,
                                    std::string function_name) {
    std::cout << "Benchmarking " << function_name << "..." << std::endl;

    std::array<ClkNano, NUM_ITERATIONS> measurements;
    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
        measurements[i] = measuring_func(data_vec);
    }

    return calcStats(measurements);
}

inline void fillMap(MapType& map, const std::vector<KeyValueType>& data_vec) {
    map.reserve(data_vec.size());
    for (auto key_val_pair: data_vec)
        map.insert(key_val_pair);
}

void printData(ClkNano total_time, ClkNano avrg, ClkNano stddev, size_t num_elements, std::string function_name) {
    std::cout << "Finished benchmarking " << function_name << " after " << std::chrono::duration_cast<ClkMili>(total_time).count() << "ms.\n"
            << "Average total time: " << std::chrono::duration_cast<ClkMicro>(avrg).count() << "μs +/- " 
                                    << 2 * std::chrono::duration_cast<ClkMicro>(stddev).count() << "μs\n"
            << "Average per-element time: " << std::chrono::duration_cast<ClkNano>(avrg / num_elements).count() << "ns\n\n";
}

void runBenchmark(const std::vector<KeyValueType>& data_vec, std::ostream& output) {
    fillMap(filled_map, data_vec);

    auto data_size = data_vec.size();

    auto [total_insertion, avrg_insertion, dev_insertion] = measure(data_vec, measureInsertion, "insertions");
    printData(total_insertion, avrg_insertion, dev_insertion, data_size, "insertions");

    auto [total_find, avrg_find, dev_find] = measure(data_vec, measureFind, "finds");
    printData(total_find, avrg_find, dev_find, data_size, "finds");

    auto [total_erase, avrg_erase, dev_erase] = measure(data_vec, measureErase, "erases");
    printData(total_erase, avrg_erase, dev_erase, data_size, "erases");

    if (output.good()) {
        output << "Map:\t" << type_names[typeid(MapType)] << '\n'
            << "Key:\t" << type_names[typeid(KeyType)] << '\n'
            << "Value:\t" << type_names[typeid(ValueType)] << '\n'
            << "Iterations:\t" << NUM_ITERATIONS << '\n'
            << "Data-Set:\t" << data_vec.size() << '\n'
            << "Insert:\t" << total_insertion.count() << ',' << avrg_insertion.count() << ',' << dev_insertion.count() << '\n'
            << "Find:\t" << total_find.count() << ',' << avrg_find.count() << ',' << dev_find.count() << '\n'
            << "Erase:\t" << total_erase.count() << ',' << avrg_erase.count() << ',' << dev_erase.count() << '\n'; 
    }
}

std::vector<KeyValueType> readData(std::istream& input) {
    std::vector<KeyValueType> data_vec;
    std::string line;
    while (getline(input, line)) {
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

    return data_vec;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <data_path> [<output_path>]\n";
        return 1;
    }

    std::ifstream input_file(argv[1]);
    if (!input_file.is_open()) {
        std::cerr << "Could not open file: " << argv[1] << "\n";
        return 1;
    }
    
    auto data_vec = readData(input_file);
    input_file.close();

    std::ofstream output_file;
    if (argc > 2) {
        // Close std::cout if writing to a file
        std::cout.setstate(std::ios_base::failbit);
        output_file.open(argv[2], std::ios::out | std::ios::trunc);
        if (!output_file.is_open()) {
            std::cerr << "Could not create/open file " << argv[2] << '\n';
            return 1;
        }
    }

    std::cout << "Benchmarking data-set of " << data_vec.size() << " size with " << NUM_ITERATIONS << " iterations\n\n";
    runBenchmark(data_vec, output_file);
    
    return 0;
}