#include "tests/test_pci.h"

uint8_t pci_cfg_smoke_test() {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x0, .function = 0x00};

    uint32_t id_dword = pci_cfg_read32(bdf, 0x00);
    uint32_t class_dword = pci_cfg_read32(bdf, 0x08);
    uint32_t hdr_dword = pci_cfg_read32(bdf, 0x0C);

    if (id_dword == 0xFFFFFFFF || class_dword == 0xFFFFFFFF ||
        hdr_dword == 0xFFFFFFFF) {
        KLOG_ERROR("PCI_TEST", "cfg_smoke_test"
                               "Config read all bits set to 1\n");
        return 0;
    }

    /*
     * this reads the 4B from the config space
     * we need to extract the necessary information
     */

    KLOG_INFO("PCI_TEST",
              "cfg_smoke_test"
              "\n\tconfig read at BDF %02x.%02x.%u"
              "\n\tid=%08x, class=%08x, hdr=%08x\n",
              bdf.bus, bdf.device, bdf.function, id_dword, class_dword,
              hdr_dword);
    return 1;
}

uint8_t pci_cfg_extract_test() {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x0, .function = 0x00};

    uint16_t vendor = pci_cfg_read16(bdf, 0x00);
    uint16_t device = pci_cfg_read16(bdf, 0x02);
    uint8_t revision = pci_cfg_read8(bdf, 0x08);
    uint8_t prog_if = pci_cfg_read8(bdf, 0x09);
    uint8_t subclass = pci_cfg_read8(bdf, 0x0A);
    uint8_t classc = pci_cfg_read8(bdf, 0x0B);
    uint8_t hdr_type = pci_cfg_read8(bdf, 0x0E);

    if (vendor == PCI_INVALID_VENDOR_ID) {
        KLOG_ERROR("PCI_TEST", "cfg_extract_test"
                               "Config read invalid vendor ID\n");
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "extract_test"
              "\n\tconfig read at BDF %02x:%02x.%u"
              "\n\tvendor=%04x device=%04x class=%02x subclass=%02x "
              "progif=%02x rev=%02x hdr=%02x\n",
              bdf.bus, bdf.device, bdf.function, vendor, device, classc,
              subclass, prog_if, revision, hdr_type);

    return 1;
}

uint8_t pci_cfg_decode_test(void) {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x00, .function = 0x00};

    uint32_t id_dword = pci_cfg_read32(bdf, PCI_CFG_VENDOR_ID);
    uint32_t class_dword = pci_cfg_read32(bdf, PCI_CFG_REVISION_ID);
    uint32_t hdr_dword = pci_cfg_read32(bdf, PCI_CFG_CACHELINE_SIZE);

    uint16_t vendor_id = pci_dword_lo16(id_dword);
    uint16_t device_id = pci_dword_hi16(id_dword);

    uint8_t revision_id = pci_dword_byte0(class_dword);
    uint8_t prog_if = pci_dword_byte1(class_dword);
    uint8_t subclass = pci_dword_byte2(class_dword);
    uint8_t class_code = pci_dword_byte3(class_dword);

    uint8_t header_type = pci_dword_byte2(hdr_dword);

    if (vendor_id == PCI_INVALID_VENDOR_ID) {
        KLOG_ERROR("PCI_TEST", "cfg_decode_test"
                               "Config read invalid vendor ID\n");
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "decode_test"
              "\n\tconfig read at BDF %02x:%02x.%u"
              "\n\tvendor=%04x device=%04x class=%02x "
              "subclass=%02x progif=%02x rev=%02x hdr=%02x multi=%u\n",
              bdf.bus, bdf.device, bdf.function, vendor_id, device_id,
              class_code, subclass, prog_if, revision_id,
              (header_type & PCI_CFG_HEADER_TYPE_MASK),
              pci_cfg_header_type_is_multifunctional(header_type));

    return 1;
}

uint8_t pci_probe_function_test() {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x00, .function = 0x00};
    pci_function_identity_t id;
    pci_probe_result_t res;

    res = pci_probe_function(bdf, &id);
    if (res == PCI_PROBE_ERROR) {
        KLOG_ERROR(
            "PCI_TEST",
            "probe_function_test error while probing bdf %02x:%02x.%u.\n",
            bdf.bus, bdf.device, bdf.function);
        return 0;
    }

    if (res == PCI_PROBE_ABSENT) {
        KLOG_ERROR(
            "PCI_TEST",
            "probe_function_test function not present bfd %02x:%02x.%u.\n",
            bdf.bus, bdf.device, bdf.function);
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "probe_function_test"
              "\n\tFunction at endpoint bdf %02x:%02x.%u."
              "\n\tvendor=%04x device=%04x class=%02x subclass=%02x "
              "progif=%02x rev=%02x hdr=%02x multi=%u layout=%02x\n",
              id.bdf.bus, id.bdf.device, id.bdf.function, id.vendor_id,
              id.device_id, id.class_code, id.subclass, id.prog_if,
              id.revision_id, id.header_type,
              pci_cfg_header_type_is_multifunctional(id.header_type),
              pci_cfg_header_type_layout(id.header_type));

    return 1;
}
