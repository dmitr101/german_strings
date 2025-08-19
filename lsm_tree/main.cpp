#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>
#include <span>
#include <cstring>
#include <stdexcept>
#include <cstdint>
#include <chrono>

// Linux memory mapping includes
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// Include the german_string header
#include "german_string.h"

// Forward declarations
template <typename StringType>
class SSTable;
template <typename StringType>
class LSMTree;

// Memory-mapped file wrapper for Linux
class MappedFile
{
private:
    int fd_;
    void *data_;
    size_t size_;
    bool writable_;

public:
    MappedFile(const std::string &filename, bool write_mode = false)
        : fd_(-1), data_(nullptr), size_(0), writable_(write_mode)
    {

        int flags = write_mode ? (O_RDWR | O_CREAT) : O_RDONLY;
        mode_t mode = write_mode ? (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) : 0;

        fd_ = open(filename.c_str(), flags, mode);
        if (fd_ == -1)
        {
            throw std::runtime_error("Failed to open file: " + filename + " - " + strerror(errno));
        }

        struct stat sb;
        if (fstat(fd_, &sb) == -1)
        {
            close(fd_);
            throw std::runtime_error("Failed to get file stats: " + std::string(strerror(errno)));
        }

        size_ = sb.st_size;

        if (size_ == 0 && !write_mode)
        {
            // Empty file, nothing to map
            close(fd_);
            fd_ = -1;
            return;
        }

        int prot = write_mode ? (PROT_READ | PROT_WRITE) : PROT_READ;
        int flags_mmap = write_mode ? MAP_SHARED : MAP_PRIVATE;

        data_ = mmap(nullptr, size_, prot, flags_mmap, fd_, 0);
        if (data_ == MAP_FAILED)
        {
            close(fd_);
            throw std::runtime_error("Failed to map file: " + std::string(strerror(errno)));
        }
    }

    // Constructor for creating a new file with a specific size
    MappedFile(const std::string &filename, size_t size, bool use_temp_file = true)
        : fd_(-1), data_(nullptr), size_(size), writable_(true)
    {
        std::string actual_filename = use_temp_file ? filename + ".tmp" : filename;

        // Create and resize the file
        fd_ = open(actual_filename.c_str(), O_CREAT | O_TRUNC | O_RDWR,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fd_ == -1)
        {
            throw std::runtime_error("Failed to create file: " + actual_filename + " - " + strerror(errno));
        }

        if (size > 0)
        {
            // Resize file to needed size
            if (ftruncate(fd_, static_cast<off_t>(size)) == -1)
            {
                close(fd_);
                throw std::runtime_error("Failed to resize file: " + std::string(strerror(errno)));
            }

            // Memory map the file
            data_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
            if (data_ == MAP_FAILED)
            {
                close(fd_);
                throw std::runtime_error("Failed to map file: " + std::string(strerror(errno)));
            }
        }
    }

    ~MappedFile()
    {
        if (data_ != nullptr && data_ != MAP_FAILED)
        {
            munmap(data_, size_);
        }
        if (fd_ >= 0)
        {
            close(fd_);
        }
    }

    // Non-copyable, movable
    MappedFile(const MappedFile &) = delete;
    MappedFile &operator=(const MappedFile &) = delete;

    MappedFile(MappedFile &&other) noexcept
        : fd_(other.fd_), data_(other.data_), size_(other.size_), writable_(other.writable_)
    {
        other.fd_ = -1;
        other.data_ = nullptr;
        other.size_ = 0;
    }

    MappedFile &operator=(MappedFile &&other) noexcept
    {
        if (this != &other)
        {
            if (data_ != nullptr && data_ != MAP_FAILED)
            {
                munmap(data_, size_);
            }
            if (fd_ >= 0)
            {
                close(fd_);
            }

            fd_ = other.fd_;
            data_ = other.data_;
            size_ = other.size_;
            writable_ = other.writable_;

            other.fd_ = -1;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    std::span<const char> data() const
    {
        if (data_ == nullptr || size_ == 0)
        {
            return {};
        }
        return {static_cast<const char *>(data_), size_};
    }

    std::span<char> writable_data()
    {
        if (!writable_ || data_ == nullptr || size_ == 0)
        {
            return {};
        }
        return {static_cast<char *>(data_), size_};
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    bool is_writable() const { return writable_; }

    // Sync data to disk (for writable files)
    void sync()
    {
        if (writable_ && data_ != nullptr && data_ != MAP_FAILED && size_ > 0)
        {
            if (msync(data_, size_, MS_SYNC) == -1)
            {
                throw std::runtime_error("Failed to sync file: " + std::string(strerror(errno)));
            }
        }
    }

    // Static method to create and write structured data atomically
    template <typename StringType>
    static void write_key_value_data(const std::string &filename, const std::map<StringType, StringType> &data)
    {
        // Calculate total size needed for binary format: |key_len|value_len|key|value|
        size_t total_size = 0;
        for (const auto &[key, value] : data)
        {
            total_size += sizeof(uint32_t) + sizeof(uint32_t) + key.size() + value.size();
        }

        if (total_size == 0)
        {
            // Create empty file
            int fd = open((filename + ".tmp").c_str(), O_CREAT | O_TRUNC | O_WRONLY,
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (fd >= 0)
            {
                close(fd);
            }
            std::filesystem::rename(filename + ".tmp", filename);
            return;
        }

        // Create memory-mapped file with the calculated size
        MappedFile mapped_file(filename, total_size, true);

        // Write data in binary format: |key_len|value_len|key|value|
        auto writable_span = mapped_file.writable_data();
        char *ptr = writable_span.data();

        for (const auto &[key, value] : data)
        {
            // Write key length (4 bytes, little endian)
            uint32_t key_len = static_cast<uint32_t>(key.size());
            std::memcpy(ptr, &key_len, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            // Write value length (4 bytes, little endian)
            uint32_t value_len = static_cast<uint32_t>(value.size());
            std::memcpy(ptr, &value_len, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            // Write key data
            std::memcpy(ptr, key.data(), key.size());
            ptr += key.size();

            // Write value data
            std::memcpy(ptr, value.data(), value.size());
            ptr += value.size();
        }

        // Ensure data is written to disk
        mapped_file.sync();

        // The destructor will clean up the memory mapping and close the file descriptor
        // Now we need to atomically rename the temp file to the final filename
        std::filesystem::rename(filename + ".tmp", filename);
    }
};

// MemTable: In-memory sorted storage
template <typename StringType>
class MemTable
{
private:
    std::map<StringType, StringType> data_;
    size_t size_threshold_;
    size_t current_size_;

public:
    explicit MemTable(size_t threshold = 1024 * 1024) // 1MB default
        : size_threshold_(threshold), current_size_(0)
    {
    }

    void put(StringType &&key, StringType &&value)
    {
        auto old_size = data_.count(key) ? data_[key].size() : 0;
        auto emplace_result = data_.try_emplace(key, std::move(value));
        if (!emplace_result.second)
        {
            // Key already exists, update the value
            emplace_result.first->second = std::move(value);
        }
        current_size_ = current_size_ - old_size + key.size() + value.size();
    }

    std::optional<StringType> get(const StringType &key) const
    {
        auto as_transient = []<typename T>(const T &str) -> T
        {
            if constexpr (std::is_same_v<T, gs::german_string>)
            {
                return str.as_transient();
            }
            else
            {
                return str;
            }
        };

        auto it = data_.find(key);
        if (it != data_.end())
        {
            return as_transient(it->second);
        }
        return std::nullopt;
    }

    bool is_full() const
    {
        return current_size_ >= size_threshold_;
    }

    bool empty() const
    {
        return data_.empty();
    }

    size_t size() const
    {
        return data_.size();
    }

    // Get all data for flushing to SSTable
    std::map<StringType, StringType> get_all_data() const
    {
        return data_;
    }

    void clear()
    {
        data_.clear();
        current_size_ = 0;
    }
};

// SSTable: Immutable sorted string table on disk using memory-mapped files
template <typename StringType>
class SSTable
{
private:
    std::string filename_;
    mutable std::map<StringType, StringType> data_cache_; // Cache for parsed data
    mutable bool cache_loaded_;
    mutable std::unique_ptr<MappedFile> mapped_file_; // Persistent mapping
    int level_;

    // Parse memory-mapped file data into cache
    void load_cache() const
    {
        if (cache_loaded_)
        {
            return;
        }

        try
        {
            // Create and keep the mapped file instance
            mapped_file_ = std::make_unique<MappedFile>(filename_, false);
            if (mapped_file_->empty())
            {
                cache_loaded_ = true;
                return;
            }

            auto data_span = mapped_file_->data();
            parse_data_from_span(data_span);
            cache_loaded_ = true;
        }
        catch (const std::exception &e)
        {
            // File doesn't exist or can't be read, leave cache empty
            // Reset the mapped file pointer on error
            mapped_file_.reset();
            cache_loaded_ = true;
        }
    }

    void parse_data_from_span(std::span<const char> data_span) const
    {
        const char *ptr = data_span.data();
        const char *end = ptr + data_span.size();

        while (ptr + sizeof(uint32_t) * 2 <= end)
        {
            // Read key length (4 bytes, little endian)
            uint32_t key_len;
            std::memcpy(&key_len, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            // Read value length (4 bytes, little endian)
            uint32_t value_len;
            std::memcpy(&value_len, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            // Check if we have enough data for key and value
            if (ptr + key_len + value_len > end)
            {
                // Corrupted data, stop parsing
                break;
            }

            // Read key data
            StringType key;
            if constexpr (std::is_same_v<StringType, std::string>)
            {
                key = StringType(ptr, key_len);
            }
            else
            {
                key = StringType(ptr, key_len, gs::string_class::transient);
            }
            ptr += key_len;

            // Read value data
            StringType value;
            if constexpr (std::is_same_v<StringType, std::string>)
            {
                value = StringType(ptr, value_len);
            }
            else
            {
                value = StringType(ptr, value_len, gs::string_class::transient);
            }
            ptr += value_len;

            data_cache_[key] = value;
        }
    }

public:
    SSTable(const std::string &filename, int level = 0)
        : filename_(filename), cache_loaded_(false), mapped_file_(nullptr), level_(level) {}

    // Create SSTable from MemTable data using memory-mapped files
    static std::unique_ptr<SSTable> create_from_memtable(
        const std::map<StringType, StringType> &data,
        const std::string &filename,
        int level = 0)
    {
        // Write data using unified memory-mapped file
        MappedFile::write_key_value_data(filename, data);

        auto sstable = std::make_unique<SSTable>(filename, level);
        // Pre-populate cache since we have the data
        sstable->data_cache_ = data;
        sstable->cache_loaded_ = true;

        return sstable;
    }

    std::optional<StringType> get(const StringType &key) const
    {
        load_cache();
        auto it = data_cache_.find(key);
        if (it != data_cache_.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    const std::map<StringType, StringType> &get_all_data() const
    {
        load_cache();
        return data_cache_;
    }

    const std::string &get_filename() const
    {
        return filename_;
    }

    int get_level() const
    {
        return level_;
    }

    size_t size() const
    {
        load_cache();
        return data_cache_.size();
    }

    // Force reload from disk (useful for testing)
    void reload_from_disk() const
    {
        cache_loaded_ = false;
        data_cache_.clear();
        mapped_file_.reset(); // Release the existing mapping
        load_cache();
    }

    // Check if the file is currently memory-mapped
    bool is_mapped() const
    {
        return mapped_file_ != nullptr;
    }

    // Release the memory mapping (but keep the cache)
    void release_mapping() const
    {
        mapped_file_.reset();
    }
};

// LSM Tree implementation
template <typename StringType>
class LSMTree
{
private:
    MemTable<StringType> memtable_;
    std::vector<std::unique_ptr<SSTable<StringType>>> sstables_;
    std::string base_dir_;
    int next_sstable_id_;

public:
    explicit LSMTree(const std::string &base_dir = "./lsm_data")
        : base_dir_(base_dir), next_sstable_id_(0)
    {

        // Create directory if it doesn't exist
        std::filesystem::create_directories(base_dir_);

        // Load existing SSTables
        load_existing_sstables();
    }

    void put(StringType key, StringType value)
    {
        // Insert into MemTable
        memtable_.put(std::move(key), std::move(value));

        // Check if MemTable is full and needs to be flushed
        if (memtable_.is_full())
        {
            flush_memtable();
        }
    }

    std::optional<StringType> get(const StringType &key)
    {
        // First check MemTable (most recent data)
        auto result = memtable_.get(key);
        if (result.has_value())
        {
            return result;
        }

        // Then check SSTables in reverse order (newest first)
        for (auto it = sstables_.rbegin(); it != sstables_.rend(); ++it)
        {
            result = (*it)->get(key);
            if (result.has_value())
            {
                return result;
            }
        }

        return std::nullopt;
    }

    void delete_key(const StringType &key)
    {
        // In LSM-trees, deletion is implemented as putting a tombstone marker
        // For simplicity, we'll use an empty string as the tombstone
        // In a real implementation, you'd use a special marker to distinguish
        // between empty values and deleted keys
        put(key, ""); // Empty value acts as tombstone
    }

    void flush_memtable()
    {
        if (memtable_.empty())
        {
            return;
        }

        // Create new SSTable from MemTable data
        std::string filename = base_dir_ + "/sstable_" + std::to_string(next_sstable_id_++) + ".dat";
        auto sstable = SSTable<StringType>::create_from_memtable(memtable_.get_all_data(), filename);

        sstables_.push_back(std::move(sstable));
        memtable_.clear();

        // Trigger compaction if needed
        if (should_compact())
        {
            compact();
        }
    }

    bool should_compact() const
    {
        // Simple compaction strategy: compact when we have more than 4 SSTables
        return sstables_.size() > 4;
    }

    void compact()
    {
        if (sstables_.size() < 2)
        {
            return;
        }

        std::cout << "Compacting " << sstables_.size() << " SSTables...\n";

        // Merge all SSTables into one
        std::map<StringType, StringType> merged_data;

        for (const auto &sstable : sstables_)
        {
            const auto &data = sstable->get_all_data();
            for (const auto &[key, value] : data)
            {
                merged_data[key] = value; // Later values overwrite earlier ones
            }
        }

        // Create new compacted SSTable
        std::string filename = base_dir_ + "/compacted_" + std::to_string(next_sstable_id_++) + ".dat";
        auto compacted_sstable = SSTable<StringType>::create_from_memtable(merged_data, filename, 1);

        // Delete old SSTable files before clearing the vector
        for (const auto &sstable : sstables_)
        {
            const std::string &old_filename = sstable->get_filename();
            try
            {
                if (std::filesystem::remove(old_filename))
                {
                    std::cout << "Deleted old SSTable file: " << old_filename << "\n";
                }
                else
                {
                    std::cerr << "Warning: Failed to delete old SSTable file: " << old_filename << "\n";
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Warning: Error deleting old SSTable file " << old_filename << ": " << e.what() << "\n";
            }
        }

        // Remove old SSTables from memory
        sstables_.clear();
        sstables_.push_back(std::move(compacted_sstable));

        std::cout << "Compaction completed. Merged into 1 SSTable.\n";
    }

    void load_existing_sstables()
    {
        // Scan directory for existing SSTable files and load them
        if (!std::filesystem::exists(base_dir_))
        {
            return;
        }

        std::vector<std::string> sstable_files;

        for (const auto &entry : std::filesystem::directory_iterator(base_dir_))
        {
            if (entry.is_regular_file())
            {
                const std::string filename = entry.path().filename().string();
                if (filename.ends_with(".dat"))
                {
                    sstable_files.push_back(entry.path().string());
                }
            }
        }

        // Sort files to ensure consistent loading order
        std::sort(sstable_files.begin(), sstable_files.end());

        // Load each SSTable
        for (const auto &filepath : sstable_files)
        {
            try
            {
                auto sstable = std::make_unique<SSTable<StringType>>(filepath, 0);
                // Test that the file can be read
                sstable->size(); // This will trigger cache loading
                sstables_.push_back(std::move(sstable));

                // Update next_sstable_id_ to avoid conflicts
                std::string filename = std::filesystem::path(filepath).filename().string();
                if (filename.starts_with("sstable_") || filename.starts_with("compacted_"))
                {
                    // Extract number from filename
                    size_t underscore = filename.find('_');
                    size_t dot = filename.find('.', underscore);
                    if (underscore != std::string::npos && dot != std::string::npos)
                    {
                        try
                        {
                            int file_id = std::stoi(filename.substr(underscore + 1, dot - underscore - 1));
                            next_sstable_id_ = std::max(next_sstable_id_, file_id + 1);
                        }
                        catch (...)
                        {
                            // Ignore parsing errors
                        }
                    }
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Warning: Failed to load SSTable " << filepath << ": " << e.what() << "\n";
            }
        }

        if (!sstables_.empty())
        {
            std::cout << "Loaded " << sstables_.size() << " existing SSTables\n";
        }
    }

    // Debug function to print current state
    void print_stats() const
    {
        std::cout << "LSM Tree Stats:\n";
        std::cout << "  MemTable size: " << memtable_.size() << " entries\n";
        std::cout << "  SSTables count: " << sstables_.size() << "\n";
        for (size_t i = 0; i < sstables_.size(); ++i)
        {
            std::cout << "    SSTable " << i << ": " << sstables_[i]->size()
                      << " entries (" << sstables_[i]->get_filename() << ")\n";
        }
    }
};

// CSV parsing helper function
template <typename StringType>
std::pair<StringType, StringType> parse_csv_line(const std::string &line)
{
    std::string key, value;
    size_t comma_pos = line.find(';');

    if (comma_pos == std::string::npos)
    {
        // No comma found, treat entire line as key with empty value
        key = line;
        value = "";
    }
    else
    {
        key = line.substr(0, comma_pos);
        value = line.substr(comma_pos + 1);
    }

    // Simple CSV unescaping - remove surrounding quotes if present
    auto trim_quotes = [](std::string &s)
    {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        {
            s = s.substr(1, s.size() - 2);
        }
    };

    trim_quotes(key);
    trim_quotes(value);

    // Convert to target string type
    if constexpr (std::is_same_v<StringType, std::string>)
    {
        return {key, value};
    }
    else
    {
        auto result = std::make_pair(StringType(key), StringType(value));
        return result;
    }
}

// Bulk ingest data from CSV file
template <typename StringType>
void bulk_ingest_csv(const std::string &csv_filename, const std::string &lsm_dir = "./lsm_data")
{
    std::cout << "=== CSV Bulk Ingestion ===\n";
    std::cout << "Reading from: " << csv_filename << "\n";
    std::cout << "LSM directory: " << lsm_dir << "\n\n";

    // Open CSV file
    std::ifstream file(csv_filename);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open CSV file: " + csv_filename);
    }

    // Create LSM tree
    LSMTree<StringType> lsm(lsm_dir);

    // Statistics
    size_t line_count = 0;
    size_t processed_count = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    std::string line;
    while (std::getline(file, line))
    {
        line_count++;

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        try
        {
            auto [key, value] = parse_csv_line<StringType>(line);
            if (!key.empty())
            {
                lsm.put(key, value);
                processed_count++;

                // Progress report every 10000 records
                if (processed_count % 10000 == 0)
                {
                    std::cout << "Processed " << processed_count << " records...\n";
                    lsm.print_stats();
                    std::cout << "\n";
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Warning: Failed to process line " << line_count
                      << ": " << e.what() << "\n";
        }
    }

    // Final flush to ensure all data is persisted
    lsm.flush_memtable();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "=== Ingestion Complete ===\n";
    std::cout << "Total lines read: " << line_count << "\n";
    std::cout << "Records processed: " << processed_count << "\n";
    std::cout << "Time taken: " << duration.count() << " ms\n";
    auto duration_count = duration.count();
    std::cout << "Records per second: " << (duration_count > 0 ? processed_count * 1000 / duration_count : 0) << "\n\n";

    lsm.print_stats();
}

// Interactive query mode
template <typename StringType>
void interactive_query(const std::string &lsm_dir = "./lsm_data")
{
    std::cout << "=== Interactive Query Mode ===\n";
    std::cout << "Type keys to query, 'stats' for statistics, or 'quit' to exit.\n\n";

    LSMTree<StringType> lsm(lsm_dir);
    lsm.print_stats();
    std::cout << "\n";

    std::string input;
    while (true)
    {
        std::cout << "query> ";
        if (!std::getline(std::cin, input))
        {
            break; // EOF
        }

        if (input == "quit" || input == "exit")
        {
            break;
        }

        if (input == "stats")
        {
            lsm.print_stats();
            continue;
        }

        if (input.empty())
        {
            continue;
        }

        StringType query_key;
        if constexpr (std::is_same_v<StringType, std::string>)
        {
            query_key = input;
        }
        else
        {
            query_key = StringType(input.c_str(), input.size(), gs::string_class::persistent);
        }
        auto result = lsm.get(query_key);
        if (result.has_value())
        {
            if constexpr (std::is_same_v<StringType, std::string>)
            {
                std::cout << "Found: " << result.value() << "\n";
            }
            else
            {
                // For gs::german_string, convert to std::string for output
                std::string output_str(result.value().data(), result.value().size());
                std::cout << "Found: " << output_str << "\n";
            }
        }
        else
        {
            std::cout << "Not found\n";
        }
    }

    std::cout << "Goodbye!\n";
}

// Demo function
template <typename StringType>
void demo_lsm_tree()
{
    std::cout << "=== LSM Tree Demo ===\n\n";

    LSMTree<StringType> lsm;

    // Insert some data
    std::cout << "Inserting data...\n";
    lsm.put("apple", "red fruit");
    lsm.put("banana", "yellow fruit");
    lsm.put("cherry", "red fruit");
    lsm.put("date", "sweet fruit");
    lsm.put("elderberry", "small fruit");

    lsm.print_stats();
    std::cout << "\n";

    // Query some data
    std::cout << "Querying data:\n";
    auto result = lsm.get("apple");
    if (result.has_value())
    {
        std::cout << "apple: " << result.value() << "\n";
    }

    result = lsm.get("banana");
    if (result.has_value())
    {
        std::cout << "banana: " << result.value() << "\n";
    }

    result = lsm.get("nonexistent");
    if (!result.has_value())
    {
        std::cout << "nonexistent: not found\n";
    }

    std::cout << "\n";

    // Add more data to trigger flushes
    std::cout << "Adding more data to trigger MemTable flush...\n";
    for (int i = 0; i < 100; ++i)
    {
        lsm.put("key" + std::to_string(i), "value" + std::to_string(i));
    }

    lsm.print_stats();
    std::cout << "\n";

    // Force flush
    std::cout << "Forcing MemTable flush...\n";
    lsm.flush_memtable();
    lsm.print_stats();

    std::cout << "\n";

    // Add even more data to trigger compaction
    std::cout << "Adding more data to trigger compaction...\n";
    for (int i = 100; i < 500; ++i)
    {
        lsm.put("key" + std::to_string(i), "value" + std::to_string(i));
    }

    // Force multiple flushes
    for (int i = 0; i < 5; ++i)
    {
        lsm.flush_memtable();
        for (int j = 0; j < 20; ++j)
        {
            lsm.put("batch" + std::to_string(i) + "_key" + std::to_string(j),
                    "batch" + std::to_string(i) + "_value" + std::to_string(j));
        }
    }

    lsm.print_stats();
    std::cout << "\n";

    // Test retrieval after compaction
    std::cout << "Testing retrieval after operations:\n";
    result = lsm.get("key50");
    if (result.has_value())
    {
        std::cout << "key50: " << result.value() << "\n";
    }

    result = lsm.get("batch2_key10");
    if (result.has_value())
    {
        std::cout << "batch2_key10: " << result.value() << "\n";
    }

    std::cout << "\n";

    // Test persistence by creating a new LSM tree instance
    std::cout << "Testing persistence with new LSM tree instance:\n";
    {
        LSMTree<std::string> lsm2; // Should load existing SSTables
        lsm2.print_stats();

        // Test that we can still retrieve data
        auto persistent_result = lsm2.get("apple");
        if (persistent_result.has_value())
        {
            std::cout << "apple (from persistent storage): " << persistent_result.value() << "\n";
        }

        persistent_result = lsm2.get("key50");
        if (persistent_result.has_value())
        {
            std::cout << "key50 (from persistent storage): " << persistent_result.value() << "\n";
        }
    }
}

void print_usage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " [string_type] [command] [options]\n\n";
    std::cout << "string_type: 'std' for std::string, 'gs' for gs::german_string\n";
    std::cout << "Commands:\n";
    std::cout << "  demo                    Run the built-in demo\n";
    std::cout << "  ingest <csv_file>       Bulk ingest data from CSV file\n";
    std::cout << "  query                   Interactive query mode\n";
    std::cout << "  get <key>               Get value for a specific key\n";
    std::cout << "  delete <key>            Delete a key (tombstone)\n\n";
    std::cout << "Options:\n";
    std::cout << "  --dir <directory>       LSM data directory (default: ./lsm_data)\n\n";
    std::cout << "CSV Format:\n";
    std::cout << "  key;value\n";
    std::cout << "  \"key with spaces\";\"value with spaces\"\n";
    std::cout << "  Lines starting with # are treated as comments\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " demo\n";
    std::cout << "  " << program_name << " ingest data.csv\n";
    std::cout << "  " << program_name << " ingest data.csv --dir /path/to/lsm\n";
    std::cout << "  " << program_name << " query --dir /path/to/lsm\n";
    std::cout << "  " << program_name << " get mykey\n";
}

template <typename StringType>
int template_main(int argc, char *argv[])
{
    try
    {
        std::string command = argv[2];
        std::string lsm_dir = "./lsm_data";

        // Parse common options
        for (int i = 2; i < argc; i++)
        {
            if (std::string(argv[i]) == "--dir" && i + 1 < argc)
            {
                lsm_dir = argv[i + 1];
                i++; // Skip the next argument since we consumed it
            }
        }

        if (command == "demo")
        {
            demo_lsm_tree<StringType>();
        }
        else if (command == "ingest")
        {
            if (argc < 4)
            {
                std::cerr << "Error: CSV file not specified\n";
                print_usage(argv[0]);
                return 1;
            }
            std::string csv_file = argv[3];
            bulk_ingest_csv<StringType>(csv_file, lsm_dir);
        }
        else if (command == "query")
        {
            interactive_query<StringType>(lsm_dir);
        }
        else if (command == "get")
        {
            if (argc < 4)
            {
                std::cerr << "Error: Key not specified\n";
                print_usage(argv[0]);
                return 1;
            }
            std::string key = argv[3];
            LSMTree<std::string> lsm(lsm_dir);
            auto result = lsm.get(key);
            if (result.has_value())
            {
                std::cout << result.value() << "\n";
                return 0;
            }
            else
            {
                std::cerr << "Key not found: " << key << "\n";
                return 1;
            }
        }
        else if (command == "delete")
        {
            if (argc < 4)
            {
                std::cerr << "Error: Key not specified\n";
                print_usage(argv[0]);
                return 1;
            }
            std::string key = argv[3];
            LSMTree<StringType> lsm(lsm_dir);
            lsm.delete_key(key);
            lsm.flush_memtable(); // Ensure the tombstone is persisted
            std::cout << "Key deleted: " << key << "\n";
            return 0;
        }
        else
        {
            std::cerr << "Error: Unknown command '" << command << "'\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        print_usage(argv[0]);
        return 1;
    }

    std::string string_type = argv[1];
    if (string_type == "std")
    {
        return template_main<std::string>(argc, argv);
    }
    else if (string_type == "gs")
    {
        return template_main<gs::german_string>(argc, argv);
    }
    else
    {
        std::cerr << "Error: Unknown string type '" << string_type << "'\n";
        print_usage(argv[0]);
        return 1;
    }
}