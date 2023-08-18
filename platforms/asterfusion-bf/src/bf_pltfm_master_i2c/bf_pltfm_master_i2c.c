/*!
 * @file bf_pltfm_master_i2c.c
 * @date 2020/03/18
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>	/* for NAME_MAX */
#include <sys/ioctl.h>
#include <string.h>
#include <strings.h>	/* for strcasecmp() */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>

#include <bf_pltfm_types/bf_pltfm_types.h>
#include "bf_pltfm_cgos_i2c.h"
#include <bf_types/bf_types.h>
#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_master_i2c.h>
#include <bf_pltfm_chss_mgmt_intf.h>

#include <pltfm_types.h>

bf_sys_rmutex_t master_i2c_lock;

static bool  bf_sio_dbg_enabled = true;
static FILE *bf_sio_dbg_fp = NULL;
#define SINGLE_LOG_SIZE     (9 * 1024 * 1024)
#define TOTAL_LOG_NUM       50

#define MISSING_FUNC_FMT	"Error: Adapter does not have %s capability\n"

enum adt { adt_dummy, adt_isa, adt_i2c, adt_smbus, adt_unknown };

struct adap_type {
    const char *funcs;
    const char *algo;
};

static struct adap_type adap_types[5] = {
    {
        .funcs	= "dummy",
        .algo	= "Dummy bus",
    },
    {
        .funcs	= "isa",
        .algo	= "ISA bus",
    },
    {
        .funcs	= "i2c",
        .algo	= "I2C adapter",
    },
    {
        .funcs	= "smbus",
        .algo	= "SMBus adapter",
    },
    {
        .funcs	= "unknown",
        .algo	= "N/A",
    },
};

struct i2c_adap {
    int nr;
    char *name;
    const char *funcs;
    const char *algo;
};

struct i2c_ctx_t {
    int fd_suio;
    int fd_cp2112;
    int fd_cp2112_1;
} i2c_ctx = {
    .fd_suio     = -1,
    .fd_cp2112   = -1,
    .fd_cp2112_1 = -1,
};

#define Enter fprintf(stdout, "%s:%d\n", __func__, __LINE__);

int open_i2c_dev (int i2cbus, char *filename,
                  size_t size, int quiet)
{
    int file;

    snprintf (filename, size, "/dev/i2c/%d", i2cbus);
    filename[size - 1] = '\0';
    file = open (filename, O_RDWR);

    if (file < 0 && (errno == ENOENT ||
                     errno == ENOTDIR)) {
        sprintf (filename, "/dev/i2c-%d", i2cbus);
        file = open (filename, O_RDWR);
    }

    if (file < 0 && !quiet) {
        if (errno == ENOENT) {
            LOG_ERROR ("Error: Could not open file "
                       "`/dev/i2c-%d' or `/dev/i2c/%d': %s",
                       i2cbus, i2cbus, oryx_safe_strerror (ENOENT));
        } else {
            LOG_ERROR ("Error: Could not open file "
                       "`%s': %s", filename, oryx_safe_strerror (errno));
            if (errno == EACCES) {
                LOG_ERROR ("Run as root?");
            }
        }
    }

    return file;
}

static enum adt i2c_get_funcs (int i2cbus)
{
    unsigned long funcs;
    int file;
    char filename[20];
    enum adt ret;

    file = open_i2c_dev (i2cbus, filename,
                         sizeof (filename), 1);
    if (file < 0) {
        return adt_unknown;
    }

    if (ioctl (file, I2C_FUNCS, &funcs) < 0) {
        ret = adt_unknown;
    } else if (funcs & I2C_FUNC_I2C) {
        ret = adt_i2c;
    } else if (funcs & (I2C_FUNC_SMBUS_BYTE |
                        I2C_FUNC_SMBUS_BYTE_DATA |
                        I2C_FUNC_SMBUS_WORD_DATA)) {
        ret = adt_smbus;
    } else {
        ret = adt_dummy;
    }

    close (file);
    return ret;
}

/* Remove trailing spaces from a string
   Return the new string length including the trailing NUL */
static int rtrim (char *s)
{
    int i;

    for (i = strlen (s) - 1; i >= 0 && (s[i] == ' ' ||
                                        s[i] == '\n'); i--) {
        s[i] = '\0';
    }
    return i + 2;
}

void free_adapters (struct i2c_adap *adapters)
{
    int i;

    for (i = 0; adapters[i].name; i++) {
        free (adapters[i].name);
    }
    free (adapters);
}

/* We allocate space for the adapters in bunches. The last item is a
   terminator, so here we start with room for 7 adapters, which should
   be enough in most cases. If not, we allocate more later as needed. */
#define BUNCH	8

/* n must match the size of adapters at calling time */
static struct i2c_adap *more_adapters (
    struct i2c_adap *adapters, int n)
{
    struct i2c_adap *new_adapters;

    new_adapters = realloc (adapters,
                            (n + BUNCH) * sizeof (struct i2c_adap));
    if (!new_adapters) {
        free_adapters (adapters);
        return NULL;
    }
    memset (new_adapters + n, 0,
            BUNCH * sizeof (struct i2c_adap));

    return new_adapters;
}

struct i2c_adap *gather_i2c_busses (void)
{
    char s[120];
    struct dirent *de, *dde;
    DIR *dir, *ddir;
    FILE *f;
    char fstype[NAME_MAX], sysfs[NAME_MAX],
         n[NAME_MAX + NAME_MAX + NAME_MAX + NAME_MAX];/* Fixed compiled errors with gcc-8 in debian 10 */
    int foundsysfs = 0;
    int count = 0;
    struct i2c_adap *adapters;

    adapters = calloc (BUNCH,
                       sizeof (struct i2c_adap));
    if (!adapters) {
        return NULL;
    }

    /* look in /proc/bus/i2c */
    if ((f = fopen ("/proc/bus/i2c", "r"))) {
        while (fgets (s, 120, f)) {
            char *algo, *name, *type, *all;
            int len_algo, len_name, len_type;
            int i2cbus;

            algo = strrchr (s, '\t');
            * (algo++) = '\0';
            len_algo = rtrim (algo);

            name = strrchr (s, '\t');
            * (name++) = '\0';
            len_name = rtrim (name);

            type = strrchr (s, '\t');
            * (type++) = '\0';
            len_type = rtrim (type);

            sscanf (s, "i2c-%d", &i2cbus);

            if ((count + 1) % BUNCH == 0) {
                /* We need more space */
                adapters = more_adapters (adapters, count + 1);
                if (!adapters) {
                    return NULL;
                }
            }

            all = malloc (len_name + len_type + len_algo);
            if (all == NULL) {
                free_adapters (adapters);
                return NULL;
            }
            adapters[count].nr = i2cbus;
            adapters[count].name = strcpy (all, name);
            adapters[count].funcs = strcpy (all + len_name,
                                            type);
            adapters[count].algo = strcpy (all + len_name +
                                           len_type,
                                           algo);
            count++;
        }
        fclose (f);
        goto done;
    }

    /* look in sysfs */
    /* First figure out where sysfs was mounted */
    if ((f = fopen ("/proc/mounts", "r")) == NULL) {
        goto done;
    }

    while (fgets (n, NAME_MAX, f)) {
        sscanf (n, "%*[^ ] %[^ ] %[^ ] %*s\n", sysfs,
                fstype);
        if (strcasecmp (fstype, "sysfs") == 0) {
            foundsysfs++;
            break;
        }
    }
    fclose (f);
    if (! foundsysfs) {
        goto done;
    }

    /* Bus numbers in i2c-adapter don't necessarily match those in
       i2c-dev and what we really care about are the i2c-dev numbers.
       Unfortunately the names are harder to get in i2c-dev */
    strcat (sysfs, "/class/i2c-dev");
    if (! (dir = opendir (sysfs))) {
        goto done;
    }
    /* go through the busses */
    while ((de = readdir (dir)) != NULL) {
        if (!strcmp (de->d_name, ".")) {
            continue;
        }
        if (!strcmp (de->d_name, "..")) {
            continue;
        }

        /* this should work for kernels 2.6.5 or higher and */
        /* is preferred because is unambiguous */
        sprintf (n, "%s/%s/name", sysfs, de->d_name);
        f = fopen (n, "r");
        /* this seems to work for ISA */
        if (f == NULL) {
            sprintf (n, "%s/%s/device/name", sysfs,
                     de->d_name);
            f = fopen (n, "r");
        }
        /* non-ISA is much harder */
        /* and this won't find the correct bus name if a driver
           has more than one bus */
        if (f == NULL) {
            sprintf (n, "%s/%s/device", sysfs, de->d_name);
            if (! (ddir = opendir (n))) {
                continue;
            }
            while ((dde = readdir (ddir)) != NULL) {
                if (!strcmp (dde->d_name, ".")) {
                    continue;
                }
                if (!strcmp (dde->d_name, "..")) {
                    continue;
                }
                if ((!strncmp (dde->d_name, "i2c-", 4))) {
                    sprintf (n, "%s/%s/device/%s/name",
                             sysfs, de->d_name, dde->d_name);
                    if ((f = fopen (n, "r"))) {
                        goto found;
                    }
                }
            }
        }

found:
        if (f != NULL) {
            int i2cbus;
            enum adt type;
            char *px;

            px = fgets (s, 120, f);
            fclose (f);
            if (!px) {
                continue;
            }
            if ((px = strchr (s, '\n')) != NULL) {
                *px = 0;
            }
            if (!sscanf (de->d_name, "i2c-%d", &i2cbus)) {
                continue;
            }
            if (!strncmp (s, "ISA ", 4)) {
                type = adt_isa;
            } else {
                /* Attempt to probe for adapter capabilities */
                type = i2c_get_funcs (i2cbus);
            }

            if ((count + 1) % BUNCH == 0) {
                /* We need more space */
                adapters = more_adapters (adapters, count + 1);
                if (!adapters) {
                    return NULL;
                }
            }

            adapters[count].nr = i2cbus;
            adapters[count].name = strdup (s);
            if (adapters[count].name == NULL) {
                free_adapters (adapters);
                return NULL;
            }
            adapters[count].funcs = adap_types[type].funcs;
            adapters[count].algo = adap_types[type].algo;
            count++;
        }
    }
    closedir (dir);

done:
    return adapters;
}

static int lookup_i2c_bus_by_name (
    const char *bus_name)
{
    struct i2c_adap *adapters;
    int i, i2cbus = -1;

    adapters = gather_i2c_busses();
    if (adapters == NULL) {
        LOG_ERROR ("Error: Out of memory!");
        return -3;
    }

    /* Walk the list of i2c busses, looking for the one with the
       right name */
    for (i = 0; adapters[i].name; i++) {
        if (strcmp (adapters[i].name, bus_name) == 0) {
            if (i2cbus >= 0) {
                LOG_ERROR (
                    "Error: I2C bus name is not unique!");
                i2cbus = -4;
                goto done;
            }
            i2cbus = adapters[i].nr;
        }
    }

    if (i2cbus == -1)
        LOG_WARNING ("Warning: I2C bus name doesn't match any "
                   "bus present!");

done:
    free_adapters (adapters);
    return i2cbus;
}

/*
 * Parse an I2CBUS command line argument and return the corresponding
 * bus number, or a negative value if the bus is invalid.
 */
int lookup_i2c_bus (const char *i2cbus_arg)
{
    unsigned long i2cbus;
    char *end;

    i2cbus = strtoul (i2cbus_arg, &end, 0);
    if (*end || !*i2cbus_arg) {
        /* Not a number, maybe a name? */
        return lookup_i2c_bus_by_name (i2cbus_arg);
    }
    if (i2cbus > 0xFFFFF) {
        LOG_ERROR ("Error: I2C bus out of range!");
        return -2;
    }

    return i2cbus;
}

/*
 * Parse a CHIP-ADDRESS command line argument and return the corresponding
 * chip address, or a negative value if the address is invalid.
 */
int parse_i2c_address (const char *address_arg)
{
    long address;
    char *end;

    address = strtol (address_arg, &end, 0);
    if (*end || !*address_arg) {
        LOG_ERROR ("Error: Chip address is not a number!");
        return -1;
    }
    if (address < 0x03 || address > 0x77) {
        LOG_ERROR ("Error: Chip address out of range "
                   "(0x03-0x77)!");
        return -2;
    }

    return address;
}

int check_funcs (int file, int size, int pec)
{
    unsigned long funcs;

    /* check adapter functionality */
    if (ioctl (file, I2C_FUNCS, &funcs) < 0) {
        LOG_ERROR ("Error: Could not get the adapter "
                   "functionality matrix: %s", oryx_safe_strerror (errno));
        return -1;
    }

    switch (size) {
        case I2C_SMBUS_BYTE:
            if (! (funcs & I2C_FUNC_SMBUS_WRITE_BYTE)) {
                LOG_ERROR (MISSING_FUNC_FMT, "SMBus send byte");
                return -1;
            }
            break;

        case I2C_SMBUS_BYTE_DATA:
            if (! (funcs & I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
                LOG_ERROR (MISSING_FUNC_FMT, "SMBus write byte");
                return -1;
            }
            break;

        case I2C_SMBUS_WORD_DATA:
            if (! (funcs & I2C_FUNC_SMBUS_WRITE_WORD_DATA)) {
                LOG_ERROR (MISSING_FUNC_FMT, "SMBus write word");
                return -1;
            }
            break;

        case I2C_SMBUS_BLOCK_DATA:
            if (! (funcs & I2C_FUNC_SMBUS_WRITE_BLOCK_DATA)) {
                LOG_ERROR (MISSING_FUNC_FMT, "SMBus block write");
                return -1;
            }
            break;
        case I2C_SMBUS_I2C_BLOCK_DATA:
            if (! (funcs & I2C_FUNC_SMBUS_WRITE_I2C_BLOCK)) {
                LOG_ERROR (MISSING_FUNC_FMT, "I2C block write");
                return -1;
            }
            break;
    }

    if (pec
        && ! (funcs & (I2C_FUNC_SMBUS_PEC |
                       I2C_FUNC_I2C))) {
        LOG_ERROR ("Warning: Adapter does "
                   "not seem to support PEC");
    }

    return 0;
}

static int bf_pltfm_master_i2c_select(uint8_t slave_addr)
{
    int fd;

    /* Before reading eeprom, we have no ideal which is the master i2c for x312p-t. */
    fd = i2c_ctx.fd_suio > 0 ? i2c_ctx.fd_suio : i2c_ctx.fd_cp2112;

    /* if we have known the platform, it's much easier to select the good one. */
    if (platform_type_equal (X308P)) {
        /* BMC  <- uart     */
        /* CPLD <- nct6679d */
        /* QSFP <- cp2112   */
        fd = i2c_ctx.fd_suio;
    } else if (platform_type_equal(X532P) ||
               platform_type_equal(X564P)) {
        if (is_HVXXX) {
            /* When master i2c changed to super IO. */
            fd = i2c_ctx.fd_suio;
        }
    } else if (platform_type_equal (X312P)) {
        if (platform_subtype_equal(v2dot0)) {
            /* BMC  <- cp2112 */
            /* CPLD <- cp2112  */
            /* QSFP <- cp2112 */
            fd = i2c_ctx.fd_cp2112;
        } else if (platform_subtype_equal(v3dot0) ||
                   platform_subtype_equal(v4dot0) ||
                   platform_subtype_equal(v5dot0)) {
            /* BMC  <- nc76779d */
            /* CPLD <- nc76779d */
            /* QSFP <- cp2112   */
            switch (slave_addr) {
                case 0x60:          //X312P_CPLD1_ADDR
                case 0x62:          //X312P_CPLD3_ADDR
                case 0x64:          //X312P_CPLD4_ADDR
                case 0x66:          //X312P_CPLD5_ADDR
                case 0x3E:          //X312P_BMC_ADDR
                    fd = i2c_ctx.fd_suio;
                    break;
                default:
                    fd = i2c_ctx.fd_cp2112;
                    break;
            }
        }
    } 
    BUG_ON(fd <= 0);
    return fd;
}

static void bf_pltfm_master_i2c_log(
    uint8_t rd_wr,
    uint8_t slave,
    uint8_t wr_off,
    uint8_t *wr_buf,
    uint8_t wr_len) {

    char debug_str[512];
    char rd_wr_str[16];
    time_t tmpcal_ptr;
    struct tm *tmp_ptr;
    struct stat file_stat;
    char *dbgstr_ptr = debug_str;
    char rfile[512] = {0};
    const char *dbg_file = LOG_DIR_PREFIX"/sio_debug.log";

    if (!bf_sio_dbg_enabled ||
        !platform_type_equal(X312P) ||
        ((slave != 0x60) &&
        (slave != 0x62) &&
        (slave != 0x64) &&
        (slave != 0x66) &&
        (slave != 0x3E))) {
        return;
    }

    time(&tmpcal_ptr);
    tmp_ptr = localtime(&tmpcal_ptr);

    switch (rd_wr) {
        case 0:
            strcpy (rd_wr_str, "RD");
            break;
        case 1:
            strcpy (rd_wr_str, "WR");
            break;
    }

    dbgstr_ptr += sprintf (debug_str, "%4d-%2d-%2d %2d:%2d:%2d (UTC) %s addr: 0x%02x, len:%2d, data: 0x%02x",
                                      1900+tmp_ptr->tm_year, 1+tmp_ptr->tm_mon, tmp_ptr->tm_mday,
                                      tmp_ptr->tm_hour, tmp_ptr->tm_min, tmp_ptr->tm_sec,
                                      rd_wr_str, slave, wr_len, wr_off);
    for (int i = 0; i < wr_len; i ++) {
        dbgstr_ptr += sprintf (dbgstr_ptr, "-%02x", wr_buf[i]);
    }
    dbgstr_ptr += sprintf (dbgstr_ptr, "\n");

    if (bf_sio_dbg_enabled) {
        if (bf_sio_dbg_fp == NULL) {
            bf_sio_dbg_fp = fopen (dbg_file, "a");
            if (bf_sio_dbg_fp == NULL) {
                LOG_ERROR ("Warning: %s", strerror (errno));
                exit (0);
            }
        }
    }

    if (bf_sio_dbg_fp) {
        fwrite (debug_str, 1, strlen(debug_str), bf_sio_dbg_fp);
        fflush (bf_sio_dbg_fp);
        stat (dbg_file, &file_stat);
        if (file_stat.st_size > SINGLE_LOG_SIZE) {
            /* Create a new file next time. */
            fclose (bf_sio_dbg_fp);
            bf_sio_dbg_fp = NULL;
            sprintf (rfile, "%s.%lu", dbg_file, tmpcal_ptr);
            rename (dbg_file, rfile);

            int i, j, file_num = 0;
            DIR *dir;
            struct dirent *ptr;
            long int tmp, tmpcal[TOTAL_LOG_NUM + 5];

            if ((dir = opendir (LOG_DIR_PREFIX)) == NULL) {
                LOG_ERROR ("Open /var/asterfusion failed!");
                return;
            }

            while ((ptr = readdir (dir)) != NULL) {
                if (strcmp (ptr->d_name, ".") == 0 || strcmp (ptr->d_name, "..") == 0) {
                    continue;
                } else if (ptr->d_type == 8) {
                    if (ptr->d_name != NULL && strstr(ptr->d_name, "sio_debug") != NULL) {
                        sprintf (rfile, "%s/%s", LOG_DIR_PREFIX, ptr->d_name);
                        stat (rfile, &file_stat);
                        tmpcal[file_num++] = file_stat.st_ctime;
                        if (file_num >= TOTAL_LOG_NUM + 5) {
                            break;
                        }
                    }
                }
            }
            closedir(dir);

            if (file_num > TOTAL_LOG_NUM) {
                for (i = 0; i < file_num - 1; ++i) {
                    for (j = 0; j < file_num - 1 - i; ++j) {
                        if (tmpcal[j] < tmpcal[j + 1]) {
                            tmp = tmpcal[j];
                            tmpcal[j] = tmpcal[j + 1];
                            tmpcal[j + 1] = tmp;
                        }
                    }
                }
                for (i = TOTAL_LOG_NUM; i < file_num; i++) {
                    sprintf (rfile, "%s.%lu", dbg_file, tmpcal[i]);
                    remove (rfile);
                }
            }
        }
    }
}

int bf_pltfm_master_i2c_read_byte (
    uint8_t slave,
    uint8_t rd_off,
    uint8_t *value)
{
    /* sp for COM-e: CG15XX */
    if (is_CG15XX && (platform_type_equal (X532P) || platform_type_equal (X564P))) {
        return bf_cgos_i2c_read_byte (slave, rd_off,
                                      value);
    }
    int force = 0;
    int err;
    int fd = -1;

    fd = bf_pltfm_master_i2c_select(slave);

    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    err = ioctl (fd,
                 force ? I2C_SLAVE_FORCE : I2C_SLAVE, slave);
    if (err < 0) {
        LOG_ERROR (
            "%s[%d], "
            "i2c.ioctl(%d): slave=0x%02x : %s"
            "\n",
            __FILE__, __LINE__, fd, slave, oryx_safe_strerror (errno));
        return -100;
    }

    int32_t val = i2c_smbus_read_byte_data (fd, rd_off);
    if (val < 0) {
        LOG_ERROR (
            "%s[%d], "
            "i2c.read(%d): slave=0x%02x : rd_off=%d : %s"
            "\n",
            __FILE__, __LINE__, fd, slave, rd_off, oryx_safe_strerror (errno));
        return -200;
    }

    *value = val;

    bf_pltfm_master_i2c_log(0, slave, rd_off, value, 1);

    return BF_PLTFM_SUCCESS;
}

int bf_pltfm_master_i2c_read_block (
    uint8_t slave,
    uint8_t rd_off,
    uint8_t *rd_buf,
    uint8_t  rd_len)
{
    /* sp for COM-e: CG15XX */
    if (is_CG15XX && (platform_type_equal (X532P) || platform_type_equal (X564P))) {
        return bf_cgos_i2c_read_block (slave, rd_off,
                                       rd_buf, rd_len);
    }
    int force = 0;
    int err;
    int fd = -1;

    fd = bf_pltfm_master_i2c_select(slave);

    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    err = ioctl (fd,
                 force ? I2C_SLAVE_FORCE : I2C_SLAVE, slave);
    if (err < 0) {
        LOG_ERROR (
            "%s[%d], "
            "i2c.ioctl(%d): slave=0x%02x : %s"
            "\n",
            __FILE__, __LINE__, fd, slave, oryx_safe_strerror (errno));
        return -100;
    }

    int i = 0;
    int off = rd_off;
    int32_t val = 0;

#if 1
    for (off = rd_off; off < (int) (rd_off + rd_len);
         off ++) {
        val = i2c_smbus_read_byte_data (fd, off);
        if (val < 0) {
            LOG_WARNING (
                "%s[%d], "
                "i2c.read(%d): slave=0x%02x : rd_off=%d : off=%d : %s"
                "\n",
                __FILE__, __LINE__, fd, slave, rd_off, off, oryx_safe_strerror (errno));
            return -200;
        }
        rd_buf[i ++] = val;
        usleep (1000);
    }
#else
    int times = rd_len / 32;
    fprintf(stdout, "1. rd_off : %d, rd_len : %d, times = %d\n", rd_off, rd_len, times);
    for (i = 0; i < times; i ++) {
        err = i2c_smbus_read_block_data (fd, rd_off,
                                             rd_buf);
        if (err < 0) {
            LOG_ERROR (
                "%s[%d], "
                "i2c.read(%d): slave=0x%02x : rd_off=%d : off=%d : %s"
                "\n",
                __FILE__, __LINE__, fd, slave, rd_off, off, oryx_safe_strerror (errno));
            return -200;
        }
        rd_buf += 32;
        rd_off += 32;
        rd_len -= 32;
        usleep (50000);
    }

    fprintf(stdout, "2. rd_off : %d, rd_len : %d\n", rd_off, rd_len);
    for (off = rd_off; off < (int) (rd_off + rd_len);
         off ++) {
        val = i2c_smbus_read_byte_data (fd, off);
        if (val < 0) {
            LOG_ERROR (
                "%s[%d], "
                "i2c.read(%d): slave=0x%02x : rd_off=%d : off=%d : %s"
                "\n",
                __FILE__, __LINE__, fd, slave, rd_off, off, oryx_safe_strerror (errno));
            return -200;
        }
        rd_buf[i ++] = val;
        usleep (50000);
    }

#if 0
    /* Doesn't support I2C block W/R method */
    val = i2c_smbus_read_block_data (fd, off,
                                     &rd_buf[0]);
    if (val < 0) {
        LOG_ERROR (
            "%s[%d], "
            "i2c.read(%d): slave=0x%02x : rd_off=%d : off=%d : %s"
            "\n",
            __FILE__, __LINE__, fd, slave, rd_off, off, oryx_safe_strerror (errno));
        return -200;
    }
#endif
#endif

    bf_pltfm_master_i2c_log(0, slave, rd_off, rd_buf, rd_len);

    return BF_PLTFM_SUCCESS;
}

int bf_pltfm_master_i2c_write_byte (
    uint8_t slave,
    uint8_t wr_off,
    uint8_t value)
{
    /* sp for COM-e: CG15XX */
    if (is_CG15XX && (platform_type_equal (X532P) || platform_type_equal (X564P))) {
        return bf_cgos_i2c_write_byte (slave, wr_off,
                                       value);
    }
    int force = 0;
    int err;
    int fd = -1;

    bf_pltfm_master_i2c_log(1, slave, wr_off, &value, 1);

    fd = bf_pltfm_master_i2c_select(slave);

    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    err = ioctl (fd,
                 force ? I2C_SLAVE_FORCE : I2C_SLAVE, slave);
    if (err < 0) {
        LOG_ERROR (
            "%s[%d], "
            "i2c.ioctl(%d): slave=0x%02x : %s"
            "\n",
            __FILE__, __LINE__, fd, slave, oryx_safe_strerror (errno));
        return -100;
    }

    err = i2c_smbus_write_byte_data (fd, wr_off,
                                     value);
    if (err < 0) {
        LOG_ERROR (
            "%s[%d], "
            "i2c.write(%d): slave=0x%02x : rd_off=%d : %s"
            "\n",
            __FILE__, __LINE__, fd, slave, wr_off, oryx_safe_strerror (errno));
        return -200;
    }

    return BF_PLTFM_SUCCESS;
}

int bf_pltfm_master_i2c_write_block (
    uint8_t slave,
    uint8_t wr_off,
    uint8_t *wr_buf,
    uint8_t  wr_len)
{
    /* sp for COM-e: CG15XX */
    if (is_CG15XX && (platform_type_equal (X532P) || platform_type_equal (X564P))) {
        return bf_cgos_i2c_write_block (slave, wr_off,
                                        wr_buf, wr_len);
    }
    int force = 0;
    int err;
    int fd = -1;

    bf_pltfm_master_i2c_log(1, slave, wr_off, wr_buf, wr_len);

    fd = bf_pltfm_master_i2c_select(slave);

    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    err = ioctl (fd,
                 force ? I2C_SLAVE_FORCE : I2C_SLAVE, slave);
    if (err < 0) {
        LOG_ERROR (
            "%s[%d], "
            "i2c.ioctl(%d): slave=0x%02x : %s"
            "\n",
            __FILE__, __LINE__, fd, slave, oryx_safe_strerror (errno));
        return -100;
    }

    int i = 0;
    int off = wr_off;

    for (off = wr_off; off < (int) (wr_off + wr_len);
         off ++) {
        err = i2c_smbus_write_byte_data (fd, off,
                                         wr_buf[i ++]);
        if (err < 0) {
            LOG_ERROR (
                "%s[%d], "
                "i2c.write(%d): slave=0x%02x : wr_off=%d : off=%d : %s"
                "\n",
                __FILE__, __LINE__, fd, slave, wr_off, off, oryx_safe_strerror (errno));
            return -200;
        }
        usleep (1000);
    }

    return BF_PLTFM_SUCCESS;
}

// this func returns read_len if read success, -1 when fail
int bf_pltfm_bmc_write_read (
    uint8_t slave,
    uint8_t wr_off,
    uint8_t *wr_buf,
    uint8_t wr_len,
    uint8_t rd_off,
    uint8_t *rd_buf,
    int     max_rd_len,
    int usec)
{
    /* sp for COM-e: CG15XX */
    if (is_CG15XX && (platform_type_equal (X532P) || platform_type_equal (X564P))) {
        return bf_cgos_i2c_bmc_read (slave,
                                     wr_off, wr_buf, wr_len, rd_buf,
                                     usec);
    }
    int force = 0;
    int err = BF_PLTFM_SUCCESS;
    int fd = -1;

    bf_pltfm_master_i2c_log(1, slave, wr_off, wr_buf, wr_len);

    fd = bf_pltfm_master_i2c_select(slave);

    MASTER_I2C_LOCK;

    /* With force, let the user read from/write to the registers
       even when a driver is also running */
    err = ioctl (fd,
                 force ? I2C_SLAVE_FORCE : I2C_SLAVE, slave);
    if (err < 0) {
        LOG_ERROR (
            "%s[%d], "
            "Access BMC Error: i2c.ioctl(%d): slave=0x%02x : %s"
            "\n",
            __FILE__, __LINE__, fd, slave, oryx_safe_strerror (errno));
        err = -100;
        goto end;
    }

    /* Is the data ready to write ? */
    if (wr_buf || wr_len) {
        err = i2c_smbus_write_block_data (fd,
                                          wr_off, wr_len, wr_buf);
        if (err < 0) {
            LOG_ERROR (
                "%s[%d], "
                "Access BMC Error: i2c.write(%d): slave=0x%02x : wr_off=%d : wr_len=%d : %s"
                "\n",
                __FILE__, __LINE__, fd, slave, wr_off, wr_len, oryx_safe_strerror (errno));
            err = -200;
            goto end;
        }

        usleep (usec);
    }

    /* Is the buffer ready to read ? */
    if (rd_buf) {
        uint8_t rd_data[256 + 2];
        err = i2c_smbus_read_block_data (fd,
                                         rd_off, rd_data);
        if (err < 0) {
            LOG_ERROR (
                "%s[%d], "
                "Access BMC Error: i2c.read(%d): slave=0x%02x : rd_off=%d : %s"
                "\n",
                __FILE__, __LINE__, fd, slave, rd_off, oryx_safe_strerror (errno));
            err = -300;
            goto end;
        }

        if ((rd_data[0] + 1 != err) || (err > max_rd_len))  {
            LOG_ERROR (
                "%s[%d], "
                "Access BMC Error: i2c.read(%d): slave=0x%02x : rd_off=%d : %s : rd_len=%d : max_rd_len=%d"
                "\n",
                __FILE__, __LINE__, fd, slave, rd_off, "data format error", err, max_rd_len);
            err = -400;
            goto end;
        }

        memcpy (rd_buf, rd_data, err);

        bf_pltfm_master_i2c_log(0, slave, rd_buf[0], &rd_buf[1], err-1);
    }

end:
    MASTER_I2C_UNLOCK;
    return err;
}

int bf_pltfm_master_i2c_init()
{
    bool cp2112_i2c = false;
    char filename[20] = {0};
    struct i2c_ctx_t *i2c = &i2c_ctx;
    int i2cbus;
    int fd;

    /* Error of "I2C bus name doesn't match any bus present!" occured
     * when find CP2112 during second launching of switchd.
     * The reason why this happen is that libusb would try to detach to kernel
     * at every launch and we couldn't see by 'i2cdetect -l' after detach successfully.
     * At this moment I don't see any negative effects with such an error/warnning.
     * by tsihang, 2022-04-20. */
    char i2c_bus_name[5][256] = {
        /* hidraw0 or hiddev0 is the same thing in dfferent OS. */
        "CP2112 SMBus Bridge on hidraw0",
        "CP2112 SMBus Bridge on hiddev0",
        /* hidraw1 or hiddev1 is the same thing in dfferent OS. */
        "CP2112 SMBus Bridge on hidraw1",
        "CP2112 SMBus Bridge on hiddev1",
        "sio_smbus"
    };

    if (platform_type_equal (X312P)) {
        if (platform_subtype_equal (v1dot0)) {
            memset(&i2c_bus_name[0][0], '0', 255);
            memset(&i2c_bus_name[1][0], '0', 255);
            memset(&i2c_bus_name[2][0], '0', 255);
            memset(&i2c_bus_name[3][0], '0', 255);
        } else if (platform_subtype_equal (v2dot0)) {
            memset(&i2c_bus_name[2][0], '0', 255);
            memset(&i2c_bus_name[3][0], '0', 255);
            memset(&i2c_bus_name[4][0], '0', 255);
        } else if (platform_subtype_equal (v3dot0) ||
                   platform_subtype_equal (v4dot0) ||
                   platform_subtype_equal (v5dot0)) {
            memset(&i2c_bus_name[2][0], '0', 255);
            memset(&i2c_bus_name[3][0], '0', 255);
        }
    } else if (platform_type_equal (X308P)) {
        memset(&i2c_bus_name[0][0], '0', 255);
        memset(&i2c_bus_name[1][0], '0', 255);
        memset(&i2c_bus_name[2][0], '0', 255);
        memset(&i2c_bus_name[3][0], '0', 255);
        if (!is_HVXXX) {
            memset(&i2c_bus_name[4][0], '0', 255);
        }
    } else if (platform_type_equal (X532P) ||
               platform_type_equal (X564P)) {
        memset(&i2c_bus_name[0][0], '0', 255);
        memset(&i2c_bus_name[1][0], '0', 255);
        memset(&i2c_bus_name[2][0], '0', 255);
        memset(&i2c_bus_name[3][0], '0', 255);
        if (!is_HVXXX) {
            memset(&i2c_bus_name[4][0], '0', 255);
        }
    }

    if (bf_sys_rmutex_init (&master_i2c_lock) != 0) {
        LOG_ERROR ("pltfm_mgr: i2c lock init failed\n");
        return -1;
    }

    if (global_come_type == COME_UNKNOWN) {
        fprintf (stdout, "X86 Come type unspecified.\n");
        exit (1);
    } else {
        fprintf (stdout,
                 "\n\n================== %s ==================\n", "IIC INIT");
        /* X564P-T V1.0, V1.1 & V1.2
         * X532P-T v1.1 */
        if (is_CG15XX &&
            (platform_type_equal (X564P) || platform_type_equal (X532P) || platform_type_equal (X308P)) &&
            (bf_pltfm_find_path_to_cpld () == VIA_CGOS)) {
            if (bf_cgos_init()) {
                fprintf (stdout, "Error in cgos init\n");
                exit (1);
            } else {
                fprintf (stdout, "cgoslx init done\n");
                return 0;
            }
        } else {
            if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CPLD_CP2112) {
                fprintf (stdout, "Skip ...\n");
                return 0;
            }

            for (int i = 0; i < (int)ARRAY_LENGTH (i2c_bus_name); i ++) {
                cp2112_i2c = false;
                //fprintf(stdout, "-> %s\n", i2c_bus_name[i]);
                i2cbus = lookup_i2c_bus(i2c_bus_name[i]);
                if ((i2c_bus_name[i][0] == '0') ||
                    (i2cbus < 0)) {
                    continue;
                }
                if (strstr (i2c_bus_name[i], "CP2112")) {
                    cp2112_i2c = true;
                }
                /* At the very beginnig we don't know which i2c-channel is selected
                 * to read eeprom to get correct hw version. So MAKE SURE that a correct
                 * i2c channel is given by /etc/platform.conf under cme3000/cme7000.
                 * by tsihang, 2022-04-19. */
                /* For x312p-t v2. */
                fd = open_i2c_dev (i2cbus, filename,
                            sizeof (filename), 0);
                if (fd < 0) {
                    exit (1);
                }
                fprintf (stdout,
                        "Openning i2c-%d : %d : %s\n", i2cbus, fd, i2c_bus_name[i]);

                if (cp2112_i2c) {
                    i2c->fd_cp2112 = fd;
                    if (i2cbus == bmc_i2c_bus) {
                        /* For x312p-t v2. Map cp2112 i2c to master i2c. */
                        i2c->fd_suio = fd;
                        fprintf (stdout,
                            "Master i2c changed to i2c-%d : %s\n", i2cbus, i2c_bus_name[i]);
                    }
                } else {
                    if (i2cbus != bmc_i2c_bus) {
                        fprintf (stdout, "Closing i2c-%d : %d : %s\n", i2cbus, fd, i2c_bus_name[i]);
                        close (fd);
                    } else {
                        /* For x312p-t v1 and v3. */
                        i2c->fd_suio = fd;
                        fprintf (stdout,
                            "Master i2c changed to i2c-%d : %s\n", i2cbus, i2c_bus_name[i]);
                    }
                }
            }
        }

        fprintf (stdout, "Master i2c-%d init done !\n",
                 bmc_i2c_bus);
        LOG_DEBUG ("Master i2c-%d init done !",
                   bmc_i2c_bus);
    }

    return 0;
}

int bf_pltfm_master_i2c_de_init()
{
    struct i2c_ctx_t *i2c = &i2c_ctx;

    fprintf(stdout, "================== Deinit .... %48s ================== \n",
        __func__);

    if (global_come_type == COME_UNKNOWN) {
        fprintf (stdout, "Skip ...\n");
        goto finish;
    } else {
        if (is_CG15XX &&
            (platform_type_equal (X564P) || platform_type_equal (X532P) || platform_type_equal (X308P)) &&
            (bf_pltfm_find_path_to_cpld () == VIA_CGOS)) {
            if (bf_cgos_de_init ()) {
                fprintf (stdout, "Deinit Master i2c\n");
                goto finish;
            }
        } else {
            if (i2c->fd_suio > 0) {
                fprintf (stdout, "Deinit Master I2C ...\n");
                close (i2c->fd_suio);
                i2c->fd_suio = -1;
            }
            if (i2c->fd_cp2112 > 0) {
                close (i2c->fd_cp2112);
                fprintf (stdout, "Deinit CP2112 I2C ...\n");
                i2c->fd_cp2112 = -1;
            }

            if (bf_sio_dbg_fp) {
                fflush (bf_sio_dbg_fp);
                fclose (bf_sio_dbg_fp);
                bf_sio_dbg_fp = NULL;
            }
        }
    }

finish:
    bf_sys_rmutex_del (&master_i2c_lock);
    fprintf(stdout, "================== Deinit done %48s ================== \n",
        __func__);

    return 0;
}

