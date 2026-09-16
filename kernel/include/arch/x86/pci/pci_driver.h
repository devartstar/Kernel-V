#ifndef PCI_DRIVER_H
#define PCI_DRIVER_H

#include "arch/x86/pci/pci_devices.h"
#include <stdint.h>

/* define 8 & 16 bits rule value to match unconditionally */
#define PCI_MATCH_ANY_U16 0xFFFF
#define PCI_MATCH_ANY_U8 0xFF

/* pointer to driver probing function */
typedef uint8_t (*pci_driver_probe_fn)(pci_device_t *pci_dev);
typedef void (*pci_driver_remove_fn)(pci_device_t *pci_dev);

/**
 * pic_match_type - enum for match type
 */
typedef enum pic_match_type {
    PCI_MATCH_NONE = 0,
    PCI_MATCH_VENDOR_DEVICE,
    PCI_MATCH_CLASS,
} pci_match_type_t;

/**
 * pci_matching_rule_t - object defininga matching ruleto associate a driver to
 * a device
 * @type - matching rule type
 * @vendor_id - pci device vendor id this driver belongs to
 * @device_id - pci device id this driver belongs to
 * @class_code - pci device class code
 * @subclass - pci device subsclass
 * @prog_if - pci prog interrupt flag
 */
typedef struct pci_match_rule {
    pci_match_type_t type;

    uint16_t device_id;
    uint16_t vendor_id;

    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
} pci_match_rule_t;

/**
 * pci_driver - a pci driver object
 * @name - identifier of driver object, or logging
 * vendor_id - pci device vendor id this driver belongs to
 * device_id - pci device id this driver belongs to
 * probe - function to invoke when this driver is called
 */
typedef struct pci_driver {
    const char *name;
    pci_driver_probe_fn probe;
    pci_driver_remove_fn remove;
    pci_match_rule_t *matches;
    uint8_t match_count;
} pci_driver_t;

/**
 * PCI DEVICE MATCHER - Matches the first Device that satisfies the rule
 * cons: Device binding to the driver depends on the registration order
 */

/**
 * pci_driver_rile_match_device - given a ref. to the pci driver rules and a
 * device, this routine checks if the device maches the rule.
 * @rule - ref. to the driver rule.
 * @pci_device - ref. to the device object.
 *
 * @return 1 - if the device matches to a rule else 0.
 */
uint8_t pci_driver_rule_match_device(const pci_match_rule_t *rule,
                                     const pci_device_t *pci_device);

/**
 * pci_driver_matches_boolean - from the list of the driver rules to match to a
 * device, checks if a device matches to any of the rules.
 * @pci_driver - Ref. to the driver object
 * @pci_device - Ref. to te device object
 *
 * @return 1 if it matches else 0
 */
uint8_t pci_driver_matches_boolean(const pci_driver_t *pci_driver,
                                   const pci_device_t *pci_device);

/**
 * PCI_SCORE_MATCHER - Matches the best Device suitable for the driver
 */

/* define the score for a successful match */
typedef enum pci_match_score {
    PCI_MATCH_SCORE_NONE = 0,
    PCI_MATCH_SCORE_CLASS_ONLY = 10,
    PCI_MATCH_SCORE_CLASS_SUBCLASS = 20,
    PCI_MATCH_SCORE_CLASS_SUBCLASS_PROGIF = 30,
    PCI_MATCH_SCORE_VENDOR_ONLY = 40,
    PCI_MATCH_SCORE_VENDOR_DEVICE = 50,
} pci_match_score_t;

/**
 * pci_driver_rule_match_device_score - get the match score for the current rule
 * for the device.
 * @rule - ref. to the driver rule.
 * @pci_device - ref. to the device object.
 *
 * return pci_match_score - match score for the current rule
 */
pci_match_score_t
pci_driver_rule_match_device_score(const pci_match_rule_t *rule,
                                   const pci_device_t *pci_device);

/**
 * pci_driver_match_score - for a device to match with a driver get the highest
 * matching score
 * @pci_driver - Ref. to the driver object
 * @pci_device - Ref. to te device object
 *
 * @return pci_match_score - highest match score
 */
pci_match_score_t pci_driver_match_score(const pci_driver_t *pci_driver,
                                         const pci_device_t *pci_device);

uint8_t pci_driver_matches(const pci_driver_t *pci_driver,
                           const pci_device_t *pci_device);
#endif /* PCI_DRIVER_H */
