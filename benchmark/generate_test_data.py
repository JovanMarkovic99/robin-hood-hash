import os
import argparse
import random

def generate_data(num_pairs):
    keys = random.sample(range(0, 100000000), num_pairs)
    values = random.sample(range(0, 100000000), num_pairs)
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

    args = parser.parse_args()
    data = generate_data(args.num_pairs)

    write_data(data, args.output_file)