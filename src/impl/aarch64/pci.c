/* PCI stub for AArch64 — QEMU virt uses PCIe via MMIO, driver not yet implemented */
#include "pci.h"
static PciDev dummy;
void        pci_scan(void)         { }
int         pci_dev_count(void)    { return 0; }
const PciDev *pci_dev(int i)       { (void)i; return (void*)0; }
int         pci_has_wifi(void)     { return 0; }
int         pci_has_bluetooth(void){ return 0; }
const char *pci_wifi_name(void)    { return "None"; }
const char *pci_bt_name(void)      { return "None"; }
