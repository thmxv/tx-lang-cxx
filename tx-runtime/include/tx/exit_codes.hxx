#pragma once

#include <cstdlib>

namespace tx {

enum ExitCode {
    SUCCESS = 0,
    USAGE_ERROR = 64,
    DATA_ERROR = 65,
    NO_INPUT = 66,
    SOFTWARE_INTERNAL_ERROR = 70,
    OS_ERROR = 71,
    IO_ERROR = 74,
};

}
