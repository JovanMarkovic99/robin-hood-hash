import os
import argparse
import random
from collections import defaultdict

def generate_data(num_pairs):
    keys = [random.randint(0, 1000000) for _ in range(num_pairs)]
    values = [random.randint(0, 1000000) for _ in range(num_pairs)]
    return zip(keys, values)

def write_data(data, filename):
    dir_path = os.path.dirname(filename)
    os.makedirs(dir_path, exist_ok=True)

    with open(filename, "w") as f:
        for key, value in data:
            f.write(f"{key},{value}\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate test data for the benchmark.cpp benchmark")
    parser.add_argument("num_pairs", type=int, help="Number of key-value pairs to generate")
    parser.add_argument("output_file", type=str, help="Output file name/path")
    args = parser.parse_args()

    dir_path = os.path.dirname(os.path.abspath(__file__))

    input_files = [os.join(dir_path, file_path) for file_path in args.input_files]
    print(input_files)
    data = generate_data(input_files)

    write_data(data, args.output_file)