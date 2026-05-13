#include "tests/test_pci.h"

void pci_cfg_smoke_test() {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x0, .function = 0x00};

    uint32_t id_dword = pci_cfg_read32(bdf, 0x00);
    uint32_t class_dword = pci_cfg_read32(bdf, 0x08);
    uint32_t hdr_dword = pci_cfg_read32(bdf, 0x0C);

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
}

void pci_cf_extract_test() {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x0, .function = 0x00};

    uint16_t vendor = pci_cfg_read16(bdf, 0x00);
    uint16_t device = pci_cfg_read16(bdf, 0x02);
    uint8_t revision = pci_cfg_read8(bdf, 0x08);
    uint8_t prog_if = pci_cfg_read8(bdf, 0x09);
    uint8_t subclass = pci_cfg_read8(bdf, 0x0A);
    uint8_t classc = pci_cfg_read8(bdf, 0x0B);
    uint8_t hdr_type = pci_cfg_read8(bdf, 0x0E);

    KLOG_INFO("PCI_TEST",
              "extract_test"
              "\n\tconfig read at BDF %02x:%02x.%u"
              "\n\tvendor=%04x device=%04x class=%02x subclass=%02x "
              "progif=%02x rev=%02x hdr=%02x\n",
              bdf.bus, bdf.device, bdf.function, vendor, device, classc,
              subclass, prog_if, revision, hdr_type);
}
