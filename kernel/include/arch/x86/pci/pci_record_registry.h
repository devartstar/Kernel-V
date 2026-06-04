#ifndef PCI_REGISTRY_H
#define PCI_REGISTRY_H

#include "arch/x86/pci/pci.h"
#include "arch/x86/pci/pci_bar.h"
#include "arch/x86/pci/pci_bind.h"
#include "arch/x86/pci/pci_cap.h"
#include "arch/x86/pci/pci_cfg.h"
#include "arch/x86/pci/pci_cmd.h"

/**
 * id - indentity information for a function entry
 * present - reduntant info is function is present
 * bars - array of information for each bar
 * bars_valid - if the bars register has valid info then 1 else 0
 * cmd_status - contains the 16 bit command and status of the config space
 * cmd_status_valid - if the entry of cmd status is valid for this record
 * caps - array of function capability information
 * caps_present - 1 if the header stats capability present for function
 * caps_valid - 1 if the cpabilities have been iterated
 * caps_count - number of capabilities discovered and populated in caps
 */
typedef struct pci_function_record {
    pci_function_identity_t id;
    uint8_t present;

    pci_bar_info_t bars[PCI_TYPE0_BAR_COUNT];
    uint8_t bars_valid;

    pci_command_status_info_t cmd_status;
    uint8_t cmd_status_valid;

    pci_capability_info_t caps[PCI_CAP_MAX_PER_FUNCTION];
    uint8_t caps_present;
    uint8_t caps_valid;
    uint8_t cap_count;

    pci_bind_state_t bind_state;
} pci_function_record_t;

/**
 * entries - fixed array of identity for all discovered functions
 * count - number of entries discovered
 */
typedef struct pci_record_registry {
    pci_function_record_t entries[PCI_MAX_FUNCTIONS_BUS0];
    uint32_t count;
} pci_record_registry_t;

/**
 * pci_record_registry_ctx - context to pass to the function probing callback
 * @registry - pointer to the registry structure to add function id on discovery
 * @inserted - number of function id successfully inserted to the registry
 * @errors - number of function id unsuccessful in registering
 */
typedef struct pci_record_registry_fill_ctx {
    pci_record_registry_t *registry;
    uint32_t inserted;
    uint32_t errors;
} pci_record_registry_fill_ctx_t;

/* Callback for invoking a fnction */
typedef void (*pci_record_registry_visitor_fn)(
    const pci_function_record_t *fn_record, void *ctx);
/**
 * pci_record_registry_init - routine to initalize the registery structure.
 * @reg - pointer to the registry array to initialize.
 *
 * @return void
 */
void pci_record_registry_init(pci_record_registry_t *reg);

/**
 * pci_record_registry_add - adds a function identity to the registry structure.
 * @reg - pointer to the registry structure to add function identity.
 * @id - pointer to the identity to be registered.
 *
 * @uint8_t - 0 on failure and 1 on success.
 */
uint8_t pci_record_registry_add(pci_record_registry_t *reg,
                                const pci_function_identity_t *id);

/**
 * pci_record_registry_fill_visitor - callback routine when a function is
 * discovered, it adds function identity to registry context.
 *
 * @id - pointer to the function identity to add to the registry context.
 * @ctx - pointer to the registry context.
 */
static void pci_record_registry_fill_visitor(const pci_function_identity_t *id,
                                             void *ctx) {
    /* organize the memory of the context from void into type
     * pci_record_registry_fill_ctx_t */
    pci_record_registry_fill_ctx_t *fill_ctx =
        (pci_record_registry_fill_ctx_t *)ctx;

    /* check for valid pointers in the registry context and id to be registered
     */
    if (!fill_ctx || !fill_ctx->registry || !id) {
        KLOG_ERROR(
            "PCI",
            "invalid registry context or entries or id to be registered.\n");
        return;
    }

    /* add the function id to the registry */
    if (pci_record_registry_add(fill_ctx->registry, id)) {
        fill_ctx->inserted++;
    } else {
        fill_ctx->errors++;
    }
}

/**
 * pci_enumerate_bus0_into_record_registry - scan all slots and functions in bus
 * 0 and add to the registry structure.
 * @reg - pointer to the registry structure that holds all function identity.
 *
 * @return - 1: success and 0:failure
 */
uint8_t pci_enumerate_bus0_into_record_registry(pci_record_registry_t *reg);

/**
 * pci_record_registry_foreach - For all the registry entries that are present.
 * invoke the callback.
 * @reg - pointer to the registry structure.
 * @visitor - pointer to the callback.
 * @ctx - pointer to the context to be passed to the callback.
 */
void pci_record_registry_foreach(const pci_record_registry_t *reg,
                                 pci_record_registry_visitor_fn visitor,
                                 void *ctx);

/**
 * pci_record_registry_find_bdf - Given an endpoint helper to find the registry
 * entry reg - pointer to the registry structure. bdf - endpoint to search in
 * the registry
 */
const pci_function_record_t *
pci_record_registry_find_bdf(const pci_record_registry_t *reg, pci_bdf_t bdf);

/**
 * pci_enrich_record_registry_resources - for all the function record in the
 * registry. read and decode the coomand, status bytes and the BAR entries.
 * @reg - reference to the registry to store decoded values
 *
 * @return - 1 for success else 0
 */
uint8_t pci_enrich_record_registry_resources(pci_record_registry_t *reg);

#endif /* PCI_REGISTRY_H */
