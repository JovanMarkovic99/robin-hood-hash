
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <array>
#include <functional>
#include <numeric>
#include <cmath>

#include "../map.h"

#define NUM_ITERATIONS 10

using KeyType = int;
using ValueType = int;
using KeyValueType = std::pair<int, int>;
using MapType = jvn::unordered_map<KeyType, ValueType>;

using Clock = std::chrono::high_resolution_clock;


void printData(double avrg, double stddev, std::string function_name)
{
    std::cout << "Finished benchmarking " << function_name << " after " << NUM_ITERATIONS << " iterations.\n";
    std::cout << "Average total time: " << uint64_t(avrg) << "ns +/- " << uint64_t(stddev) << "ns\n";
    std::cout << "Average per-element time: " << uint64_t(avrg / NUM_ITERATIONS) << "ns\n\n";
}

std::pair<double, double> calcStats(const std::array<uint64_t, NUM_ITERATIONS>& measurements)
{
    double avrg = (double)std::accumulate(measurements.begin(), measurements.end(), 0) / NUM_ITERATIONS,
        sum_of_squared_diff = 0;
    for (uint64_t time: measurements)
        sum_of_squared_diff += std::pow(time - avrg, 2);

    return {avrg, sqrt(sum_of_squared_diff / NUM_ITERATIONS)};
}

uint64_t measureErase(const std::vector<KeyValueType>& data_vec)
{
    MapType map;
    for (auto key_value: data_vec)
        map.insert(key_value);

    auto start = Clock::now();
    for (auto [key, value]: data_vec)
        map.erase(key);
    auto stop = Clock::now();

    return std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
}

uint64_t measureFind(const std::vector<KeyValueType>& data_vec)
{
    MapType map;
    for (auto key_value: data_vec)
        map.insert(key_value);

    auto start = Clock::now();
    for (auto [key, value]: data_vec)
    {
        auto iter = map.find(key);
        if (iter == map.end())
            std::cout << "ERROR! Key not found in map!\n";
    }
    auto stop = Clock::now();

    return std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
}

uint64_t measureInsertion(const std::vector<KeyValueType>& data_vec)
{
    MapType map;

    auto start = Clock::now();
    for (auto key_value: data_vec)
        map.insert(key_value);
    auto stop = Clock::now();

    return std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
}

std::pair<double, double> measure(const std::vector<KeyValueType>& data_vec, std::function<uint64_t(const std::vector<KeyValueType>&)> measuring_func, std::string function_name)
{
    std::cout << "Benchmarking " << function_name << "...\n";

    std::array<uint64_t, NUM_ITERATIONS> measurements;
    for (int i = 0; i < NUM_ITERATIONS; ++i)
    {
        std::cout << "Iteration: " << i + 1 << '\n';
        measurements[i] = measuring_func(data_vec);
    }

    std::cout << '\n';
    return calcStats(measurements);
}

void runBenchmark(const std::vector<KeyValueType>& data_vec)
{
    auto [avrg_insertion, dev_insertion] =  measure(data_vec, measureInsertion, "insertions");
    printData(avrg_insertion, dev_insertion, "insertions");

    auto [avrg_find, dev_find] =  measure(data_vec, measureFind, "finds");
    printData(avrg_insertion, dev_insertion, "insertions");

    auto [avrg_erase, dev_erase] =  measure(data_vec, measureErase, "erases");
    printData(avrg_insertion, dev_insertion, "insertions");
}


int main(int argc, char* argv[]) {
    if (argc < 2) 
    {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return 1;
    }

    std::ifstream input_file(argv[1]);
    if (!input_file.is_open())
    {
        std::cerr << "Could not open file: " << argv[1] << "\n";
        return 1;
    }

    std::vector<KeyValueType> data_vec;
    std::string line;
    while (getline(input_file, line))
    {
        auto comma_pos = line.find(',');
        if (comma_pos == std::string::npos)
        {
            std::cerr << "Error: invalid CSV line: " << line << '\n';
            continue;
        }

        try 
        {
            KeyType key = std::stoi(line.substr(0, comma_pos));
            ValueType value = std::stoi(line.substr(comma_pos + 1));

            data_vec.emplace_back(key, value);
        }
        catch (std::exception e)
        {
            std::cerr << "Error while parsing CSV line: " << line << '\n';
        }
    }
    input_file.close();

    std::cout << "Benchmarking data-set of " << data_vec.size() << " size\n\n";
    runBenchmark(data_vec);

    return 0;
}