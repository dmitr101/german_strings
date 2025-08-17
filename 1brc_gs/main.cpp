#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>
#include <span>
#include <charconv>

#include "german_string.h"

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// A move-only helper
template <typename T, T empty = T{} >
struct MoveOnly
{
    MoveOnly() : store_(empty) {}
    MoveOnly(T value) : store_(value) {}
    MoveOnly(MoveOnly&& other) : store_(std::exchange(other.store_, empty)) {}
    MoveOnly& operator=(MoveOnly&& other)
    {
        store_ = std::exchange(other.store_, empty);
        return *this;
    }
    operator T() const { return store_; }
    T get() const { return store_; }

private:
    T store_;
};

struct FileFD
{
#ifdef _WIN32
    FileFD(const std::filesystem::path& file_path)
    {
        // Convert path to wide string for Windows Unicode support
        std::wstring wide_path = file_path.wstring();

        fd_ = CreateFileW(
            wide_path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (fd_ == INVALID_HANDLE_VALUE)
        {
            DWORD error = GetLastError();
            throw std::system_error(static_cast<int>(error), std::system_category(),
                "Failed to open file");
        }
    }

    ~FileFD()
    {
        if (fd_ != INVALID_HANDLE_VALUE)
            CloseHandle(fd_);
    }

    HANDLE get() const { return fd_.get(); }

private:
    MoveOnly<HANDLE, INVALID_HANDLE_VALUE> fd_;
#else
    FileFD(const std::filesystem::path& file_path)
        : fd_(open(file_path.c_str(), O_RDONLY))
    {
        if (fd_ == -1)
            throw std::system_error(errno, std::system_category(),
                "Failed to open file");
    }

    ~FileFD()
    {
        if (fd_ >= 0)
            close(fd_);
    }

    int get() const { return fd_.get(); }

private:
    MoveOnly<int, -1> fd_;
#endif
};

struct MappedFile
{
#ifdef _WIN32
    MappedFile(const std::filesystem::path& file_path) : fd_(file_path)
    {
        // Get file size
        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(fd_.get(), &file_size))
        {
            DWORD error = GetLastError();
            throw std::system_error(static_cast<int>(error), std::system_category(),
                "Failed to get file size");
        }
        sz_ = static_cast<size_t>(file_size.QuadPart);

        // Create file mapping
        mapping_ = CreateFileMappingW(
            fd_.get(),
            nullptr,
            PAGE_READONLY,
            0,  // High-order DWORD of maximum size
            0,  // Low-order DWORD of maximum size (0 = entire file)
            nullptr  // Mapping object name
        );

        if (mapping_ == nullptr)
        {
            DWORD error = GetLastError();
            throw std::system_error(static_cast<int>(error), std::system_category(),
                "Failed to create file mapping");
        }

        // Map view of file
        begin_ = static_cast<char*>(MapViewOfFile(
            mapping_,
            FILE_MAP_READ,
            0,  // High-order DWORD of file offset
            0,  // Low-order DWORD of file offset
            0   // Number of bytes to map (0 = entire file)
        ));

        if (begin_ == nullptr)
        {
            DWORD error = GetLastError();
            CloseHandle(mapping_);
            throw std::system_error(static_cast<int>(error), std::system_category(),
                "Failed to map view of file");
        }
    }

    ~MappedFile()
    {
        if (begin_ != nullptr)
            UnmapViewOfFile(begin_.get());
        if (mapping_ != nullptr)
            CloseHandle(mapping_);
    }

    // The entire file content as a std::span
    std::span<const char> data() const { return { begin_.get(), sz_.get() }; }

private:
    FileFD fd_;
    HANDLE mapping_ = nullptr;
    MoveOnly<char*> begin_;
    MoveOnly<size_t> sz_;
#else
    MappedFile(const std::filesystem::path& file_path) : fd_(file_path)
    {
        // Determine the filesize (needed for mmap)
        struct stat sb;
        if (fstat(fd_.get(), &sb) == -1)
            throw std::system_error(errno, std::system_category(),
                "Failed to read file stats");
        sz_ = sb.st_size;

        begin_ = static_cast<char*>
            (mmap(nullptr, sz_, PROT_READ, MAP_PRIVATE, fd_.get(), 0));
        if (begin_ == MAP_FAILED)
            throw std::system_error(errno, std::system_category(),
                "Failed to map file to memory");
    }

    ~MappedFile()
    {
        if (begin_ != nullptr)
            munmap(begin_, sz_);
    }

    // The entire file content as a std::span
    std::span<const char> data() const { return { begin_.get(), sz_.get() }; }

private:
    FileFD fd_;
    MoveOnly<char*> begin_;
    MoveOnly<size_t> sz_;
#endif
};

struct Record
{
    uint64_t cnt;
    double sum;

    float min;
    float max;
};

using DB = std::unordered_map<gs::german_string, Record>;

template <typename String>
bool getline(std::span<const char> &data, String &line, char delim = '\n')
{
    auto pos = std::ranges::find(data, delim);
    line = String{data.begin(), pos};
    data = data.subspan(std::distance(data.begin(), pos) + 1);
    if (pos != data.end())
        return true;
    return false;
}

// NOTE(dshynkar): Also had to simplify as to not use spanstreams
DB process_input(std::span<const char> data)
{
    DB db;

    gs::german_string station;
    gs::german_string value;

    // Grab the station and the measured value from the input
    while (getline(data, station, ';') && getline(data, value, '\n'))
    {
        // Convert the measured value into a floating point
        float fp_value = gs::stof(value);

        // Lookup the station in our database
        auto it = db.find(station);
        if (it == db.end())
        {
            // If it's not there, insert
            db.emplace(station, Record{1, fp_value, fp_value, fp_value});
            continue;
        }
        // Otherwise update the information
        it->second.min = std::min(it->second.min, fp_value);
        it->second.max = std::max(it->second.max, fp_value);
        it->second.sum += fp_value;
        ++it->second.cnt;
    }

    return db;
}

void format_output(std::ostream &out, const DB &db)
{
    std::vector<gs::german_string> names(db.size());
    // Grab all the unique station names
    std::ranges::copy(db | std::views::keys, names.begin());
    // Sorting UTF-8 strings lexicographically is the same
    // as sorting by codepoint value
    std::ranges::sort(names);

    std::string delim = "";

    out << std::setiosflags(out.fixed | out.showpoint) << std::setprecision(1);
    out << "{";
    for (auto &k : names | std::ranges::views::take(10))
    {
        // Print StationName:min/avg/max
        auto &[_, record] = *db.find(k);
        out << std::exchange(delim, ", ") << k.as_string_view() << "=" << record.min << "/"
            << (record.sum / record.cnt) << "/" << record.max;
    }
    out << "}\n";
}

// Had to modify the code a bit because spanstreams do not have a wide compiler suuport yet
// Not in the latest Clang, nor in GCC 13

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <input_file>\n";
        return 1;
    }

    try
    {
        MappedFile mfile(argv[1]);
        auto db = process_input(mfile.data());
        format_output(std::cout, db);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}