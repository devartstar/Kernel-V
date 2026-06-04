#ifndef PCI_DUMP_H
#define PCI_DUMP_H

#include "arch/x86/pci/pci_record_registry.h"

/**
 * pci_dump_record_visitor - callback for dumping all pci records.
 * @rec - registry record to dump.
 * @ctx - constext passed to the callback.
 */
void pci_dump_record_visitor(const pci_function_record_t *record, void *ctx);

/**
 * pci_dump_record_registry - helper routine to dump all the function registered
 * in the registry.
 * @reg - pointer to the registry structure.
 */
void pci_dump_record_registry(const pci_record_registry_t *reg);

/**
 * pci_class_name - decode the class name based on class and subclass bits
 */
const char *pci_class_name(uint8_t class_code, uint8_t subclass);

/**
 * pci_bar_kind_name - convert enum to string for bar type.
 */
const char *pci_bar_kind_name(pci_bar_kind_t kind);

/**
 * pci_capability_kind_name - convert capability kind enum to string.
 */
const char *pci_capability_kind_name(pci_cap_kind_t kind);

/**
 * pci_dump_type0_bars - helper routine to dump all the bars for the type0
 * endpoint
 *
 * @record - pointer to the type0 endpoint identifier.
 */
void pci_dump_type0_bars(const pci_function_record_t *record);

/**
 * pci_dump_record_resource - dump entries of a function record
 * @rec - reference to the function record to dump.
 */
void pci_dump_record_resources(const pci_function_record_t *rec);

/**
 * pci_dump_record_registry_resources - dump all entries of the pci registry
 * structure.
 * @reg - reference to the pci registry structure.
 */
void pci_dump_record_registry_resources(const pci_record_registry_t *reg);

/**
 * pci_dump_record_capabilities - dump all the capability info in the
 * function record
 * @rec - pointer to the function record
 */
void pci_dump_record_capabilities(const pci_function_record_t *rec);

#endif
