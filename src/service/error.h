#pragma once

namespace service {
    enum class Error { Ok = 0, ErrInternal = 500, ErrNotFound = 404, ErrBadRequest = 400 };
}
