#include "core/device.h"

const char *device_state_name(device_state_t state) {
    switch (state) {
    case DEVICE_STATE_EMPTY:
        return "empty";
    case DEVICE_STATE_DISCOVERED:
        return "discovered";
    case DEVICE_STATE_BOUND:
        return "bound";
    case DEVICE_STATE_PROBE_FAILED:
        return "probe_failed";
    default:
        return "unknown";
    }
}
