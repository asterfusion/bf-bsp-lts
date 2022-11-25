/*!
 * @file tofino_pci_util.c
 * @date 2022/11/14
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <inttypes.h>  //strlen
#include <errno.h>

static uint8_t *tofino_base_addr = NULL;

static size_t tofino2_bar_size = (128 * 1024 *
                                  1024);
static size_t tofino1_bar_size = (64 * 1024 *
                                  1024);
static size_t tofino_bar_size = 0;

static void tofino_deinit (int tofino_id)
{
    munmap (tofino_base_addr, tofino_bar_size);
}

static int tofino_init (void *arg)
{
    int dev_fd, tofino_id;
    char tofino_name[32];

    tofino_id = (int) (uintptr_t)arg;
    snprintf (tofino_name, sizeof (tofino_name),
              "%s%d", "/dev/bf", tofino_id);
    dev_fd = open (tofino_name, O_RDWR);
    if (dev_fd < 0) {
        printf ("Error in opening %s\n", tofino_name);
        return -1;
    }
    tofino_bar_size = tofino2_bar_size;
    tofino_base_addr = mmap (
                           NULL, tofino2_bar_size, PROT_READ | PROT_WRITE,
                           MAP_SHARED, dev_fd, 0);
    if (tofino_base_addr == (uint8_t *) -1) {
        tofino_base_addr = mmap (
                               NULL, tofino1_bar_size, PROT_READ | PROT_WRITE,
                               MAP_SHARED, dev_fd, 0);
        if (tofino_base_addr) {
            tofino_bar_size = tofino1_bar_size;
        }
    }
    if (tofino_base_addr == (uint8_t *) -1) {
        printf ("Error in mmapping %s\n", tofino_name);
        tofino_bar_size = 0;
        close (dev_fd);
        return -1;
    }
    return 0;
}

static char *usage[] = {
    "tofino_pci_util reg_read <tofino_id> <reg_addr> [count] [addr increment]",
    "tofino_pci_util reg_write <tofino_id> <reg_addr> <32bit data>"
};

static int proc_reg_read (int fd, int count,
                          char *cmd[])
{
    uint32_t reg = 0;
    uint32_t val = 0;
    uint32_t stride = 0;
    int iterations = 1;

    reg = strtol (cmd[0], NULL, 0);
    if (count > 3) {
        iterations = atoi (cmd[1]);
    }
    if (count > 4) {
        stride = strtol (cmd[2], NULL, 0);
        stride &= 0xfffffffc; /* must be multiple of 4 */
    }
    while (iterations-- > 0) {
        if (reg >= tofino_bar_size) {
            printf ("offset %x larger than BAR0 size %zx\n",
                    reg, tofino_bar_size);
            return 1;
        }
        val = * (volatile uint32_t *) (tofino_base_addr +
                                       reg);
        reg += stride;
    }
    printf ("register offset 0x%x is 0x%x\n",
            reg - stride, val);
    return 0;
}

static int proc_reg_write (int fd, int count,
                           char *cmd[])
{
    uint32_t reg, val;

    if (count < 4) {
        printf ("Error: Insufficient arguments\nUsage : %s\n",
                usage[1]);
        return -1;
    }

    reg = strtol (cmd[0], NULL, 0);
    val = strtol (cmd[1], NULL, 0);
    if (reg >= tofino_bar_size) {
        printf ("offset %x larger than BAR0 size %zx\n",
                reg, tofino_bar_size);
        return 1;
    }

    * (volatile uint32_t *) (tofino_base_addr + reg) =
        val;
    printf ("wrote 0x%x at register offset 0x%x\n",
            val, reg);
    return 0;
}

static int process_request (int fd, int count,
                            char *cmd[])
{
    if (strcmp (cmd[0], "reg_read") == 0) {
        return proc_reg_read (fd, count, &cmd[2]);
    } else if (strcmp (cmd[0], "reg_write") == 0) {
        return proc_reg_write (fd, count, &cmd[2]);
    } else {
        printf ("Error: Invalid command\n");
        return -1;
    }
}

int main (int argc, char *argv[])
{
    if (argc < 4) {
        uint32_t i = 0;
        printf ("Usage: \n");
        for (i = 0;
             i < (sizeof (usage) / sizeof (usage[0])); i++) {
            printf ("%s\n", usage[i]);
        }
        return 1;
    }
    uint8_t tofino_id = (uint8_t)strtol (argv[2],
                                         NULL, 0);
    if (tofino_init ((void *) (uintptr_t)tofino_id)) {
        printf (
            "Error: Not able to initialize the tofino device. Check device on PCI "
            "and/or BAR0 size\n");
        return 1;
    }
    int ret = process_request (tofino_id, argc - 1,
                               &argv[1]);
    tofino_deinit (tofino_id);
    return ret;
}
