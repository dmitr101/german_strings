#include "german_string.h"

namespace gs::detail
{
    _gs_impl_no_alloc::_gs_impl_no_alloc()
    {
        _state[0] = 0;
        _state[1] = 0;
    }

    _gs_impl_no_alloc::size_type _gs_impl_no_alloc::_get_size() const
    {
        // relies on little endian
        return static_cast<size_type>(_state[0]);
    }

    bool _gs_impl_no_alloc::_is_small() const
    {
        return _get_size() <= SMALL_STRING_SIZE;
    }

    const char *_gs_impl_no_alloc::_get_non_small_ptr() const
    {
        // relies on the ptr being in user-space canonical form
        // and the first 2 bits of the pointer being 0
        return reinterpret_cast<const char *>(_state[1] & ~PTR_TAG_MASK);
    }

    char *_gs_impl_no_alloc::_get_non_small_ptr()
    {
        // relies on the ptr being in user-space canonical form
        // and the first 2 bits of the pointer being 0
        return reinterpret_cast<char *>(_state[1] & ~PTR_TAG_MASK);
    }

    char *_gs_impl_no_alloc::_get_small_ptr()
    {
        return reinterpret_cast<char *>(&_state) + sizeof(size_type);
    }

    const char *_gs_impl_no_alloc::_get_small_ptr() const
    {
        return reinterpret_cast<const char *>(&_state) + sizeof(size_type);
    }

    uint32_t _gs_impl_no_alloc::_get_prefix() const
    {
        // TODO: probably very UB
        return *reinterpret_cast<const uint32_t *>(_get_small_ptr());
    }

    char *_gs_impl_no_alloc::_get_maybe_small_ptr()
    {
        return _is_small()
                   ? _get_small_ptr()
                   : _get_non_small_ptr();
    }

    const char *_gs_impl_no_alloc::_get_maybe_small_ptr() const
    {
        return _is_small()
                   ? _get_small_ptr()
                   : _get_non_small_ptr();
    }

    string_class _gs_impl_no_alloc::_get_class() const
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
    void _gs_impl_no_alloc::_make_transient()
    {
        _state[1] = _state[1] | _get_ptr_tag(string_class::transient);
    }

    bool _gs_impl_no_alloc::_equals(const _gs_impl_no_alloc &other) const
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

    // probably wrong for a case with a string and its prefix
    // also probably not optimal
    int _gs_impl_no_alloc::_compare(const _gs_impl_no_alloc &other) const
    {
        const auto min_size = std::min(_get_size(), other._get_size());
        const auto min_or_prefix_size = std::min(min_size, (uint32_t)sizeof(std::uint32_t));
        int prefix_cmp = std::memcmp(_get_small_ptr(), other._get_small_ptr(), min_or_prefix_size);
        if (min_or_prefix_size == min_size || prefix_cmp != 0)
        {
            return prefix_cmp != 0 ? prefix_cmp : (_get_size() - other._get_size());
        }
        int result = std::memcmp(_get_maybe_small_ptr() + 4, other._get_maybe_small_ptr() + 4, min_size - min_or_prefix_size);
        return result != 0 ? result : (_get_size() - other._get_size());
    }
}