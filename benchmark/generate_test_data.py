import argparse
import random

def generate_data(num_pairs):
    keys = [random.randint(0, 1000000) for _ in range(num_pairs)]
    values = [random.randint(0, 1000000) for _ in range(num_pairs)]
    return zip(keys, values)

def write_data(data, filename):
    with open(filename, "w") as f:
        for key, value in data:
            f.write(f"{key},{value}\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate test data for jvn::unordered_map benchmark")
    parser.add_argument("num_pairs", type=int, help="Number of key-value pairs to generate")
    parser.add_argument("output_file", type=str, help="Output file name")

    args = parser.parse_args()
    data = generate_data(args.num_pairs)

    write_data(data, args.output_file)