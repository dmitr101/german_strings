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

// TODO: I'm passing a lot by const reference, but I should be passing by value maybe???
// TODO: A constructor withj size type being size_t and check if the size fits in 32 bits and panic if it doesn't

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
        // template<typename T>
        // constexpr bool probably_str_literal =
        //      std::is_convertible_v      <T, const char*> &&
        //     !std::is_rvalue_reference_v <T> &&
        //     !std::is_pointer_v          <T> &&
        //     !std::is_array_v            <T> &&
        //     !std::is_class_v            <T>;

        // template<typename T>
        // consteval string_class predict_string_class()
        // {
        //     if constexpr (probably_str_literal<T>)
        //     {
        //         return string_class::persistent;
        //     }
        //     else
        //     {
        //         return string_class::temporary;
        //     }
        // }

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

            size_type _get_size() const;
            bool _is_small() const;
            const char *_get_non_small_ptr() const;
            char *_get_non_small_ptr();
            char *_get_small_ptr();
            const char *_get_small_ptr() const;
            uint32_t _get_prefix() const;
            char *_get_maybe_small_ptr();
            const char *_get_maybe_small_ptr() const;
            string_class _get_class() const;
            void _make_transient();
            bool _equals(const _gs_impl_no_alloc &other) const;
            int _compare(const _gs_impl_no_alloc &other) const;
            bool _starts_with(const _gs_impl_no_alloc &other) const;

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

        basic_german_string(basic_german_string &&other)
            : _impl(std::move(other._impl))
        {
            if (other.get_class() == string_class::temporary)
            {
                other._impl._make_transient();
            }
        }

        basic_german_string &operator=(const basic_german_string &) = default;
        basic_german_string &operator=(basic_german_string &&other)
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

        bool operator!=(const basic_german_string &other) const
        {
            return !(*this == other);
        }

        bool operator<(const basic_german_string &other) const
        {
            return _impl._compare(other._impl) < 0;
        }

        bool operator>(const basic_german_string &other) const
        {
            return _impl._compare(other._impl) > 0;
        }

        bool operator<=(const basic_german_string &other) const
        {
            return _impl._compare(other._impl) <= 0;
        }

        bool operator>=(const basic_german_string &other) const
        {
            return _impl._compare(other._impl) >= 0;
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
        bool operator==(const basic_german_string<TOtherAllocator> &other) const
        {
            return _impl._equals(other._impl);
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
        inline german_string operator"" _gs(const char *str, size_t size)
        {
            return german_string(str, static_cast<german_string::size_type>(size), string_class::persistent);
        }
    } // namespace literals
}