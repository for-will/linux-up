

/* Hd controller regs. Ref: IBM AT Bios-listing */
#define HD_DATA         0x1f0   /* _CTL when writing */
#define HD_ERROR        0x1f1   /* see err-bits */
#define HD_STATUS       0x1f7   /* see status-bits */

#define HD_CMD          0x3f6

/* Bits of HD_STATUS */
#define ERR_STAT        0x01
#define INDEX_STAT      0x02
#define ECC_STAT        0x04    /* Corrected error */
#define DRQ_STAT        0x08
#define SEEK_STAT       0x10
#define WRERR_STAT      0x20
#define READY_STAT      0x40
#define BUSY_STAT       0x80

/* Values for HD_COMMAND */
#define WIN_RESTORE             0x10
#define WIN_READ                0x20
#define WIN_WRITE               0x30
#define WIN_SPECIFY             0x91

struct partition {
        unsigned char boot_ind;         /* 0x80 - active (unused) */
        unsigned char head;
        unsigned char sector;
        unsigned char cyl;
        unsigned char sys_ind;
        unsigned char end_head;
        unsigned char end_sector;
        unsigned char end_cyl;
        unsigned int start_sect;        /* starting sector counting from 0 */
        unsigned int nr_sects;          /* nr of sectors in partition */
};
