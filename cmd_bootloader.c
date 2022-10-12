/*
 * M4 boot command.
 * Read blob configuration file from flash, read and pars load parameters from blob, store error number if fail, set error flag.
 * Load M4 boot image to memory with the available options.
 * params: no
 * note: the magic value located at special place  (0x800000 A9 core map  or 0x20000000 M4 core map),
 *       m4 executes test proc then put value at the place.
*/

#include "../include/configs/bootloader.h"
#include <div64.h>
#include <dm.h>
#include <malloc.h>
#include <mapmem.h>
#include <spi.h>
#include <spi_flash.h>
#include <jffs2/jffs2.h>
#include <linux/mtd/mtd.h>

#include <asm/io.h>
#include <dm/device-internal.h>


ulong uerrno = 0;
char str[10] = {'\0'};
char *pbootable_attribute = NULL, bootable_attribute = 0;

extern int do_m4boot(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]) ;


int do_SystemBootloader(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]) 
{
	//char *pbootable_attribute, bootable_attribute;
	int result,up;
	char *const argA[] = {"\0","A\0"};
	char *const argB[] = {"\0","B\0"};

	printf("Bootloader started.\n");

	up = arch_auxiliary_core_check_up(0);
	
		if (up) {
#ifdef DEBUG
			printf("Auxiliary core is already up.\n");
#endif
			uerrno = IMAGE_RERUN;
			goto err;
		}
#ifdef DEBUG
		printf("Starting auxiliary core at 0x%08lX.\n", stack);
#endif
	
	pbootable_attribute = env_get("bootattr#");
	bootable_attribute =  (char)simple_strtoul(pbootable_attribute, NULL, 10);
	
#ifdef DEBUG
		printf("\n#bootable_attribute =  %d\n", bootable_attribute);
#endif
		
// check if there is correct boot attribute
	if((bootable_attribute > 0x77) || (bootable_attribute < 0)){
		uerrno = BADATTR_CRERROR;
		goto err;
	}
	
// check if there is bootable slotes

	if((bootable_attribute & 0x01) && (bootable_attribute & 0x02) && (bootable_attribute & 0x04)) {
		result = do_m4boot(NULL, 0, 0, (char *const*)argA);
	}
	else if ((bootable_attribute & 0x10) && (bootable_attribute & 0x20) && (bootable_attribute & 0x40)){
		result = do_m4boot(NULL, 0, 0, (char *const*)argB);
	}
	else{
		uerrno = CONSISTENT_CRERROR;
		goto err;
	}

	if(result == CMD_RET_SUCCESS){

		return CMD_RET_SUCCESS;
	}
	else	
		err:
			result = ErrorHandler(uerrno);
	return result;
}


U_BOOT_CMD(
	sbloader, 1, 0, do_SystemBootloader,
	"no params, check configuration, execute m4boot, set bootable attribute and error flag.\n",
	""
);


