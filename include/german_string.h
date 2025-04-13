#include <memory>
#include <string_view>
#include <string>
#include <cstdint>
#include <cstring>
#include <cassert>

namespace gs
{
    struct temporary_tag
    {
    };
    struct persistent_tag
    {
    };
    struct transient_tag
    {
    };

    class german_string
    {
    public:
        using size_type = std::uint32_t;
        enum class string_class : uint8_t
        {
            temporary = 0,
            persistent = 1,
            transient = 2,
        };

        static constexpr size_type SMALL_STRING_SIZE = 12;
        static constexpr std::uint64_t PTR_TAG_MASK = static_cast<std::uint64_t>(0b11) << 62;

    private:
        // The first 4 bytes are the size of the string
        // The next 4 bytes are the prefix
        // The next 8 bytes are a tagged pointer, first 2 bits are the tag for class
        std::uint64_t _state[2];

        size_type _get_size() const
        {
            // relies on little endian
            return static_cast<size_type>(_state[0]);
        }

        bool _is_small() const
        {
            return _get_size() <= SMALL_STRING_SIZE;
        }

        const char *_get_ptr() const
        {
            // relies on the ptr being in user-space canonical form
            // and the first 2 bits of the pointer being 0
            return reinterpret_cast<const char *>(_state[1] & ~PTR_TAG_MASK);
        }

        char *_get_ptr()
        {
            // relies on the ptr being in user-space canonical form
            // and the first 2 bits of the pointer being 0
            return reinterpret_cast<char *>(_state[1] & ~PTR_TAG_MASK);
        }

        char *_get_maybe_small_ptr()
        {
            return _is_small()
                       ? (reinterpret_cast<char *>(&_state) + sizeof(size_type))
                       : _get_ptr();
        }

        const char *_get_maybe_small_ptr() const
        {
            return _is_small()
                       ? (reinterpret_cast<const char *>(&_state) + sizeof(size_type))
                       : _get_ptr();
        }

        static constexpr std::uint64_t _get_ptr_tag(string_class cls)
        {
            return static_cast<std::uint64_t>(cls) << 62;
        }

    public:
        german_string(const char *str, size_type size, string_class cls)
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
                    char *copied_str = new char[size];
                    std::memcpy(copied_str, str, size);
                    str = copied_str;
                }
                _state[1] = reinterpret_cast<std::uint64_t>(str) | _get_ptr_tag(cls);
            }
        }

        ~german_string()
        {
            if (!_is_small())
            {
                std::uint64_t tag = _state[1] & PTR_TAG_MASK;
                if (tag == _get_ptr_tag(string_class::temporary))
                {
                    delete[] _get_ptr();
                }
            }
        }

        string_class get_class() const
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

        size_type get_size() const
        {
            return _get_size();
        }

        std::string_view as_string_view() const
        {
            return std::string_view(_get_maybe_small_ptr(), _get_size());
        }

        bool operator==(const german_string &other) const
        {
            if (_state[0] != other._state[0])
            {
                return false;
            }

            if (_is_small())
            {
                return _state[1] == other._state[1];
            }
            return std::memcmp(_get_ptr(), other._get_ptr(), get_size()) == 0;
        }
    };
}