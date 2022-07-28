#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>

static char *usage[] = {
    "uart_util </dev/ttySX> <BMC CMD> ",
    "Example: uart_util /dev/ttyS1 0xd 0xaa 0xaa"
};

static const unsigned short ccitt_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

int send_handle (int fd, char *order)
{
    unsigned int n = write (fd, order, strlen (order));
    n = n;
    return 0;
}

int set_opt (int fd, int nSpeed, int nBits,
             char nEvent, int nStop)
{

    struct termios newtio, oldtio;
    if (tcgetattr (fd, &oldtio) != 0) {
        perror ("SetupSerial");
        return -1;
    }

    bzero (&newtio, sizeof (newtio));

    newtio.c_cflag |= CLOCAL | CREAD;

    newtio.c_cflag &= ~CSIZE;

    switch (nBits) {
        case 7:
            newtio.c_cflag |= CS7;
            break;
        case 8:
            newtio.c_cflag |= CS8;
            break;
    }

    switch (nEvent) {
        case 'O':
            newtio.c_cflag |= PARENB;
            newtio.c_cflag |= PARODD;
            newtio.c_iflag |= (INPCK | ISTRIP);
            break;
        case 'E':
            newtio.c_iflag |= (INPCK | ISTRIP);
            newtio.c_cflag |= PARENB;
            newtio.c_cflag &= ~PARODD;
            break;
        case 'N':
            newtio.c_cflag &= ~PARENB;
            break;
    }


    switch (nSpeed) {
        case 2400:
            cfsetispeed (&newtio, B2400);
            cfsetospeed (&newtio, B2400);
            break;
        case 4800:
            cfsetispeed (&newtio, B4800);
            cfsetospeed (&newtio, B4800);
            break;
        case 9600:
            cfsetispeed (&newtio, B9600);
            cfsetospeed (&newtio, B9600);
            break;
        case 115200:
            cfsetispeed (&newtio, B115200);
            cfsetospeed (&newtio, B115200);
            break;
        case 460800:
            cfsetispeed (&newtio, B460800);
            cfsetospeed (&newtio, B460800);
            break;
        default:
            cfsetispeed (&newtio, B9600);
            cfsetospeed (&newtio, B9600);
            break;
    }

    if (nStop == 1) {
        newtio.c_cflag &= ~CSTOPB;
    } else if (nStop == 2) {
        newtio.c_cflag |= CSTOPB;
    }

    //newtio.c_cflag &= ~(ICANON |ISIG| ICRNL|IGNCR);
    newtio.c_lflag &= ~ (ICANON | ISIG);
    newtio.c_iflag &= ~ (ICRNL | IGNCR);
    newtio.c_cc[VTIME] = 128;
    newtio.c_cc[VMIN] = 100;

    tcflush (fd, TCIOFLUSH);

    if ((tcsetattr (fd, TCSANOW, &newtio)) != 0) {
        //TCSANOW
        perror ("com set error");
        return -1;
    }
    return 0;
}

unsigned short CRC16 (unsigned char *q, int len)
{
    unsigned short crc = 0;

    while (len-- > 0) {
        crc = (crc << 8) ^ ccitt_table[ ((crc >> 8) ^
                                                     *q++) & 0xff];
    }
    return crc;
}

int cmp_read_crc (unsigned char *string, int len)
{
    unsigned char crc_read[2] = {string[len - 2], string[len - 1]};
    unsigned short read_crc_cal = CRC16 ((
            unsigned char *)&string[0], len - 2);
    //	printf("crc1 = %x %x, crc2 = %x %x\n", crc_read[0], crc_read[1], (read_crc_cal&0xff00)>>8, (read_crc_cal&0xff));
    if (crc_read[0] == ((read_crc_cal & 0xff00) >> 8)
        && crc_read[1] == (read_crc_cal & 0xff)) {
        return 0;
    } else {
        return -1;
    }
}

static void help()
{
    fprintf (stdout, "%s\n", usage[0]);
}

/* uart_util /dev/ttyS1 0xd 0xaa 0xaa.
 * by tsihang, 2022-05-26. */
int main (int argc, char **argv)
{
    char uart_order[50] = {0};
    //const char *tty = "/dev/ttyS1";
    int fd;
    int i;
    int ReadByte = 0;
    unsigned char Buffer[2048];
    char *dev = (char *)argv[1];

    /* Test cmd. */
    if (argc == 2) {
        if (!strcmp (argv[1], "help")) {
            fprintf (stdout, "Good\n");
            return 0;
        }
        exit (0);
    }

    if ((fd = open (dev,
                    O_RDWR | O_NOCTTY | O_NONBLOCK)) <
        0) { //open serial
        fprintf (stdout, "Can't open serial port!\n");
        help();
        return -1;
    }
    set_opt (fd, 9600, 8, 'N', 1);
    //strncpy (uart_order, "uart_", 5);
    uart_order[0] = 'u';
    uart_order[1] = 'a';
    uart_order[2] = 'r';
    uart_order[3] = 't';
    uart_order[4] = '_';

    if (argc == 3) {
        sprintf (&uart_order[5], "%s", argv[2]);
    } else if (argc == 4) {
        sprintf (&uart_order[5], "%s_%s", argv[2],
                 argv[3]);
    } else if (argc == 5) {
        sprintf (&uart_order[5], "%s_%s_%s", argv[2],
                 argv[3], argv[4]);
    }
    unsigned short crc_value = CRC16 ((
                                          unsigned char *)&uart_order[0],
                                      strlen (uart_order));
    char crc[10] = "\0";
    sprintf (crc, "_%x%x#", (crc_value & 0xff00) >> 8,
             crc_value & 0xff);
    strcat (uart_order, crc);
    if (send_handle (fd, uart_order)) {
        fprintf (stdout, "tx failed\n");
        return -1;
    }
    if (argv[2][strlen (argv[2]) - 1] == '7' ||
        argv[2][strlen (argv[2]) - 1] == '9' ||
        argv[2][strlen (argv[2]) - 1] == 'a' ||
        argv[2][strlen (argv[2]) - 1] == 'c') {
        return 0;
    }
    usleep (700000);

    ReadByte = read (fd, Buffer, 256);
#if 0
    tcflush (fd,
             TCIOFLUSH);
    int read_crc_check = cmp_read_crc (Buffer,
                                       ReadByte);
    if (read_crc_check) {
        fprintf (stdout, "crc read failed\n");
        return -1;
    }
#endif
    if (ReadByte > 0) {
        if (argv[2][strlen (argv[2]) - 1] == '5') {
            fprintf (stdout, "%02x ", 12);
            for (i = 1; i < ReadByte - 2; i++) {
                if ( i >= 3 && (i % 2) == 1) {
                    fprintf (stdout, "%02x", Buffer[i]);
                } else {
                    fprintf (stdout, "%02x ", Buffer[i]);
                }
            }

        } else {
            for (i = 0; i < ReadByte - 2; i++) {
                if (argv[2][strlen (argv[2]) - 1] == '1' ||
                    argv[2][strlen (argv[2]) - 1] == 'e' ||
                    argv[2][strlen (argv[2]) - 1] == 'E') {
                    fprintf (stdout, "%c", Buffer[i]);
                } else {
                    fprintf (stdout, "%02x ", Buffer[i]);
                }
            }
        }
        fprintf (stdout, "\n");

    } else {
        fprintf (stdout, "read failed\n");
    }
    usleep (5000);
    close (fd);

    return 0;
}

