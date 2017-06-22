/* Header file for use from userspace. */

/* Returns size of register area as unsigned 32-bit integer. */
#define LAMC_MAP_SIZE       _IO('L', 0)

/* Locks and unlocks access to register area for exclusive access by caller. */
#define LAMC_REG_LOCK       _IO('L', 2)
#define LAMC_REG_UNLOCK     _IO('L', 3)

/* Returns size of DMA buffer. */
#define LAMC_BUF_SIZE       _IO('L', 1)
