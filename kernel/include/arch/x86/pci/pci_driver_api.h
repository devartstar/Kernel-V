#ifndef PCI_DRIVER_API_H
#define PCI_DRIVER_API_H

#include "arch/x86/pci/pci_bar.h";
#include "arch/x86/pci/pci_cmd.h";
#include "arch/x86/pci/pci_devices.h";

/**
 * pci_device_get_bar - given a pci device and the BAR index, helper to get the
 * BAR information from the function record.
 * @device - Ref. to the PCI device
 * @idx - BAR index to return
 *
 * @return - Ref of the BAR information
 */
const pci_bar_info_t *pci_device_get_bar(const pci_device_t *device,
                                         uint8_t idx);

/**
 * pci_device_get_bar_kind - given a pci device and the BAR kind, helper to get
 * the BAR information from the function record.
 * @device - Ref. to the PCI device
 * @kind - Kind of BAR to search for
 *
 * @return - Ref of the first BAR of matching kind
 */
const pci_bar_info_t *pci_device_get_bar_kind(const pci_device_t *device,
                                              pci_bar_kind_t kind);

/**
 * pci_command_staus_info - given a pci device get the command status
 * @device - Ref. to the PCI device
 *
 * @return to the command and status information holding struct
 */
const pci_command_status_info_t *
pci_device_get_cmd_status(const pci_device_t *device);

/**
 * pci_device_get_cap_kind - given a pci device get the capability information
 * of a matching kind
 * @device - Ref. to the device to search for capability
 * @kind - Capability knid to search
 *
 * @return - Ref. of the first capability matching the kind
 */
const pci_capability_info_t *pci_device_get_cap_kind(const pci_device_t *device,
                                                     pci_cap_kind_t kind);

/**
 * pci_device_cmd_status_refresh - update the command and status info with the
 * latest.
 * @device - Ref. to the device for which we need to update
 *
 * @return 1 if update was success else 0
 */
uint8_t pci_device_cmd_status_refresh(const pci_device_t *device);

/**
 * pci_device_enable_io - enable the io by updating the config space
 * @device - Ref. to the device whose config space to access and update.
 *
 * @return - 1 on successful update else 0
 */
uint8_t pci_device_enable_io(const pci_device_t *device);

/**
 * pci_device_enable_mem - enable memory access by updating the config space
 * @device - Ref. to the device whose config space to access and update.
 *
 * @return - 1 on successful update else 0
 */
uint8_t pci_device_enable_mem(const pci_device_t *device);

/**
 * pci_device_enable_busmaster - enable bus master (enabling DMA) by updating
 * the config space
 * @device - Ref. to the device whose config space to access and update.
 *
 * @return - 1 on successful update else 0
 */
uint8_t pci_device_enable_bus_master(const pci_device_t *device);

#endif /* PCI_DRIVER_API_H */
