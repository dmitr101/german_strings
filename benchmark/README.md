# German Strings Benchmark Suite

This directory contains a comprehensive benchmark suite for evaluating the performance of the `german_string` implementation compared to `std::string` and other alternatives.

## Analysis Tools

### `analyze_results.py` - Enhanced Performance Analysis

A comprehensive tool for analyzing Google Benchmark JSON results with three main commands:

#### 1. Report Generation
```bash
python3 analyze_results.py report --results <benchmark_results.json> [--output <report.md>]
```
Generates a detailed markdown report with system information and performance data.

#### 2. Performance Comparison
```bash
python3 analyze_results.py compare --baseline <baseline.json> --optimized <optimized.json>
```
Compares two benchmark result files and calculates speedup ratios.

#### 3. Graph Generation ✨ NEW ✨
```bash
python3 analyze_results.py graph --results <benchmark_results.json> [--output <graphs_directory>]
```
Generates performance visualization graphs with:
- **Individual test type graphs**: Time vs Input Size for each benchmark family
- **Performance comparison**: std::string vs gs::german_string side-by-side  
- **Summary overview**: Combined view of all major test types
- **Smart scaling**: Automatic log scaling for wide data ranges
- **Performance annotations**: Shows percentage improvements

**Requirements**: `pip install matplotlib`

### Generated Graph Outputs
- `StringConstruction_performance.png` - String construction benchmarks
- `StringSorting_performance.png` - Sorting algorithm performance  
- `StringEqualityComparison_performance.png` - Equality comparison speed
- `StringLexicographicComparison_performance.png` - Lexicographic comparison
- `StringComparisonByLength_performance.png` - Length-based comparisons
- `GermanStringPrefixComparison_performance.png` - Prefix optimization analysis
- `GermanStringSmallStringBoundary_performance.png` - Small string optimization
- `performance_summary.png` - Combined overview of all major benchmarks

## Benchmark Files

### 1. `benchmark.cpp` (Original)
The original benchmark file focusing on:
- String equality comparison performance
- Sorting performance comparison
- Basic performance characteristics

### 2. `enhanced_benchmark.cpp` (New)
Comprehensive benchmarks covering realistic usage scenarios:

#### Construction Benchmarks
- **String Construction**: Measures construction time from C strings
- **Copy Construction**: Evaluates copy performance for different string lengths

#### Container Performance
- **Hash Map Operations**: Insertion and lookup performance in `std::unordered_map`
- **Set Operations**: Insertion performance in `std::set` (ordered containers)
- **Vector Operations**: Performance when storing strings in vectors

#### Comparison Scenarios
- **Length-based Comparisons**: Performance across different string lengths
- **Search Operations**: Substring search and pattern matching

#### Memory and Cache Performance
- **Vector Resize**: Memory allocation patterns during dynamic growth
- **Cache Effects**: Sequential vs random access patterns

#### German String Specific
- **String Class Types**: Performance comparison between temporary, persistent, and transient string classes

### 3. `micro_benchmark.cpp` (New)
Micro-benchmarks focusing on specific `german_string` optimizations:

#### Small String Optimization
- **Boundary Testing**: Performance around the 12-byte small string boundary
- **Cache Performance**: Memory layout effects on performance

#### Prefix Optimization
- **Prefix Comparison**: Performance benefits of prefix-based comparison
- **Shared Prefix Scenarios**: Different lengths of shared prefixes

#### Equality Optimization
- **Length-based Early Exit**: Performance when strings have different lengths
- **Hash Performance**: Hash function efficiency

#### String Class Impact
- **Class Type Performance**: Overhead of different string class types
- **Memory Management**: Allocation patterns for different classes

## Analysis Tools

### `analyze_results.py`
Python script for analyzing and comparing benchmark results:

```bash
# Generate a detailed report from benchmark results
./analyze_results.py report --results results.json --output report.md

# Compare two benchmark runs
./analyze_results.py compare --baseline std_results.json --optimized german_results.json
```

### `run_benchmarks.sh`
Comprehensive benchmark runner with multiple execution modes:

```bash
# Build and run all benchmarks
./run_benchmarks.sh run

# Run only comparison benchmarks
./run_benchmarks.sh compare

# Run micro-benchmarks
./run_benchmarks.sh micro

# Run enhanced realistic scenarios
./run_benchmarks.sh enhanced

# Run with profiling data
./run_benchmarks.sh profile

# Generate reports from existing results
./run_benchmarks.sh report
```

## Benchmark Categories

### 1. **Construction Performance**
Measures the cost of creating strings from various sources:
- From C strings of different lengths
- Copy construction overhead
- Different string class types (temporary, persistent, transient)

### 2. **Comparison Performance** 
Evaluates string comparison efficiency:
- Equality checks with early termination
- Lexicographic ordering for sorting
- Prefix-based optimization benefits
- Cache-friendly comparison patterns

### 3. **Container Integration**
Tests performance in standard containers:
- Hash table performance (insertion, lookup)
- Ordered containers (std::set, std::map)
- Vector storage and manipulation
- Memory allocation patterns

### 4. **Memory Characteristics**
Analyzes memory usage and patterns:
- Small string optimization effectiveness
- Memory layout effects on cache performance
- Allocation overhead for different string classes
- Sequential vs random access patterns

### 5. **Real-world Scenarios**
Benchmarks based on common usage patterns:
- URL and path string handling
- Log message processing
- Configuration key-value pairs
- String interning scenarios

## Key Performance Metrics

Based on initial results, `german_string` shows significant improvements:

### String Comparison
- **11-26% faster** equality comparisons
- **30-84% faster** sorting operations
- Excellent performance for duplicate detection

### Memory Efficiency
- Reduced memory allocations for string interning
- Better cache locality for comparison operations
- Optimized prefix comparison reduces full string scans

### Container Performance
- Faster hash map operations due to optimized hash functions
- Improved set operations through better comparison performance
- Efficient vector storage with reduced memory overhead

## Usage Examples

### Running Basic Comparisons
```bash
# Build and run comparison benchmarks
./run_benchmarks.sh compare --repetitions 5 --format json

# Generate detailed report
./analyze_results.py report --results comparison_results.json --output comparison_report.md
```

### Analyzing Specific Features
```bash
# Test small string optimization
./run_benchmarks.sh micro --filter "SmallString|Boundary"

# Test prefix optimization
./run_benchmarks.sh micro --filter "Prefix"

# Test hash performance
./run_benchmarks.sh enhanced --filter "HashMap"
```

### Performance Profiling
```bash
# Run with detailed profiling
./run_benchmarks.sh profile --repetitions 10

# Compare different build configurations
./run_benchmarks.sh run --compiler clang --stdlib libcxx
./run_benchmarks.sh run --compiler gcc --stdlib libstdcxx
```

## Interpreting Results

### Time Measurements
- **ns (nanoseconds)**: For very fast operations
- **μs (microseconds)**: For typical string operations  
- **ms (milliseconds)**: For bulk operations
- **s (seconds)**: For large dataset operations

### Performance Indicators
- **Speedup Ratio**: How much faster german_string is (e.g., "2.5x faster")
- **Items/Second**: Throughput for operations like insertions or comparisons
- **Iterations**: Number of times the benchmark was executed for accuracy

### Key Metrics to Watch
1. **Construction time** - Overhead of creating strings
2. **Comparison speed** - Critical for sorting and searching
3. **Hash performance** - Important for hash table operations
4. **Memory usage** - Affects cache performance and overall efficiency
5. **Container integration** - Real-world usage patterns

## Customization

### Adding New Benchmarks
1. Add benchmark functions to the appropriate file
2. Use the `BENCHMARK` or `BENCHMARK_TEMPLATE` macros
3. Follow the naming convention for automatic categorization

### Benchmark Parameters
- Adjust string lengths, counts, and patterns in the generation functions
- Modify the `Args()` calls to test different scenarios
- Add new string patterns to `COMMON_PREFIXES` and `COMMON_SUFFIXES`

### Analysis Scripts
- Extend `analyze_results.py` for custom metrics
- Modify `run_benchmarks.sh` for additional execution modes
- Add new comparison patterns and filters

This comprehensive benchmark suite provides deep insights into `german_string` performance characteristics and helps validate the optimization strategies used in the implementation.
