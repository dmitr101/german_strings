#include "german_string.h"

namespace gs::detail
{
    _gs_impl_no_alloc::_gs_impl_no_alloc()
    {
        _state[0] = 0;
        _state[1] = 0;
    }

}