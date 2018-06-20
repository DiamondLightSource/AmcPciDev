/* Header file for use from userspace. */

/* Although our ioctls don't transfer any data, use the direction hint anyway:
 * this helps valgrind which otherwise complains about missing size hints, and
 * it doesn't seem to mind the zero size code. */
#define AMC_IOCTL(n)        _IOC(_IOC_WRITE, 'L', n, 0)

/* Returns size of register area as unsigned 32-bit integer. */
#define AMC_MAP_SIZE        AMC_IOCTL(0)

/* Locks and unlocks access to register area for exclusive access by caller. */
#define AMC_REG_LOCK        AMC_IOCTL(2)
#define AMC_REG_UNLOCK      AMC_IOCTL(3)

/* Returns size of DMA buffer. */
#define AMC_BUF_SIZE        AMC_IOCTL(1)
