#include "core/device.h"

const char *device_state_name(device_state_t state) {
    switch (state) {
    case DEVICE_STATE_INVALID:
        return "invalid";
    case DEVICE_STATE_DISCOVERED:
        return "discovered";
    case DEVICE_STATE_MATCHED:
        return "matched";
    case DEVICE_STATE_PROBING:
        return "probing";
    case DEVICE_STATE_BOUND:
        return "bound";
    case DEVICE_STATE_PROBE_FAILED:
        return "probe_failed";
    case DEVICE_STATE_UNBOUND:
        return "unbound";
    default:
        return "unknown";
    }
}
