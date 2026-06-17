#ifndef CORE_DEVICE_H
#define CORE_DEVICE_H

#include "lib/printk.h"
#include "lib/string.h"

#define DEVICE_NAME_MAX 32

/**
 * device_bus_type - to distinguish between PCI backed devices and others
 */
typedef enum device_bus_type {
    DEVICE_BUS_NONE = 0,
    DEVICE_BUS_PCI,
} device_bus_type_t;

/*
 * device_state - stores the state of the device
 * @EMPTY - device object pci slot exists but is not yet initialized
 * @DISCOVERED - device is discovered, but no driver bound to it
 * @BOUND - device is discoverd, driver matched and probed successfully
 * @FAILED - device is discovered, driver matched but probing failed
 */
typedef enum device_state {
    /* default state of a device */
    DEVICE_STATE_INVALID = 0,

    /* device object exists and is discovered by bus core
     * but no binding decision taken yet */
    DEVICE_STATE_DISCOVERED,

    /* a driver has been matched with the device
     * but probe has not been started yet */
    DEVICE_STATE_MATCHED,

    /* matched drivers probe routine has been executing
     * On probe completion state transitions into BOUND or PROBE_FAILED */
    DEVICE_STATE_PROBING,

    /* driver probe routine has succeeded and device is now owned by a driver */
    DEVICE_STATE_BOUND,

    /* driver probe routine has failed, device is not owned by a driver yet */
    DEVICE_STATE_PROBE_FAILED,

    /*no registered driver rule matches with this device.
     * device is valid but not bound to any driver */
    DEVICE_STATE_UNBOUND,

    DEVICE_STATE_COUNT,
} device_state_t;

/**
 * device_state_name - convert device state to string.
 */
const char *device_state_name(device_state_t state);

/**
 * device - struct which stores the device information
 * @name - Name of the device, should be simple and scructured like -
 * pci-00:03.0.
 * @bus_type - the type of bus if any associated with this device
 * @state - current state of device - if device discovered and probed, driver
 * bound
 * @parent - Ref to the parent device
 * @bus_data - Ref to Data owned by the bus layer. For PCI devices it will
 * reference the function record.
 * @driver_data - Ref to Data owned by the bound driver after successful.
 * intialized post successful probe and preserver by bus core.
 * probing.
 * @bound_driver - Ref to the bus specific Device Driver object. Unused for now.
 */
typedef struct device {
    char name[DEVICE_NAME_MAX];
    device_bus_type_t bus_type;
    device_state_t state;
    struct device *parent;
    void *bus_data;
    void *driver_data;
    const void *bound_driver;
} device_t;

/**
 * device_init - initailize a device object as discovered and with other passed
 * information.
 * @device - Ref. to the device object.
 * @bus_type - bus type for the device
 * @name - device name
 * @parent - Ref. to the parent device object.
 */
static inline void device_init(device_t *device, device_bus_type_t bus_type,
                               const char *name, device_t *parent) {
    /* check for valid ref of the device to initialize */
    if (!device) {
        KLOG_ERROR(
            "DEVICE",
            "Device initialize failed, invalid reference to the device.\n");
        return;
    }

    /* intialize 0 assigned memory for device struct object */
    memset(device, 0, sizeof(*device));

    /* initalize with the passed value */
    device->bus_type = bus_type;
    device->parent = parent;
    device->state = DEVICE_STATE_DISCOVERED;

    if (name) {
        strncpy(device->name, name, DEVICE_NAME_MAX - 1);
        device->name[DEVICE_NAME_MAX - 1] = '\0';
    }
}

/**
 * device_set_state - set the current state of the device
 * @device - Ref. to the device object.
 * @state - current state of the device
 */
static inline void device_set_state(device_t *device, device_state_t state) {
    /* check for valid ref of the device to initialize */
    if (!device) {
        KLOG_ERROR(
            "DEVICE",
            "Device state set failed, invalid reference to the device.\n");
        return;
    }

    device->state = state;
}

/**
 * device_set_driver_data - helper to set the ref of the driver data to the
 * device
 * @device - device object in which to set the driver info.
 * @dricer_data - ref. to the driver data to be set.
 */
static inline void device_set_driver_data(device_t *device, void *driver_data) {
    if (!device) {
        KLOG_ERROR(
            "DEVICE",
            "Failed ot set the driver data to device. Invalid input args.\n");
        return;
    }

    device->driver_data = driver_data;
}

/**
 * device_get_driver_data - helper to get the ref to the driver data of a
 * device.
 * @device - device object of which we need driver data.
 */
static inline void *device_get_driver_data(device_t *device) {
    if (!device) {
        KLOG_ERROR(
            "DEVICE",
            "Failed ot get the driver data of device. Invalid input args.\n");
        return NULL;
    }

    return device->driver_data;
}

#endif /* CORE_DEVICE_H */
