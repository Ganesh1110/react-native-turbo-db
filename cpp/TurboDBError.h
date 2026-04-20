#pragma once

#include <string>
#include <cstdint>

namespace turbo_db {

enum class TurboDBErrorCode : int32_t {
    OK                = 0,
    NOT_INITIALIZED   = 1001,
    KEY_TOO_LONG      = 1002,
    SERIALIZE_FAIL    = 1003,
    DECRYPT_FAIL      = 1004,
    CRC_MISMATCH      = 1005,
    IO_FAIL           = 1006,
    DB_CORRUPT        = 1007,
    KEY_NOT_FOUND     = 1008,
    INVALID_ARGS      = 1009,
    SCHEDULER_STOPPED = 1010,
    REPAIR_FAILED     = 1011,
};

struct TurboDBError {
    TurboDBErrorCode code;
    std::string message;

    TurboDBError(TurboDBErrorCode c, std::string msg)
        : code(c), message(std::move(msg)) {}

    std::string toString() const {
        return "[TurboDB E" + std::to_string(static_cast<int32_t>(code)) +
               "] " + message;
    }
};

} // namespace turbo_db
