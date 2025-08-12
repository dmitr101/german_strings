#!/bin/bash

# Enhanced Benchmark Runner Script for German Strings

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_colored() {
    echo -e "${1}${2}${NC}"
}

print_usage() {
    echo "Usage: $0 [OPTIONS] [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  build              Build the benchmark executables"
    echo "  run                Run all benchmarks and save results"
    echo "  compare            Compare std::string vs german_string performance"
    echo "  micro              Run micro-benchmarks focusing on specific features"
    echo "  enhanced           Run enhanced benchmarks with realistic scenarios"
    echo "  profile            Run benchmarks with profiling data"
    echo "  report             Generate detailed performance report"
    echo "  clean              Clean build directory"
    echo ""
    echo "Options:"
    echo "  --build-type TYPE  Build type: Release (default), Debug, RelWithDebInfo"
    echo "  --compiler COMP    Compiler: clang (default), gcc"
    echo "  --stdlib STDLIB    Standard library: libcxx (default), libstdcxx"
    echo "  --output DIR       Output directory for results (default: ./results)"
    echo "  --filter REGEX     Filter benchmarks by regex pattern"
    echo "  --repetitions N    Number of repetitions for each benchmark (default: 3)"
    echo "  --time-unit UNIT   Time unit: ns, us, ms, s (default: auto)"
    echo "  --format FORMAT    Output format: console, json, csv (default: json)"
    echo "  --parallel         Run benchmarks in parallel"
    echo "  --help             Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 build --build-type Release --compiler clang"
    echo "  $0 run --filter \"StringSort\" --repetitions 5"
    echo "  $0 compare --output ./comparison_results"
    echo "  $0 micro --filter \"SmallString\""
}

# Default values
BUILD_TYPE="Release"
COMPILER="clang"
STDLIB="libcxx"
OUTPUT_DIR="$SCRIPT_DIR/results"
FILTER=""
REPETITIONS=3
TIME_UNIT="auto"
FORMAT="json"
PARALLEL=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --compiler)
            COMPILER="$2"
            shift 2
            ;;
        --stdlib)
            STDLIB="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --filter)
            FILTER="$2"
            shift 2
            ;;
        --repetitions)
            REPETITIONS="$2"
            shift 2
            ;;
        --time-unit)
            TIME_UNIT="$2"
            shift 2
            ;;
        --format)
            FORMAT="$2"
            shift 2
            ;;
        --parallel)
            PARALLEL=true
            shift
            ;;
        --help)
            print_usage
            exit 0
            ;;
        build|run|compare|micro|enhanced|profile|report|clean)
            COMMAND="$1"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# Set build directory based on compiler and stdlib
if [[ "$COMPILER" == "clang" && "$STDLIB" == "libcxx" ]]; then
    BUILD_PRESET="clang-20-libcxx"
elif [[ "$COMPILER" == "clang" && "$STDLIB" == "libstdcxx" ]]; then
    BUILD_PRESET="clang-20"
elif [[ "$COMPILER" == "gcc" ]]; then
    BUILD_PRESET="gcc-11"
else
    print_colored $RED "Error: Unsupported compiler/stdlib combination: $COMPILER/$STDLIB"
    exit 1
fi

ACTUAL_BUILD_DIR="$BUILD_DIR/$BUILD_PRESET"
BENCHMARK_EXECUTABLE="$ACTUAL_BUILD_DIR/german_strings_talk_benchmarks"

# Create output directory
mkdir -p "$OUTPUT_DIR"

build_benchmarks() {
    print_colored $BLUE "Building benchmarks..."
    print_colored $YELLOW "  Build type: $BUILD_TYPE"
    print_colored $YELLOW "  Compiler: $COMPILER"
    print_colored $YELLOW "  Standard library: $STDLIB"
    print_colored $YELLOW "  Build directory: $ACTUAL_BUILD_DIR"
    
    cd "$PROJECT_ROOT"
    
    # Configure with CMake preset
    cmake --preset "$BUILD_PRESET" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    
    # Build
    cmake --build "$ACTUAL_BUILD_DIR" --parallel $(nproc)
    
    if [[ ! -f "$BENCHMARK_EXECUTABLE" ]]; then
        print_colored $RED "Error: Benchmark executable not found at $BENCHMARK_EXECUTABLE"
        exit 1
    fi
    
    print_colored $GREEN "Build completed successfully!"
}

run_benchmark() {
    local benchmark_name="$1"
    local output_file="$2"
    local extra_args="$3"
    
    local benchmark_args=""
    
    if [[ -n "$FILTER" ]]; then
        benchmark_args="$benchmark_args --benchmark_filter=\"$FILTER\""
    fi
    
    if [[ "$REPETITIONS" -gt 1 ]]; then
        benchmark_args="$benchmark_args --benchmark_repetitions=$REPETITIONS"
    fi
    
    if [[ "$TIME_UNIT" != "auto" ]]; then
        benchmark_args="$benchmark_args --benchmark_time_unit=$TIME_UNIT"
    fi
    
    benchmark_args="$benchmark_args --benchmark_format=$FORMAT"
    benchmark_args="$benchmark_args --benchmark_out=\"$output_file\""
    
    if [[ -n "$extra_args" ]]; then
        benchmark_args="$benchmark_args $extra_args"
    fi
    
    print_colored $BLUE "Running $benchmark_name benchmarks..."
    print_colored $YELLOW "  Output: $output_file"
    print_colored $YELLOW "  Args: $benchmark_args"
    
    cd "$ACTUAL_BUILD_DIR"
    eval "./german_strings_talk_benchmarks $benchmark_args"
    
    print_colored $GREEN "$benchmark_name benchmarks completed!"
}

run_all_benchmarks() {
    local timestamp=$(date +%Y%m%d_%H%M%S)
    local base_filename="benchmark_results_${BUILD_PRESET}_${timestamp}"
    
    run_benchmark "All" "$OUTPUT_DIR/${base_filename}.json"
    
    # If JSON format, also generate a readable report
    if [[ "$FORMAT" == "json" ]]; then
        print_colored $BLUE "Generating readable report..."
        "$SCRIPT_DIR/analyze_results.py" report --results "$OUTPUT_DIR/${base_filename}.json" \
            --output "$OUTPUT_DIR/${base_filename}_report.md"
    fi
}

run_comparison() {
    local timestamp=$(date +%Y%m%d_%H%M%S)
    
    # Run benchmarks that have both std::string and german_string variants
    local comparison_filter="(StringConstruction|StringCopyConstruction|StringHashMap|StringSet|StringComparison|StringSort|StringVectorResize|StringSearch)"
    
    print_colored $BLUE "Running comparison benchmarks..."
    run_benchmark "Comparison" "$OUTPUT_DIR/comparison_${BUILD_PRESET}_${timestamp}.json" \
        "--benchmark_filter=\"$comparison_filter\""
    
    # Generate comparison report
    if [[ "$FORMAT" == "json" ]]; then
        print_colored $BLUE "Generating comparison analysis..."
        "$SCRIPT_DIR/analyze_results.py" report --results "$OUTPUT_DIR/comparison_${BUILD_PRESET}_${timestamp}.json" \
            --output "$OUTPUT_DIR/comparison_${BUILD_PRESET}_${timestamp}_report.md"
        
        # Note: For direct comparison, we'd need separate runs or a more sophisticated analysis
        print_colored $YELLOW "For detailed std::string vs german_string comparison, run separate benchmarks and use the compare command."
    fi
}

run_micro_benchmarks() {
    local timestamp=$(date +%Y%m%d_%H%M%S)
    
    # Filter for micro-benchmarks specific to german_string features
    local micro_filter="(GermanString|Prefix|Equality|Cache|Class)"
    
    run_benchmark "Micro" "$OUTPUT_DIR/micro_${BUILD_PRESET}_${timestamp}.json" \
        "--benchmark_filter=\"$micro_filter\""
}

run_enhanced_benchmarks() {
    local timestamp=$(date +%Y%m%d_%H%M%S)
    
    # Filter for enhanced realistic scenario benchmarks
    local enhanced_filter="(Construction|Copy|HashMap|Set|Vector|Search)"
    
    run_benchmark "Enhanced" "$OUTPUT_DIR/enhanced_${BUILD_PRESET}_${timestamp}.json" \
        "--benchmark_filter=\"$enhanced_filter\""
}

run_profiling() {
    print_colored $BLUE "Running benchmarks with profiling data..."
    local timestamp=$(date +%Y%m%d_%H%M%S)
    
    # Add profiling-specific arguments
    local profile_args="--benchmark_counters_tabular=true --benchmark_report_aggregates_only=false"
    
    run_benchmark "Profile" "$OUTPUT_DIR/profile_${BUILD_PRESET}_${timestamp}.json" \
        "$profile_args"
}

generate_report() {
    print_colored $BLUE "Available result files:"
    find "$OUTPUT_DIR" -name "*.json" -type f | sort
    
    echo ""
    echo "To generate a report from a specific results file, use:"
    echo "  $SCRIPT_DIR/analyze_results.py report --results <file.json> --output <report.md>"
    echo ""
    echo "To compare two result files, use:"
    echo "  $SCRIPT_DIR/analyze_results.py compare --baseline <baseline.json> --optimized <optimized.json>"
}

clean_build() {
    print_colored $YELLOW "Cleaning build directory: $ACTUAL_BUILD_DIR"
    rm -rf "$ACTUAL_BUILD_DIR"
    print_colored $GREEN "Clean completed!"
}

# Main execution
case "$COMMAND" in
    build)
        build_benchmarks
        ;;
    run)
        build_benchmarks
        run_all_benchmarks
        ;;
    compare)
        build_benchmarks
        run_comparison
        ;;
    micro)
        build_benchmarks
        run_micro_benchmarks
        ;;
    enhanced)
        build_benchmarks
        run_enhanced_benchmarks
        ;;
    profile)
        build_benchmarks
        run_profiling
        ;;
    report)
        generate_report
        ;;
    clean)
        clean_build
        ;;
    *)
        print_colored $RED "Error: No command specified or unknown command: $COMMAND"
        print_usage
        exit 1
        ;;
esac
