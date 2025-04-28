#include <memory>
#include <string_view>
#include <string>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <compare>
#include <concepts>

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
    }

    template <typename TAllocator = std::allocator<char>>
    class basic_german_string
    {
    public:
        using size_type = std::uint32_t;
        static constexpr size_type SMALL_STRING_SIZE = 12;
        static constexpr std::uint64_t PTR_TAG_MASK = static_cast<std::uint64_t>(0b11) << 62;

    private:
        struct _gs_impl : TAllocator
        {
            _gs_impl(TAllocator alloc)
                : TAllocator(alloc), _state{0, 0}
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

            size_type _get_size() const
            {
                // relies on little endian
                return static_cast<size_type>(_state[0]);
            }

            bool _is_small() const
            {
                return _get_size() <= SMALL_STRING_SIZE;
            }

            const char *_get_non_small_ptr() const
            {
                // relies on the ptr being in user-space canonical form
                // and the first 2 bits of the pointer being 0
                return reinterpret_cast<const char *>(_state[1] & ~PTR_TAG_MASK);
            }

            char *_get_non_small_ptr()
            {
                // relies on the ptr being in user-space canonical form
                // and the first 2 bits of the pointer being 0
                return reinterpret_cast<char *>(_state[1] & ~PTR_TAG_MASK);
            }

            char *_get_small_ptr()
            {
                return reinterpret_cast<char *>(&_state) + sizeof(size_type);
            }

            const char *_get_small_ptr() const
            {
                return reinterpret_cast<const char *>(&_state) + sizeof(size_type);
            }

            char *_get_maybe_small_ptr()
            {
                return _is_small()
                           ? _get_small_ptr()
                           : _get_non_small_ptr();
            }

            const char *_get_maybe_small_ptr() const
            {
                return _is_small()
                           ? _get_small_ptr()
                           : _get_non_small_ptr();
            }

            static constexpr std::uint64_t _get_ptr_tag(string_class cls)
            {
                return static_cast<std::uint64_t>(cls) << 62;
            }

            string_class _get_class() const
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

            // VERY UNSAFE! Only used when moving out of a temporary-class string.
            void _make_transient()
            {
                _state[1] = _state[1] | _get_ptr_tag(string_class::transient);
            }

            bool _equals(const _gs_impl &other) const
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

            // comparison for a string and it's prefix???
            int _compare(const _gs_impl &other) const
            {
                const auto min_size = std::min(_get_size(), other._get_size());
                const auto min_or_prefix_size = std::min(min_size, (uint32_t)sizeof(std::uint32_t));
                int prefix_cmp = std::memcmp(_get_small_ptr(), other._get_small_ptr(), min_or_prefix_size);
                if (prefix_cmp != 0 || min_or_prefix_size == min_size)
                {
                    return prefix_cmp;
                }
                return std::memcmp(_get_maybe_small_ptr() + 4, other._get_maybe_small_ptr() + 4, min_size - min_or_prefix_size);
            }

            // The first 4 bytes are the size of the string
            // The next 4 bytes are the prefix
            // The next 8 bytes are a tagged pointer, first 2 bits are the tag for class
            std::uint64_t _state[2];
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
            : basic_german_string(ptr, std::strlen(ptr), cls, allocator)
        {
        }

        basic_german_string(std::nullptr_t) = delete;

        // basic_german_string(const basic_german_string& other)
        // {
        //     // TODO: impl
        // }

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

        size_type get_size() const
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

        bool operator==(const basic_german_string &other) const
        {
            return _impl._equals(other._impl);
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

        // implement starts_with, ends_with, first one should benefit

        TAllocator get_allocator() const
        {
            return static_cast<TAllocator>(_impl);
        }
    };

    using german_string = basic_german_string<>;
}