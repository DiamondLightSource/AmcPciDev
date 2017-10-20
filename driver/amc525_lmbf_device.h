/* Header file for use from userspace. */

/* Although our ioctls don't transfer any data, use the direction hint anyway:
 * this helps valgrind which otherwise complains about missing size hints, and
 * it doesn't seem to mind the zero size code. */
#define LAMC_IOCTL(n)       _IOC(_IOC_WRITE, 'L', n, 0)

/* Returns size of register area as unsigned 32-bit integer. */
#define LAMC_MAP_SIZE       LAMC_IOCTL(0)

/* Locks and unlocks access to register area for exclusive access by caller. */
#define LAMC_REG_LOCK       LAMC_IOCTL(2)
#define LAMC_REG_UNLOCK     LAMC_IOCTL(3)

/* Returns size of DMA buffer. */
#define LAMC_BUF_SIZE       LAMC_IOCTL(1)
