#include "pci.h"
#include "io.h"

#define PCI_ADDR  0xCF8
#define PCI_DATA  0xCFC

static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    uint32_t addr = (1u << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)slot << 11)
                  | ((uint32_t)func <<  8)
                  | (off & 0xFC);
    outl(PCI_ADDR, addr);
    return inl(PCI_DATA);
}

static PciDev   devs[PCI_MAX_DEVS];
static int      ndevs      = 0;
static int      has_wifi   = 0;
static int      has_bt     = 0;
static const char *wifi_nm = "None";
static const char *bt_nm   = "None";

void pci_scan(void) {
    ndevs = 0; has_wifi = 0; has_bt = 0;
    wifi_nm = "None"; bt_nm = "None";

    for (int bus = 0; bus < 16 && ndevs < PCI_MAX_DEVS; bus++) {
        for (int slot = 0; slot < 32 && ndevs < PCI_MAX_DEVS; slot++) {
            uint32_t id = pci_read32((uint8_t)bus, (uint8_t)slot, 0, 0);
            if ((id & 0xFFFF) == 0xFFFF) continue;   /* no device */

            uint32_t cc     = pci_read32((uint8_t)bus, (uint8_t)slot, 0, 8);
            uint8_t  cls    = (uint8_t)(cc >> 24);
            uint8_t  sub    = (uint8_t)(cc >> 16);
            uint16_t vendor = (uint16_t)(id & 0xFFFF);
            uint16_t dev    = (uint16_t)(id >> 16);

            devs[ndevs++] = (PciDev){ vendor, dev, (uint8_t)bus, (uint8_t)slot, 0, cls, sub };

            /* WiFi: class 0x02 subclass 0x80 (802.11) */
            if (cls == 0x02 && sub == 0x80) {
                has_wifi = 1;
                if      (vendor == 0x8086) wifi_nm = "Intel WiFi";
                else if (vendor == 0x10EC) wifi_nm = "Realtek WiFi";
                else if (vendor == 0x14E4) wifi_nm = "Broadcom WiFi";
                else if (vendor == 0x168C) wifi_nm = "Qualcomm WiFi";
                else                       wifi_nm = "WiFi Adapter";
            }
            /* Bluetooth: class 0x0E (wireless) sub 0x01 */
            if ((cls == 0x0E && sub == 0x01)) {
                has_bt = 1;
                if      (vendor == 0x8086) bt_nm = "Intel Bluetooth";
                else if (vendor == 0x0A5C) bt_nm = "Broadcom Bluetooth";
                else if (vendor == 0x0BDA) bt_nm = "Realtek Bluetooth";
                else                       bt_nm = "Bluetooth Adapter";
            }
        }
    }
}

int           pci_dev_count(void)    { return ndevs; }
const PciDev *pci_dev(int idx)       { return (idx >= 0 && idx < ndevs) ? &devs[idx] : (void*)0; }
int           pci_has_wifi(void)     { return has_wifi; }
int           pci_has_bluetooth(void){ return has_bt; }
const char   *pci_wifi_name(void)    { return wifi_nm; }
const char   *pci_bt_name(void)      { return bt_nm; }
