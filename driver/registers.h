/* Interface to register file. */

/* Called to open the file. */
int lamc_pci_reg_open(struct file *file, struct pci_dev *dev);

/* Called to push interrupt events to user space. */
void event_interrupt(uint32_t events);

extern struct file_operations lamc_pci_reg_fops;
