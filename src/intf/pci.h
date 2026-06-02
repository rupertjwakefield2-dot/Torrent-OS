#pragma once
#include <stdint.h>

typedef struct {
    uint16_t vendor;
    uint16_t device;
    uint8_t  bus, slot, func;
    uint8_t  class_code, subclass;
} PciDev;

#define PCI_MAX_DEVS 64

void        pci_scan(void);
int         pci_dev_count(void);
const PciDev *pci_dev(int idx);

int         pci_has_wifi(void);
int         pci_has_bluetooth(void);
const char *pci_wifi_name(void);
const char *pci_bt_name(void);
