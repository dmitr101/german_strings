#pragma once

#include <memory>
#include <string_view>
#include <string>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <compare>
#include <concepts>
#include <stdexcept>
#include <limits>
#include <bit>

// TODO: I'm passing a lot by const reference, but I should be passing by value maybe???
// TODO: A constructor withj size type being size_t and check if the size fits in 32 bits and panic if it doesn't

// define per-compiler macros for force inline
#ifdef _MSC_VER
#define GS_FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define GS_FORCEINLINE inline __attribute__((always_inline))
#else
#define GS_FORCEINLINE inline
#endif

namespace gs
{
    enum class string_class : uint8_t
    {
        temporary = 0,
        persistent = 1,
        transient = 2,
    };

    struct temporary_t
    {
        constexpr inline operator string_class() const
        {
            return string_class::temporary;
        }
    };

    struct persistent_t
    {
        constexpr inline operator string_class() const
        {
            return string_class::persistent;
        }
    };

    struct transient_t
    {
        constexpr inline operator string_class()
        {
            return string_class::transient;
        }
    };

    namespace detail
    {

        GS_FORCEINLINE int prefix_memcmp(std::uint32_t a, std::uint32_t b, int n)
        {
            std::uint32_t diff = a ^ b;
            std::uint32_t mask = (std::uint32_t)((0xFFFFFFFFull << (n * 8)) >> 32);
            diff &= mask;

            int first_diff = std::countr_zero(diff) / 8;
            std::uint8_t byte_a = ((std::uint64_t)a >> (first_diff * 8)) & 0xFF;
            std::uint8_t byte_b = ((std::uint64_t)b >> (first_diff * 8)) & 0xFF;
            return (int)byte_a - (int)byte_b;
        }

        struct _gs_impl_no_alloc
        {
            using size_type = std::uint32_t;
            static constexpr size_type SMALL_STRING_SIZE = 12;
            static constexpr std::uint64_t PTR_TAG_MASK = static_cast<std::uint64_t>(0b11) << 62;

            _gs_impl_no_alloc();

            static constexpr std::uint64_t _get_ptr_tag(string_class cls)
            {
                return static_cast<std::uint64_t>(cls) << 62;
            }

            GS_FORCEINLINE size_type _get_size() const
            {
                // relies on little endian
                return static_cast<size_type>(_state[0]);
            }

            GS_FORCEINLINE bool _is_small() const
            {
                return _get_size() <= SMALL_STRING_SIZE;
            }

            GS_FORCEINLINE const char* _get_non_small_ptr() const
            {
                // relies on the ptr being in user-space canonical form
                // and the first 2 bits of the pointer being 0
                return reinterpret_cast<const char*>(_state[1] & ~PTR_TAG_MASK);
            }

            GS_FORCEINLINE char* _get_non_small_ptr()
            {
                // relies on the ptr being in user-space canonical form
                // and the first 2 bits of the pointer being 0
                return reinterpret_cast<char*>(_state[1] & ~PTR_TAG_MASK);
            }

            GS_FORCEINLINE char* _get_small_ptr()
            {
                return reinterpret_cast<char*>(&_state) + sizeof(size_type);
            }

            GS_FORCEINLINE const char* _get_small_ptr() const
            {
                return reinterpret_cast<const char*>(&_state) + sizeof(size_type);
            }

            GS_FORCEINLINE uint32_t _get_prefix() const
            {
				return static_cast<uint32_t>(_state[0] >> 32);
            }

            GS_FORCEINLINE char* _get_maybe_small_ptr()
            {
                return _is_small()
                    ? _get_small_ptr()
                    : _get_non_small_ptr();
            }

            GS_FORCEINLINE const char* _get_maybe_small_ptr() const
            {
                return _is_small()
                    ? _get_small_ptr()
                    : _get_non_small_ptr();
            }

            GS_FORCEINLINE string_class _get_class() const
            {
                if (_is_small())
                {
                    return string_class::persistent;
                }
                else
                {
                    std::uint64_t tag = (_state[1] & PTR_TAG_MASK) >> 62;
                    return static_cast<string_class>(tag);
                }
            }

            GS_FORCEINLINE bool _equals(const _gs_impl_no_alloc& other) const
            {
                if (_state[0] != other._state[0])
                {
                    return false;
                }

                // If we are small, it's guaranteed that that the other one is as well as we checked on the size and the prefix in the previous conditional
                if (_is_small())
                {
                    return _state[1] == other._state[1];
                }
                return std::memcmp(_get_non_small_ptr(), other._get_non_small_ptr(), _get_size()) == 0;
            }

            GS_FORCEINLINE int _compare(const _gs_impl_no_alloc& other) const
            {
                const auto min_size = std::min(_get_size(), other._get_size());
                const auto min_or_prefix_size = std::min(min_size, (uint32_t)sizeof(size_type));
                int prefix_cmp = prefix_memcmp(_get_prefix(), other._get_prefix(), min_or_prefix_size);
                if (min_or_prefix_size == min_size || prefix_cmp != 0)
                {
                    return prefix_cmp != 0 ? prefix_cmp : (_get_size() - other._get_size());
                }
                int result = std::memcmp(_get_maybe_small_ptr() + 4, other._get_maybe_small_ptr() + 4, static_cast<size_t>(min_size) - min_or_prefix_size);
                return result != 0 ? result : (_get_size() - other._get_size());
            }

            // The first 4 bytes are the size of the string
            // The next 4 bytes are the prefix
            // The next 8 bytes are a tagged pointer, first 2 bits are the tag for class
            // TODO: Experiment with using a union to be more clear about the layout
            std::uint64_t _state[2];
        };

        inline _gs_impl_no_alloc::size_type _checked_size_cast(size_t size)
        {
            if (size > std::numeric_limits<_gs_impl_no_alloc::size_type>::max())
            {
                throw std::length_error("Size exceeds maximum size for basic_german_string");
            }
            return static_cast<_gs_impl_no_alloc::size_type>(size);
        }
    }

    template <typename TAllocator = std::allocator<char>>
    class basic_german_string
    {
    public:
        using size_type = std::uint32_t;
        static constexpr size_type SMALL_STRING_SIZE = 12;
        static constexpr std::uint64_t PTR_TAG_MASK = static_cast<std::uint64_t>(0b11) << 62;

    private:
        // Impl also hides the allocator
        struct _gs_impl : public detail::_gs_impl_no_alloc, public TAllocator
        {
            using base = detail::_gs_impl_no_alloc;
            _gs_impl(TAllocator alloc)
                : TAllocator(alloc)
            {
            }

            _gs_impl(const char *str, size_type size, string_class cls, const TAllocator &allocator)
                : TAllocator(allocator)
            {
                _state[0] = size;
                if (_is_small())
                {
                    std::memcpy(_get_maybe_small_ptr(), str, size);
                }
                else
                {
                    if (cls == string_class::temporary)
                    {
                        char *copied_str = static_cast<char *>(std::allocator_traits<TAllocator>::allocate(*this, size));
                        if (!copied_str)
                        {
                            throw std::bad_alloc();
                        }

                        std::memcpy(copied_str, str, size);
                        str = copied_str;
                    }
                    std::memcpy(_get_small_ptr(), str, 4);
                    _state[1] = reinterpret_cast<std::uint64_t>(str) | _get_ptr_tag(cls);
                }
            }

            ~_gs_impl()
            {
                if (!_is_small())
                {
                    std::uint64_t tag = _state[1] & PTR_TAG_MASK;
                    if (tag == _get_ptr_tag(string_class::temporary))
                    {
                        std::allocator_traits<TAllocator>::deallocate(*this, _get_non_small_ptr(), _get_size());
                    }
                }
            }

            _gs_impl _substr(size_type start, size_type length, string_class cls = string_class::transient) const
            {
                if (start + length > _get_size())
                {
                    throw std::out_of_range("Substring out of range");
                }
                return _gs_impl(_get_maybe_small_ptr() + start, length, cls, *this);
            }

            bool _starts_with(const _gs_impl_no_alloc &other) const
            {
                if (_get_prefix() != other._get_prefix() || _get_size() < other._get_size())
                {
                    return false;
                }
                return _substr(0, other._get_size())._compare(other) == 0;
            }

            bool _ends_with(const _gs_impl_no_alloc &other) const
            {
                if (_get_size() < other._get_size())
                {
                    return false;
                }
                return _substr(_get_size() - other._get_size(), other._get_size())._compare(other) == 0;
            }
        };
        _gs_impl _impl;

    public:
        basic_german_string(const TAllocator &allocator = TAllocator()) noexcept(noexcept(TAllocator(allocator)))
            : _impl(allocator)
        {
        }

        basic_german_string(const char *str, size_type size, string_class cls, const TAllocator &allocator = TAllocator())
            : _impl(str, size, cls, allocator)
        {
        }

        basic_german_string(const char *ptr,
                            string_class cls = string_class::temporary,
                            const TAllocator &allocator = TAllocator())
            : basic_german_string(ptr, detail::_checked_size_cast(std::strlen(ptr)), cls, allocator)
        {
        }

        basic_german_string(std::nullptr_t) = delete;

        // TODO: Should I worry about allocator rebinding here?
        basic_german_string(const std::string &str, const TAllocator &allocator = TAllocator())
            : basic_german_string(str.data(), str.size(), string_class::temporary, allocator)
        {
        }

        basic_german_string(basic_german_string &&other) noexcept
            : _impl(std::move(other._impl))
        {
            if (other.get_class() == string_class::temporary)
            {
                other._impl._state[1] = other._impl._state[1] | other._impl._get_ptr_tag(string_class::transient);
            }
        }

        basic_german_string &operator=(const basic_german_string &) = default;
        basic_german_string &operator=(basic_german_string &&other) noexcept
        {
            if (this != &other)
            {
                _impl.~_gs_impl();
                _impl = std::move(other._impl);
                other._impl._state[0] = 0;
                other._impl._state[1] = 0;
            }
            return *this;
        }

        string_class get_class() const
        {
            return _impl._get_class();
        }

        size_type size() const
        {
            return _impl._get_size();
        }

        size_type length() const
        {
            return _impl._get_size();
        }

        std::string_view as_string_view() const
        {
            return std::string_view(_impl._get_maybe_small_ptr(), _impl._get_size());
        }

        basic_german_string copy_to_temporary() const
        {
            if (_impl._is_small())
            {
                return *this;
            }
            return basic_german_string(_impl._get_non_small_ptr(), _impl._get_size(), string_class::temporary, get_allocator());
        }

        int compare(const basic_german_string &other) const
        {
            return _impl._compare(other._impl);
        }

        template <typename TOtherAllocator>
        bool operator==(const basic_german_string<TOtherAllocator>& other) const
        {
            return _impl._equals(other._impl);
        }

        bool operator!=(const basic_german_string &other) const
        {
            return !(*this == other);
        }

        std::strong_ordering operator<=>(const basic_german_string &other) const
        {
            return _impl._compare(other._impl) <=> 0;
		}

        bool empty() const
        {
            return _impl._get_size() == 0;
        }

        std::string_view get_prefix_sv() const
        {
            return std::string_view(_impl._get_small_ptr(), 4);
        }

        template <typename TOtherAllocator>
        bool starts_with(const basic_german_string<TOtherAllocator> &other) const
        {
            return _impl._starts_with(other._impl);
        }

        template <typename TOtherAllocator>
        bool ends_with(const basic_german_string<TOtherAllocator> &other) const
        {
            return _impl._ends_with(other._impl);
        }

        // By default we return a transient string, basically a string_view
        basic_german_string substr(size_type start, size_type length, string_class cls = string_class::transient) const
        {
            return _impl._substr(start, length, cls);
        }

        TAllocator get_allocator() const
        {
            return static_cast<TAllocator>(_impl);
        }
    };

    using german_string = basic_german_string<>;

    namespace literals
    {
        inline german_string operator""_gs(const char *str, size_t size)
        {
            return german_string(str, static_cast<german_string::size_type>(size), string_class::persistent);
        }
    } // namespace literals
}