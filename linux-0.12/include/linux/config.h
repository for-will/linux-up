#ifndef _CONFIG_H
#define _CONFIG_H

/* 
 * Defines for what uname() should return
 */
#define UTS_SYSNAME "Linux"
#define UTS_NODENAME "(none)"   /* set by sethostname() */
#define UTS_RELEASE "0"         /* patchlevel */
#define UTS_VERSION "0.12"
#define UTS_MACHINE "i386"      /* hardware type */

/* Don't touch these, unless you really know what your doing. */
// 下面这些符号常数用于指明系统引导和加载内核时的具体内存位置，以及默念的最大内核内核大小值。
#define DEF_INITSEG	0x9000			/* 引导扇区程序将被移动到的段值 */
#define DEF_SYSSEG	0x1000			/* 引导扇区程序把系统模块加载到内存的段值 */
#define DEF_SETUPSEG	0x9020			/* setup程序所处内存段位置 */
#define DEF_SYSSIZE	0x3000			/* 内核系统模块默认最大节数（16字节=1节） */

/*
 * The root-device is no longer hard-coded. You can change the default
 * root-device by changing the line ROOT_DEV = XXX in boot/bootsect.s
 */

/*
 * The keyboard is now defined in kernel/chr_drv/keyboard.S
 */

/*
 * Normally, Linux can get the drive parameters from the BIOS at
 * startup, but if this for some unfathomable reason  fails, you'd
 * be left stranded. For this case,you can define HD_TYPE, which
 * contains all necessary info on your harddisk.
 *
 * The HD_TYPE macro should look like this:
 *
 * #define HD_TYPE { head, sect, cyl, wpcom, lzone, ctl}
 *
 * In case of two harddisks, the info should be separated by
 * commas:
 *
 * #define HD_TYPE { h,s,c,wpcom,lz,ctl },{ h,s,c,wpcom,lz,ctl }
 */
/*
 * 通常，Linux能够在启动时从BIOS中获取驱动器的参数，但是若由于未知原因
 * 而没有得到这些参数时，会使程序束手无策。对于这种情况，你可以定义HD_TYPE，
 * 其中包括硬盘的所有信息。
 *
 * HD_TYPE宏应该像下面这样的形式：
 *
 * #define HD_TYPE (head, sect, cyl, wpcom, lzone, ctl)
 *
 * 对于有两个硬盘的情况，参数信息需用逗号分开：
 *
 * #define HD_TYPE { h,s,c,wpcom,lz,ctl },{h,s,c,wpcom,lz,ctl }
 */
/*
  This is an example, two drives, first is type 2, second is type 3:

  #define HD_TYPE { 4,17,615,300,615,8 }, { 6,17,615,300,615,0 }

  NOTE: ctl is 0 for all drives with heads<=8, and ctl=8 for drives
  with more than 8 heads.

  If you want the BIOS to tell what kind of drive you have, just
  leave HD_TYPE undefined. This is the normal thing to do.
 */
/*
 * 下面是一个例子，两个硬盘，第1个是类型2，第2个是类型3：
 *
 * #define HD_TYPE { 4,17,615,300,615,8 }, {6,17,615,300,615,0 }
 *
 * 注意：对应所有硬盘，若其碰头数<=8，则ctl等于0，若碰头数多于8个，
 * 则ctl=8。
 *
 * 如果你想让BIOS给出硬盘的类型，那么只需不定义HD_TYPE。这是默认操作。
 */

#endif

