import sys
import os
import subprocess
import multiprocessing

def mult_range(start, stop, multiplier):
    while start < stop:
        yield start
        start *= multiplier

def generate_data_sets(script_path, data_path, data_sizes):
    data_set_paths = []
    processes = []
    for size in data_sizes:
        output_path = os.path.join(data_path, f'data_set_{size}.csv')
        print(f"Generating data-set of size {size} at {output_path} ...")

        process = multiprocessing.Process(target=subprocess.run, args=(["python", script_path, str(size), output_path],))
        process.start()
        processes.append(process)

        data_set_paths.append(output_path)

    for process in processes:
        process.join()

    print("Finished data generation")
    return data_set_paths

def run_benchmarks(benchmark_path, output_path, data_set_paths):
    output_paths = []
    for data_set_path in data_set_paths:
        file_name = os.path.splitext(os.path.basename(data_set_path))[0]
        result_path = os.path.join(output_path, file_name + '.txt')
        print(f"Running benchmark for data-set {data_set_path} ...")

        subprocess.run([benchmark_path, data_set_path, result_path])

        output_paths.append(result_path)

    print("Finished benchmark.\n")
    return output_paths

if __name__ == "__main__":
    dir_path = os.path.dirname(os.path.abspath(__file__))

    script_path, data_path = os.path.join(dir_path, "generate_test_data.py"), os.path.join(dir_path, "datasets\\")
    data_set_paths = generate_data_sets(script_path, data_path, mult_range(10, 10000001, 10))

    benchmark_path, output_path = os.path.join(dir_path, "benchmark.exe"), os.path.join(dir_path, "results\\")
    output_paths = run_benchmarks(benchmark_path, output_path, data_set_paths)
