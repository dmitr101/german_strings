#!/usr/bin/env python3

import json
import argparse
import sys
import re
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    import matplotlib.pyplot as plt
    import numpy as np
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False

def extract_input_size(benchmark_name: str) -> Optional[int]:
    """Extract input size from benchmark name."""
    # Look for patterns like /1000/, /10000/, etc.
    match = re.search(r'/(\d+)/', benchmark_name)
    if match:
        return int(match.group(1))
    
    # Look for patterns at the end like /1000, /10000
    match = re.search(r'/(\d+)$', benchmark_name)
    if match:
        return int(match.group(1))
    
    return None

def parse_benchmark_data(results: Dict) -> Dict[str, Dict[str, List[Tuple[int, float]]]]:
    """Parse benchmark results and group by test type and implementation."""
    data = {}
    
    for benchmark in results['benchmarks']:
        name = benchmark['name']
        cpu_time = benchmark['cpu_time']
        
        # Extract test type (everything before the first '<' or '/')
        test_type = name.split('<')[0].split('/')[0]
        
        # Extract implementation type
        if '<std::string>' in name or 'std::string' in name:
            impl_type = 'std::string'
        elif '<gs::german_string>' in name or 'gs::german_string' in name:
            impl_type = 'gs::german_string'
        else:
            impl_type = 'other'
        
        # Extract input size
        input_size = extract_input_size(name)
        if input_size is None:
            continue
        
        # Initialize data structure
        if test_type not in data:
            data[test_type] = {}
        if impl_type not in data[test_type]:
            data[test_type][impl_type] = []
        
        data[test_type][impl_type].append((input_size, cpu_time))
    
    # Sort data points by input size
    for test_type in data:
        for impl_type in data[test_type]:
            data[test_type][impl_type].sort(key=lambda x: x[0])
    
    return data

def generate_graphs(results_file: str, output_dir: Optional[str] = None) -> None:
    """Generate performance graphs per test type."""
    if not MATPLOTLIB_AVAILABLE:
        print("Error: matplotlib is required for graph generation. Install with: pip install matplotlib")
        sys.exit(1)
    
    results = load_benchmark_results(results_file)
    data = parse_benchmark_data(results)
    
    if output_dir is None:
        output_dir = Path(results_file).parent / "graphs"
    else:
        output_dir = Path(output_dir)
    
    output_dir.mkdir(exist_ok=True)
    
    print(f"Generating graphs in {output_dir}")
    
    # Set style for better looking graphs
    plt.style.use('seaborn-v0_8' if 'seaborn-v0_8' in plt.style.available else 'default')
    
    for test_type, implementations in data.items():
        if not implementations:
            continue
            
        plt.figure(figsize=(12, 8))
        
        # Colors for different implementations
        colors = {'std::string': '#1f77b4', 'gs::german_string': '#ff7f0e', 'other': '#2ca02c'}
        markers = {'std::string': 'o', 'gs::german_string': 's', 'other': '^'}
        
        # Determine the time unit for this graph
        all_times = []
        for impl_type, points in implementations.items():
            all_times.extend([point[1] for point in points])
        
        if not all_times:
            continue
            
        max_time = max(all_times)
        if max_time > 1e9:  # > 1 second
            time_unit = "seconds"
            time_divisor = 1e9
        elif max_time > 1e6:  # > 1 millisecond  
            time_unit = "milliseconds"
            time_divisor = 1e6
        elif max_time > 1e3:  # > 1 microsecond
            time_unit = "microseconds"
            time_divisor = 1e3
        else:
            time_unit = "nanoseconds"
            time_divisor = 1
        
        for impl_type, points in implementations.items():
            if not points:
                continue
                
            x_values = [point[0] for point in points]
            y_values = [point[1] / time_divisor for point in points]
            
            plt.plot(x_values, y_values, 
                    marker=markers.get(impl_type, 'o'),
                    color=colors.get(impl_type, 'black'),
                    label=impl_type,
                    linewidth=2.5,
                    markersize=8,
                    markeredgewidth=1,
                    markeredgecolor='white')
        
        plt.xlabel('Input Size', fontsize=14, fontweight='bold')
        plt.ylabel(f'Time ({time_unit})', fontsize=14, fontweight='bold')
        plt.title(f'{test_type} Performance Comparison', fontsize=16, fontweight='bold', pad=20)
        plt.grid(True, alpha=0.3, linestyle='--')
        plt.legend(fontsize=12, frameon=True, shadow=True)
        
        # Use log scale if there's a wide range of values
        x_values_all = []
        y_values_all = []
        for impl_type, points in implementations.items():
            x_values_all.extend([point[0] for point in points])
            y_values_all.extend([point[1] / time_divisor for point in points])
        
        if x_values_all and max(x_values_all) / min(x_values_all) > 10:
            plt.xscale('log')
            plt.xlabel('Input Size (log scale)', fontsize=14, fontweight='bold')
        
        # Add percentage improvement annotation if both std::string and gs::german_string exist
        if 'std::string' in implementations and 'gs::german_string' in implementations:
            std_points = dict(implementations['std::string'])
            gs_points = dict(implementations['gs::german_string'])
            
            # Find common input sizes
            common_sizes = set(std_points.keys()) & set(gs_points.keys())
            if common_sizes:
                max_size = max(common_sizes)
                std_time = std_points[max_size] / time_divisor
                gs_time = gs_points[max_size] / time_divisor
                improvement = ((std_time - gs_time) / std_time) * 100
                
                if improvement > 0:
                    plt.text(0.02, 0.98, f'Max improvement: {improvement:.1f}% faster', 
                            transform=plt.gca().transAxes, fontsize=11,
                            bbox=dict(boxstyle='round,pad=0.3', facecolor='lightgreen', alpha=0.7),
                            verticalalignment='top')
                else:
                    plt.text(0.02, 0.98, f'Performance: {abs(improvement):.1f}% slower', 
                            transform=plt.gca().transAxes, fontsize=11,
                            bbox=dict(boxstyle='round,pad=0.3', facecolor='lightcoral', alpha=0.7),
                            verticalalignment='top')
        
        plt.tight_layout()
        
        # Save the graph
        output_file = output_dir / f"{test_type}_performance.png"
        plt.savefig(output_file, dpi=300, bbox_inches='tight', facecolor='white')
        print(f"Generated: {output_file}")
        
        plt.close()
    
    # Generate a summary comparison graph
    generate_summary_graph(data, output_dir, time_unit="microseconds")
    
    print(f"\nAll graphs generated in: {output_dir}")

def generate_summary_graph(data: Dict, output_dir: Path, time_unit: str = "microseconds") -> None:
    """Generate a summary graph comparing performance across all test types."""
    
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))
    axes = axes.flatten()
    
    colors = {'std::string': '#1f77b4', 'gs::german_string': '#ff7f0e'}
    
    plot_idx = 0
    for test_type, implementations in sorted(data.items()):
        if plot_idx >= len(axes):
            break
            
        if 'std::string' not in implementations or 'gs::german_string' not in implementations:
            continue
            
        ax = axes[plot_idx]
        
        for impl_type in ['std::string', 'gs::german_string']:
            if impl_type not in implementations:
                continue
                
            points = implementations[impl_type]
            if not points:
                continue
                
            x_values = [point[0] for point in points]
            y_values = [point[1] / 1e3 for point in points]  # Convert to microseconds
            
            ax.plot(x_values, y_values, 
                   marker='o' if impl_type == 'std::string' else 's',
                   color=colors[impl_type],
                   label=impl_type,
                   linewidth=2,
                   markersize=6)
        
        ax.set_xlabel('Input Size', fontsize=10)
        ax.set_ylabel('Time (μs)', fontsize=10)
        ax.set_title(test_type, fontsize=11, fontweight='bold')
        ax.grid(True, alpha=0.3)
        ax.legend(fontsize=9)
        
        # Use log scale if needed
        x_values_all = []
        for impl_type, points in implementations.items():
            x_values_all.extend([point[0] for point in points])
        if x_values_all and max(x_values_all) / min(x_values_all) > 10:
            ax.set_xscale('log')
        
        plot_idx += 1
    
    # Hide unused subplots
    for idx in range(plot_idx, len(axes)):
        axes[idx].set_visible(False)
    
    plt.suptitle('Performance Comparison Summary', fontsize=16, fontweight='bold')
    plt.tight_layout()
    
    summary_file = output_dir / "performance_summary.png"
    plt.savefig(summary_file, dpi=300, bbox_inches='tight', facecolor='white')
    print(f"Generated: {summary_file}")
    plt.close()

def load_benchmark_results(file_path: str) -> Dict:
    """Load benchmark results from JSON file."""
    with open(file_path, 'r') as f:
        return json.load(f)

def format_time(nanoseconds: float) -> str:
    """Format time in appropriate units."""
    if nanoseconds < 1000:
        return f"{nanoseconds:.2f} ns"
    elif nanoseconds < 1000000:
        return f"{nanoseconds/1000:.2f} μs"
    elif nanoseconds < 1000000000:
        return f"{nanoseconds/1000000:.2f} ms"
    else:
        return f"{nanoseconds/1000000000:.2f} s"

def calculate_speedup(baseline: float, optimized: float) -> str:
    """Calculate speedup ratio."""
    if baseline == 0:
        return "N/A"
    ratio = baseline / optimized
    if ratio > 1:
        return f"{ratio:.2f}x faster"
    else:
        return f"{1/ratio:.2f}x slower"

def compare_benchmarks(baseline_results: Dict, optimized_results: Dict, 
                      baseline_pattern: str = "std::string", 
                      optimized_pattern: str = "gs::german_string") -> None:
    """Compare benchmark results between two implementations."""
    
    baseline_benchmarks = {}
    optimized_benchmarks = {}
    
    # Group benchmarks by name (without template parameter)
    for benchmark in baseline_results['benchmarks']:
        name = benchmark['name']
        if baseline_pattern in name:
            clean_name = name.replace(f"<{baseline_pattern}>", "").replace(baseline_pattern, "")
            baseline_benchmarks[clean_name] = benchmark
    
    for benchmark in optimized_results['benchmarks']:
        name = benchmark['name']
        if optimized_pattern in name:
            clean_name = name.replace(f"<{optimized_pattern}>", "").replace(optimized_pattern, "")
            optimized_benchmarks[clean_name] = benchmark
    
    print(f"Benchmark Comparison: {baseline_pattern} vs {optimized_pattern}")
    print("=" * 80)
    print(f"{'Benchmark Name':<40} {'Baseline':<15} {'Optimized':<15} {'Speedup':<15}")
    print("-" * 80)
    
    # Find common benchmarks
    common_benchmarks = set(baseline_benchmarks.keys()) & set(optimized_benchmarks.keys())
    
    total_speedup = []
    
    for name in sorted(common_benchmarks):
        baseline = baseline_benchmarks[name]
        optimized = optimized_benchmarks[name]
        
        baseline_time = baseline['cpu_time']
        optimized_time = optimized['cpu_time']
        
        speedup = calculate_speedup(baseline_time, optimized_time)
        
        print(f"{name:<40} {format_time(baseline_time):<15} {format_time(optimized_time):<15} {speedup:<15}")
        
        if baseline_time > 0:
            total_speedup.append(baseline_time / optimized_time)
    
    if total_speedup:
        avg_speedup = sum(total_speedup) / len(total_speedup)
        print("-" * 80)
        print(f"Average speedup: {avg_speedup:.2f}x")

def generate_report(results_file: str, output_file: Optional[str] = None) -> None:
    """Generate a detailed benchmark report."""
    results = load_benchmark_results(results_file)
    
    report_lines = []
    report_lines.append("# Benchmark Results Report")
    report_lines.append("")
    
    # System info
    context = results['context']
    report_lines.append("## System Information")
    report_lines.append(f"- **Host**: {context['host_name']}")
    report_lines.append(f"- **CPU Cores**: {context['num_cpus']}")
    report_lines.append(f"- **CPU MHz**: {context['mhz_per_cpu']}")
    report_lines.append(f"- **Date**: {context['date']}")
    report_lines.append(f"- **Benchmark Library**: {context['library_version']} ({context['library_build_type']})")
    report_lines.append("")
    
    # Cache info
    report_lines.append("## CPU Cache Information")
    for cache in context['caches']:
        size_kb = cache['size'] // 1024
        report_lines.append(f"- **L{cache['level']} {cache['type']}**: {size_kb} KB (shared by {cache['num_sharing']} cores)")
    report_lines.append("")
    
    # Group benchmarks by type
    benchmark_groups = {}
    for benchmark in results['benchmarks']:
        name = benchmark['name']
        # Extract benchmark family name
        family_name = name.split('/')[0].split('<')[0]
        if family_name not in benchmark_groups:
            benchmark_groups[family_name] = []
        benchmark_groups[family_name].append(benchmark)
    
    report_lines.append("## Benchmark Results")
    report_lines.append("")
    
    for group_name, benchmarks in sorted(benchmark_groups.items()):
        report_lines.append(f"### {group_name}")
        report_lines.append("")
        report_lines.append("| Configuration | CPU Time | Iterations | Items/sec |")
        report_lines.append("|---------------|----------|------------|-----------|")
        
        for benchmark in sorted(benchmarks, key=lambda x: x['name']):
            name = benchmark['name'].replace(group_name, "").lstrip('/')
            cpu_time = format_time(benchmark['cpu_time'])
            iterations = benchmark['iterations']
            
            # Calculate items per second if possible
            if 'items_processed' in benchmark:
                items_per_sec = benchmark['items_processed'] / (benchmark['cpu_time'] / 1e9)
                items_per_sec_str = f"{items_per_sec:.0f}"
            else:
                items_per_sec_str = "N/A"
            
            report_lines.append(f"| {name} | {cpu_time} | {iterations:,} | {items_per_sec_str} |")
        
        report_lines.append("")
    
    report_content = "\n".join(report_lines)
    
    if output_file:
        with open(output_file, 'w') as f:
            f.write(report_content)
        print(f"Report written to {output_file}")
    else:
        print(report_content)

def main():
    parser = argparse.ArgumentParser(description="Analyze and compare benchmark results")
    parser.add_argument("command", choices=["compare", "report", "graph"], 
                       help="Command to execute")
    parser.add_argument("--baseline", help="Baseline benchmark results JSON file")
    parser.add_argument("--optimized", help="Optimized benchmark results JSON file")
    parser.add_argument("--results", help="Benchmark results JSON file (for report/graph)")
    parser.add_argument("--output", help="Output file for report or directory for graphs")
    parser.add_argument("--baseline-pattern", default="std::string",
                       help="Pattern to identify baseline benchmarks")
    parser.add_argument("--optimized-pattern", default="gs::german_string",
                       help="Pattern to identify optimized benchmarks")
    
    args = parser.parse_args()
    
    if args.command == "compare":
        if not args.baseline or not args.optimized:
            print("Error: --baseline and --optimized are required for compare command")
            sys.exit(1)
        
        baseline_results = load_benchmark_results(args.baseline)
        optimized_results = load_benchmark_results(args.optimized)
        compare_benchmarks(baseline_results, optimized_results, 
                         args.baseline_pattern, args.optimized_pattern)
    
    elif args.command == "report":
        if not args.results:
            print("Error: --results is required for report command")
            sys.exit(1)
        
        generate_report(args.results, args.output)
    
    elif args.command == "graph":
        if not args.results:
            print("Error: --results is required for graph command")
            sys.exit(1)
        
        generate_graphs(args.results, args.output)

if __name__ == "__main__":
    main()
