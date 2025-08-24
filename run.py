# bench_min.py
import os
import glob
import re
import csv
import subprocess
import matplotlib.pyplot as plt

DIR = './Data/'
OUTPUT_CSV = 'results.csv'

# regex: minimal fields
ALGO_RE = re.compile(r"class:\s*([A-Za-z0-9_]+):")
NUM_MS  = re.compile(r"Construction time:\s*([0-9.]+)\s*ms")
CLS_TP  = re.compile(r"Classify Performance:")
UPD_TP  = re.compile(r"Update Performance:")
THR     = re.compile(r"Throughput:\s*([0-9.]+)\s*Mpps")
CLS_AVG = re.compile(r"Average classification time:\s*([0-9.]+)\s*us")
UPD_AVG = re.compile(r"Average update time:\s*([0-9.]+)\s*us")
MIS     = re.compile(r"([0-9]+)\s+packets are classified,\s+([0-9]+)\s+of them are misclassified")
UP_CNT  = re.compile(r"([0-9]+)\s+rules update:\s+insert_num\s*=\s*([0-9]+)\s+delete_num\s*=\s*([0-9]+)")

# Add configuration at the top
TIMEOUT_SECONDS = 1200  # 20 minutes for large datasets
SKIP_ON_TIMEOUT = True   # Set to False if you want the script to stop on timeout

def find_trace_file(rule_file):
    """Find corresponding trace file for a rule file"""
    base_name = os.path.splitext(rule_file)[0]  # Remove .rules extension
    
    # Try different trace file patterns
    possible_traces = [
        rule_file + '_trace',           # acl1_1k.rules_trace
        base_name + '_trace',           # acl1_1k_trace
        base_name + '.trace',           # acl1_1k.trace
        base_name + '_trace.txt',       # acl1_1k_trace.txt
        rule_file.replace('.rules', '.trace'),  # acl1_1k.trace
    ]
    
    for trace_file in possible_traces:
        if os.path.isfile(trace_file):
            return trace_file
    
    return None

def parse_stdout(stdout: str, dataset: str):
    rows, block = [], {}
    algo, in_cls, in_upd = None, False, False

    def flush():
        nonlocal block
        if algo and block:
            print(f"  Found algorithm: {algo} with {len(block)} metrics")
            rows.append({
                "dataset": dataset,
                "algorithm": algo,
                "construction_time_ms": block.get("construction_time_ms"),
                "classify_packets": block.get("classify_packets"),
                "misclassified": block.get("misclassified"),
                "avg_classify_us": block.get("avg_classify_us"),
                "classify_throughput_mpps": block.get("classify_throughput_mpps"),
                "update_rules": block.get("update_rules"),
                "insert_num": block.get("insert_num"),
                "delete_num": block.get("delete_num"),
                "avg_update_us": block.get("avg_update_us"),
                "update_throughput_mpps": block.get("update_throughput_mpps"),
            })
        block = {}

    for line in stdout.splitlines():
        m = ALGO_RE.search(line)
        if m:
            flush(); algo = m.group(1); in_cls = in_upd = False; continue
        if CLS_TP.search(line): in_cls, in_upd = True, False;  continue
        if UPD_TP.search(line): in_cls, in_upd = False, True;  continue
        m = NUM_MS.search(line)
        if m: block["construction_time_ms"] = float(m.group(1)); continue
        if in_cls and (m := CLS_AVG.search(line)): block["avg_classify_us"] = float(m.group(1)); continue
        if in_upd and (m := UPD_AVG.search(line)): block["avg_update_us"]   = float(m.group(1)); continue
        if in_cls and (m := MIS.search(line)):
            block["classify_packets"] = int(m.group(1)); block["misclassified"] = int(m.group(2)); continue
        if in_upd and (m := UP_CNT.search(line)):
            block["update_rules"] = int(m.group(1)); block["insert_num"] = int(m.group(2)); block["delete_num"] = int(m.group(3)); continue
        m = THR.search(line)
        if m:
            if in_cls: block["classify_throughput_mpps"] = float(m.group(1))
            elif in_upd: block["update_throughput_mpps"] = float(m.group(1))
            continue
        if line.strip().startswith("-------------------------------"): flush()
    flush()
    
    print(f"  Parsed {len(rows)} algorithm results from output")
    return rows

def run_one(data_file, trace_file):
    print(f"Running: ./main -r {data_file} -p {trace_file}")
    
    try:
        run = subprocess.run(['./main', '-r', data_file, '-p', trace_file], 
                           capture_output=True, text=True, timeout=TIMEOUT_SECONDS)
        
        print(f"  Return code: {run.returncode}")
        
        if run.returncode != 0:
            print(f"  ERROR: Program failed with code {run.returncode}")
            if run.stderr:
                print(f"  STDERR: {run.stderr}")
            return []
        
        if not run.stdout:
            print("  WARNING: No output captured")
            return []
        
        print(f"  Output length: {len(run.stdout)} characters")
        
        # Show a sample of the output to verify format
        lines = run.stdout.splitlines()[:5]
        print(f"  First few lines of output:")
        for line in lines:
            print(f"    {line}")
        
        return parse_stdout(run.stdout, os.path.basename(data_file))
        
    except subprocess.TimeoutExpired:
        if SKIP_ON_TIMEOUT:
            print(f"  WARNING: Program timed out after {TIMEOUT_SECONDS} seconds - skipping {os.path.basename(data_file)}")
            return []
        else:
            print(f"  ERROR: Program timed out after {TIMEOUT_SECONDS} seconds")
            raise  # Re-raise the exception to stop the script
    except FileNotFoundError:
        print(f"  ERROR: ./main executable not found")
        return []
    except Exception as e:
        print(f"  ERROR: {e}")
        return []

def main():
    print("=" * 60)
    print("Starting HybridTSS Benchmark")
    print("=" * 60)
    
    # Show current working directory
    print(f"Current working directory: {os.getcwd()}")
    
    # Check what executables exist
    print("\nChecking for executables:")
    executables = ['./main', 'main', './HybridTSS', './neurotss']
    for exe in executables:
        exists = os.path.isfile(exe)
        print(f"  {exe}: {'✓' if exists else '✗'}")
    
    # List all files in current directory
    print(f"\nFiles in current directory:")
    try:
        current_files = [f for f in os.listdir('.') if not f.startswith('.')]
        for f in sorted(current_files)[:10]:  # Show first 10 files
            file_type = "DIR" if os.path.isdir(f) else "FILE"
            print(f"  {file_type}: {f}")
        if len(current_files) > 10:
            print(f"  ... and {len(current_files) - 10} more files")
    except Exception as e:
        print(f"Error reading current directory: {e}")
    
    # Check Data directory
    print(f"\nExamining Data directory contents:")
    try:
        if not os.path.isdir(DIR):
            print(f"ERROR: Directory {DIR} not found!")
            return
            
        all_files = os.listdir(DIR)
        print(f"All files in {DIR}:")
        for f in sorted(all_files):
            file_path = os.path.join(DIR, f)
            file_type = "DIR" if os.path.isdir(file_path) else "FILE"
            size = os.path.getsize(file_path) if os.path.isfile(file_path) else 0
            print(f"  {file_type}: {f} ({size} bytes)")
    except Exception as e:
        print(f"Error reading directory: {e}")
        return
    
    # Try to build the project
    print(f"\nTrying to build the project...")
    if os.path.isfile('Makefile') or os.path.isfile('makefile'):
        print("Found Makefile, running 'make'...")
        result = os.system('make')
        print(f"Make result: {result}")
        
        # Check again for executables after build
        print("Checking for executables after build:")
        for exe in executables:
            exists = os.path.isfile(exe)
            print(f"  {exe}: {'✓' if exists else '✗'}")
    else:
        print("No Makefile found")
    
    # Find the correct executable
    main_exe = None
    for exe in executables:
        if os.path.isfile(exe):
            main_exe = exe
            break
    
    if not main_exe:
        print("\nERROR: No executable found!")
        print("Please build the project first with 'make' or provide the correct executable name")
        return
    
    print(f"\nUsing executable: {main_exe}")
    
    # Continue with the rest of the script
    rows = []
    data_files = [f for f in glob.glob(os.path.join(DIR, '*')) if os.path.isfile(f) and not f.endswith('_trace')]
    
    print(f"\nFound {len(data_files)} potential rule files:")
    for f in sorted(data_files):
        print(f"  {os.path.basename(f)}")
    
    if not data_files:
        print("No data files found!")
        return
    
    print("\nProcessing datasets:")
    print("-" * 40)
    
    for data_file in sorted(data_files):
        dataset_name = os.path.basename(data_file)
        print(f"\nDataset: {dataset_name}")
        
        # Try to find corresponding trace file
        trace_file = find_trace_file(data_file)
        
        if not trace_file:
            print(f"  SKIP: No trace file found for {dataset_name}")
            print(f"  Looked for patterns like: {dataset_name}_trace, {dataset_name}.trace, etc.")
            continue
        
        print(f"  Rule file: {os.path.basename(data_file)}")
        print(f"  Trace file: {os.path.basename(trace_file)}")
        
        # Use the found executable instead of hardcoded './main'
        print(f"Running: {main_exe} -r {data_file} -p {trace_file}")
        
        try:
            run = subprocess.run([main_exe, '-r', data_file, '-p', trace_file], 
                               capture_output=True, text=True, timeout=TIMEOUT_SECONDS)
            
            print(f"  Return code: {run.returncode}")
            
            if run.returncode != 0:
                print(f"  ERROR: Program failed with code {run.returncode}")
                if run.stderr:
                    print(f"  STDERR: {run.stderr}")
                continue
            
            if not run.stdout:
                print("  WARNING: No output captured")
                continue
            
            print(f"  Output length: {len(run.stdout)} characters")
            
            # Show a sample of the output to verify format
            lines = run.stdout.splitlines()[:5]
            print(f"  First few lines of output:")
            for line in lines:
                print(f"    {line}")
            
            result_rows = parse_stdout(run.stdout, os.path.basename(data_file))
            rows.extend(result_rows)
            
        # except subprocess.TimeoutExpired:
        #     print(f"  ERROR: Program timed out after 5 minutes")
        except Exception as e:
            print(f"  ERROR: {e}")

    print("\n" + "=" * 60)
    print(f"SUMMARY: Collected {len(rows)} total results")
    
    if not rows:
        print("No results to save!")
        return

    # Save results
    fieldnames = [
        "dataset","algorithm","construction_time_ms",
        "classify_packets","misclassified","avg_classify_us","classify_throughput_mpps",
        "update_rules","insert_num","delete_num","avg_update_us","update_throughput_mpps",
    ]
    
    with open(OUTPUT_CSV, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        for r in rows:
            w.writerow(r)
    
    print(f"Results saved to: {OUTPUT_CSV}")

    # Generate line charts
    def plot(key, title, out_png):
        valid_data = [(r['dataset'], r['algorithm'], r[key]) for r in rows if r.get(key) is not None]
        
        if not valid_data:
            print(f"No data available for {key} chart")
            return
        
        # Group data by algorithm
        algo_data = {}
        for dataset, algo, value in valid_data:
            if algo not in algo_data:
                algo_data[algo] = {'datasets': [], 'values': []}
            algo_data[algo]['datasets'].append(dataset)
            algo_data[algo]['values'].append(float(value))
        
        plt.figure(figsize=(12, 6))
        
        # Plot line for each algorithm
        for algo, data in algo_data.items():
            plt.plot(data['datasets'], data['values'], marker='o', linewidth=2, label=algo)
        
        plt.xticks(rotation=45, ha="right")
        plt.title(title)
        plt.ylabel(key.replace("_", " ").title())
        plt.xlabel("Dataset")
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        plt.savefig(out_png, dpi=160, bbox_inches="tight")
        plt.close()
        print(f"Chart saved: {out_png}")
    
    if rows:
        print("\nGenerating charts...")
        plot("classify_throughput_mpps", "Classification Throughput (Mpps)", "classify_throughput_mpps.png")
        plot("update_throughput_mpps", "Update Throughput (Mpps)", "update_throughput_mpps.png")
        plot("construction_time_ms", "Construction Time (ms)", "construction_time_ms.png")
    
    print("Done!")

if __name__ == "__main__":
    main()
