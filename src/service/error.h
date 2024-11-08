#pragma once

namespace service
{
    enum class Error
    {
        Ok = 0,
        ErrInternal,
        ErrNotFound,
        ErrBadRequest
    };
}
