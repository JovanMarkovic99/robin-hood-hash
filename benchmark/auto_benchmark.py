import sys
import os
import argparse
import csv
import subprocess
import multiprocessing
import json
from collections import defaultdict

def mult_range(start, stop, multiplier):
    while start < stop:
        yield start
        start *= multiplier

def generate_data_sets(script_path, data_path, data_sizes):
    data_set_paths = []
    processes = []
    for size in data_sizes:
        output_path = os.path.join(data_path, f'data_set_{size}.csv')

        if FORCE_GENERATE_DATA or not os.path.exists(output_path):
            print(f"Generating data-set of size {size} at {output_path} ...")
            process = multiprocessing.Process(target=subprocess.run, args=(["python", script_path, str(size), output_path],))
            process.start()
            processes.append(process)

        data_set_paths.append(output_path)

    for process in processes:
        process.join()

    print("Finished data generation\n")
    return data_set_paths

def run_benchmarks(benchmark_path, output_path, data_set_paths):
    output_paths = []
    for data_set_path in data_set_paths:
        file_name = os.path.splitext(os.path.basename(data_set_path))[0]
        result_path = os.path.join(output_path, file_name + '.txt')

        if FORCE_BENCHMARK or not os.path.exists(result_path):
            print(f"Running benchmark for data-set {data_set_path} ...")
            subprocess.run([benchmark_path, data_set_path, result_path])

        output_paths.append(result_path)

    print("Finished benchmark.\n")
    return output_paths

def read_data(data_paths):
    data = []
    for data_path in data_paths:
        with open(data_path, "r") as file:
            print(f"Reading benchmark data from {data_path}")
            reader = csv.reader(file, delimiter='\t')
            data_point = dict()
            for row in reader:
                key = row[0].strip(": ").lower()

                value = row[1]
                if row[1][0].isdigit():
                    value = []
                    for val in row[1].split(','):
                        value.append(int(val))
                    
                    if len(value) == 1:
                        value = value[0]

                data_point[key] = value

            data.append(data_point)

    print("Finished reading benchmark data\n")
    return data

def process_data(data):
    '''
    Format:
    operation_type: {
        (map_name, key_type, value_type): [
            [data_size, avrg_erase]
            ...   
        ]
        ...
    }
    '''
    processed_data = {operation_type: defaultdict(list) for operation_type in OPERATIONS}
    for data_row in data:
        map_name, key_type, value_type = data_row["map"], data_row["key"], data_row["value"]
        identifier = f"{map_name}<{key_type},{value_type}>"

        data_size = data_row["data-set"]
        for operation_type in OPERATIONS:
            operation_total_avrg = data_row[operation_type][1]
            operation_avrg = round(operation_total_avrg / data_size, 2)

            data_point = (data_size, operation_avrg)

            processed_data[operation_type][identifier].append(data_point)

    return processed_data

def write_data(data, output_path):
    with open(output_path, "w") as file:
        json.dump(data, file, indent=2)
    print(f"Data written to {output_path}")

# Will force data-set generation and benchmarks if already present
FORCE_GENERATE_DATA = False
FORCE_BENCHMARK = True

# Operations that the benchmark will parse
OPERATIONS = ("insert", "find", "erase")

DATASET_DIR = "datasets\\"
RESULTS_DIR = "results\\"

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Automate test generation and benchmarking for unordered_map's")
    parser.add_argument("--output-file", default="results\\result.json", type=str, help="Output JSON filename")
    args = parser.parse_args()

    dir_path = os.path.dirname(os.path.abspath(__file__))

    script_path, data_path = os.path.join(dir_path, "generate_test_data.py"), os.path.join(dir_path, DATASET_DIR)
    data_set_paths = generate_data_sets(script_path, data_path, mult_range(10, 10000001, 10))

    benchmark_path, output_path = os.path.join(dir_path, "benchmark.exe"), os.path.join(dir_path, RESULTS_DIR)
    output_paths = run_benchmarks(benchmark_path, output_path, data_set_paths)

    data = read_data(output_paths)

    processed_data = process_data(data)
    
    result_data_path = os.path.join(dir_path, args.output_file)
    write_data(processed_data, result_data_path)