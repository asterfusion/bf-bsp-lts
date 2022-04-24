#include "onie_tlvinfo.h"

int process_request (int count, char *cmd[]);

static char start_offset[16];
const char *cmd1[] = {"0", "write", "1", "0xe8", "1", "0x40"};
const char *cmd2[] = {
    /*"0", "addr-read-unsafe",*/ "1", "0xa0", "60", "2", "0", start_offset
};

int read_sys_eeprom (unsigned char *eeprom_hdr,
                     int offset, int size)
{
    unsigned int len = 0;
    process_request (6, cmd1);
    //  process_request(8, cmd2);
    // loop 60 bytes at a time and read
    while (len < size) {
        snprintf (start_offset, 8, "0x%02x",
                  len + offset);
        proc_addr_read (8, cmd2, true, eeprom_hdr + len);
        len += 60;
    }
    return 0;
}

int write_sys_eeprom (unsigned char *eeprom,
                      int eeprom_len)
{
    return 0;
}

int main (int argc, char **argv)
{
    static uint8_t eeprom[2048];
    bf_pltfm_status_t sts;
    sts = bf_pltfm_cp2112_util_init (
              BF_PLTFM_BD_ID_MONTARA_P0C);
    read_sys_eeprom (eeprom, 0, 2048);
    if (!is_checksum_valid (eeprom)) {
        show_eeprom (eeprom);
        return -1;
    }
    //
    // get TLV INFO
    tlvinfo_header_t *hdr = (tlvinfo_header_t *)
                            eeprom;
    uint32_t totalLen = be16_to_cpu (hdr->totallen);
    //  printf("\ntotal length: %d\n", totalLen);
    uint32_t len = 0;
    static char ret[128];
    memset (ret, 0, 128);
    while (len < totalLen) {
        tlvinfo_tlv_t *t =
            (tlvinfo_tlv_t *) ((uint8_t *)eeprom + len +
                               sizeof (tlvinfo_header_t));
        if (t->type == ((argc > 1) ? atoi (
                            argv[1]) : 0x22)) {
            // return the string in stdout
            strncpy (ret, t->value, t->length);
            printf ("%s\n", ret);
            return 0;
        }
        if (t->type == 0) {
            break;
        }
        //    printf("Type = 0x%x\n", t->type);
        len += t->length + 2;
    }
    return -1;
}
