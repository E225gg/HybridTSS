import re
import csv
import sys
from typing import Dict, List


def parse_output_file(file_path: str) -> List[Dict[str, any]]:
    """
    Parse the output.txt file and extract metrics for each classification method.

    Metrics extracted:
    - Dataset name (e.g., acl1_1k)
    - Construction time (ms)
    - Total classification time (s)
    - Average classification time (us)
    - Throughput for classification (Mpps)
    - Total update time (s)
    - Average update time (us)
    - Throughput for update (Mpps)

    Args:
        file_path: Path to the output.txt file

    Returns:
        List of dictionaries containing all extracted metrics
    """

    data = []
    current_dataset = None

    with open(file_path, "r") as f:
        content = f.read()

    # Split by "-------------------------------" to separate different class runs
    sections = content.split("-------------------------------")

    for section in sections:
        lines = section.strip().split("\n")

        # Check for dataset name (appears at the beginning of section)
        for line in lines:
            # Dataset names typically end with a number like acl1_1k, acl2_10k, etc.
            if "./" in line or "../" in line:
                # Extract the dataset name from the path
                dataset_match = re.search(r"([a-z]+\d+_\d+[a-z]*)", line)
                if dataset_match:
                    current_dataset = dataset_match.group(1)

        # Find the class name
        class_name = None
        for line in lines:
            if "class:" in line:
                class_name = line.split("class:")[1].strip().split(":")[0]
                break

        if not class_name:
            continue

        # Extract metrics
        metrics = {
            "Dataset": current_dataset,
            "Class": class_name,
            "Construction_time_ms": None,
            "Total_classification_time_s": None,
            "Average_classification_time_us": None,
            "Throughput_classification_Mpps": None,
            "Total_update_time_s": None,
            "Average_update_time_us": None,
            "Throughput_update_Mpps": None,
        }

        section_text = "\n".join(lines)

        # Extract Construction time
        construction_match = re.search(
            r"Construction time:\s*([\d.]+)\s*ms", section_text
        )
        if construction_match:
            metrics["Construction_time_ms"] = float(construction_match.group(1))

        # Extract Total classification time
        total_class_match = re.search(
            r"Total classification time:\s*([\d.]+)\s*s", section_text
        )
        if total_class_match:
            metrics["Total_classification_time_s"] = float(total_class_match.group(1))

        # Extract Average classification time
        avg_class_match = re.search(
            r"Average classification time:\s*([\d.]+)\s*us", section_text
        )
        if avg_class_match:
            metrics["Average_classification_time_us"] = float(avg_class_match.group(1))

        # Extract Throughput for classification
        throughput_class_match = re.search(
            r"(?:Classify Performance:.*?)?Throughput:\s*([\d.]+)\s*Mpps(?=\n|Update)",
            section_text,
            re.DOTALL,
        )
        if throughput_class_match:
            metrics["Throughput_classification_Mpps"] = float(
                throughput_class_match.group(1)
            )

        # Extract Total update time
        total_update_match = re.search(
            r"Total update time:\s*([\d.]+)\s*s", section_text
        )
        if total_update_match:
            metrics["Total_update_time_s"] = float(total_update_match.group(1))

        # Extract Average update time
        avg_update_match = re.search(
            r"Average update time:\s*([\d.]+)\s*us", section_text
        )
        if avg_update_match:
            metrics["Average_update_time_us"] = float(avg_update_match.group(1))

        # Extract Throughput for update (second occurrence)
        throughput_matches = re.findall(r"Throughput:\s*([\d.]+)\s*Mpps", section_text)
        if len(throughput_matches) >= 2:
            metrics["Throughput_update_Mpps"] = float(throughput_matches[1])

        data.append(metrics)

    return data


def save_to_csv(data: List[Dict], output_path: str = "extracted_metrics.csv") -> None:
    """Save the extracted metrics to a CSV file."""
    if not data:
        print("No data to save")
        return

    fieldnames = data[0].keys()
    with open(output_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(data)

    print(f"Metrics saved to {output_path}")


def display_metrics(data: List[Dict]) -> None:
    """Display the extracted metrics in a formatted table."""
    if not data:
        print("No data to display")
        return

    print("\n=== Extracted Metrics ===\n")

    # Get column widths
    fieldnames = data[0].keys()
    col_widths = {field: len(field) for field in fieldnames}

    for row in data:
        for field in fieldnames:
            value = str(row[field]) if row[field] is not None else "N/A"
            col_widths[field] = max(col_widths[field], len(value))

    # Print header
    header = " | ".join(f"{field:<{col_widths[field]}}" for field in fieldnames)
    print(header)
    print("-" * len(header))

    # Print rows
    for row in data:
        row_str = " | ".join(
            f"{str(row[field] if row[field] is not None else 'N/A'):<{col_widths[field]}}"
            for field in fieldnames
        )
        print(row_str)

    print(f"\nTotal records: {len(data)}")


if __name__ == "__main__":
    # Default file names
    input_file = "output.txt"
    output_file = "extracted_metrics.csv"

    # Parse command-line arguments
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    if len(sys.argv) > 2:
        output_file = sys.argv[2]

    # Validate input file exists
    try:
        with open(input_file, "r") as f:
            pass
    except FileNotFoundError:
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)

    print(f"Processing: {input_file}")
    print(f"Output: {output_file}\n")

    # Parse the output file
    data = parse_output_file(input_file)

    # Display the metrics
    display_metrics(data)

    # Save to CSV
    save_to_csv(data, output_file)
