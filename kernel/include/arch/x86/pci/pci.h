#ifndef PCI_H
#define PCI_H

#include "arch/x86/pci/pci_cfg.h"

#define PCI_FUNCTION_PER_SLOT 8
#define PCI_DEVICES_PER_BUS 32
#define PCI_MAX_FUNCTIONS_BUS0 256

/**
 * pci_function_identity - a minimal register record of what the config space
 * tells the pci subsystem for discovery.
 */
typedef struct pci_function_identity {
    pci_bdf_t bdf;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;

    uint8_t header_type;
} pci_function_identity_t;

/**
 * Defining an enum for the differnet typed of outcome on probing:
 * If function is absent, should not be classified with transfort failure
 * For transport failure, result is 0
 * For success result is 1
 */
typedef enum pci_probe_result {
    PCI_PROBE_ABSENT = -1,
    PCI_PROBE_ERROR = 0,
    PCI_PROBE_SUCCESS = 1,
} pci_probe_result_t;

/* Callback for bus scaner on finding a device(slot) */
typedef void (*pci_scan_visitor_fn)(const pci_function_identity_t *id,
                                    void *ctx);

/* Callback from slot scanner on fiding a funtion */
typedef void (*pci_probe_visitor_fn)(const pci_function_identity_t *id,
                                     void *ctx);

/* Check if function is the first entry of the slot */
static inline uint8_t pci_func_is_zero(pci_function_num_t func) {
    return func == 0;
}

/* Check if pci config space is valid */
static inline uint8_t
pci_identity_is_present(const pci_function_identity_t *id) {
    return (id && id->vendor_id != 0xffff);
}

/* Comparision between two bdf address */
static inline uint8_t pci_bdf_is_equal(pci_bdf_t a, pci_bdf_t b) {
    return (a.bus == b.bus) && (a.device == b.device) &&
           (a.function == b.function);
}

/**
 * pci_probe_function - routine to check given a bdf, function is present and
 * valid
 *
 * @bdf - enpoint for which we need to check the function
 * @out - populate the info about function from config space
 *
 * @pci_probe_result_t - result of the probing
 */
pci_probe_result_t pci_probe_function(pci_bdf_t bdf,
                                      pci_function_identity_t *out);

/**
 * pci_probe_slot - scans for pci device slot for all the function
 * @bus in which the device exists for scanning
 * @device to scan for fnctions
 * @visitor - callback routine whenever a function is found in the slot.
 * @ctx - info to pass to the callback on finding a function.
 * @fn_found - updates with the number of functions found.
 *
 * @return - status if the of slot scan. -1: absent, 0: error, 1: success.
 */
pci_probe_result_t pci_probe_slot(pci_bus_num_t bus, pci_device_num_t device,
                                  pci_probe_visitor_fn visitor, void *ctx,
                                  uint32_t *fn_found);

/**
 * pci_scan_bus0 - a bus wise scanner for all device slots
 * @visitor - callback routine whenever we discover a slot
 * @ctx - info to pass to the callback
 * @fn_found - count of the number of fuctions under a bus.
 *
 * @return - status of the bus0 scan. -1: absent, 0: error, 1: success.
 */
pci_probe_result_t pci_scan_bus0(pci_scan_visitor_fn visitor, void *ctx,
                                 uint32_t *fn_found);

#endif /* PCI_H */
