#include <stdio.h>

#include "logger.h"

int main(void) {
    TRACE("trace test");
    DEBUG("debug test");
    INFO("info test");
    WARN("warn test");
    ERROR("error test");
    CRIT("crit test");

    return 0;
}
