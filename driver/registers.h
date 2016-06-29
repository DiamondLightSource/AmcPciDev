/* Interface to register file. */

struct interrupt_control;

/* Called to open the file. */
int lamc_pci_reg_open(
    struct file *file, struct pci_dev *dev,
    struct interrupt_control *interrupts);

extern struct file_operations lamc_pci_reg_fops;
