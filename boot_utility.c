/*
*	boot_utility.c
*/

#include "../include/configs/bootloader.h"

int Calc_CRC32(slot * _slot, crcbuf * pbuf, ulong *uerrno)
{
	int result = CMD_RET_SUCCESS;
	ulong image_wsize = _slot->size / 4;
	ulong slot_addr = _slot->addr;
	ulong crc = 0;
	//crcbuf buf = {0};	
							// read image data into buf
	for(int i = 0; i < image_wsize; i++){
		pbuf->u[i] = readl(slot_addr);
#ifdef DEBUGTHISFILE
		printf("%ld",pbuf->u[i]); printf("  ");
#endif
		slot_addr += 4;
	}
	
	crc = Get_CRC((unsigned char*)&(pbuf->c[0]), _slot->size);

	if( (_slot->character == 'A') && ( _slot->crc != crc) )
	{
		*uerrno =  IMAGEaCRC_NCERROR;
		result = CMD_RET_FAILURE;
	}
	if( (_slot->character == 'B') && ( _slot->crc != crc) )
	{
		*uerrno =  IMAGEbCRC_NCERROR;
		result = CMD_RET_FAILURE;
	}

#ifdef DEBUGTHISFILE
	printf("\n cal_crc32 = %lu\n",crc);
#endif
	
	return result;
}

/*
*	purpose:	reads header blob data. it is located in flash 0x70000000, offset- 0x8FF80.
			check if data exists and is sanity
*	params: 	blob's fields
*	return: 	errno number if there is error else 0 
*/
ulong ReadHeader(header *head, ulong *uerrno)
{
	ulong address = HEADER_ADD;

	head->sanity_check = readl(address);
	head->ser_num = readl(address+=4);
	head->prod_id = readl(address+=4);
	head->stack_addr = readl(address+=4);
	head->reset_handler_addr = readl(address+=4);
	head->dest_ram_addr = readl(address+=4);
	head->wait_time = readl(address+=4);
	head->magic_num = readl(address+=4);
	head->slotAoffset = readl(address+=4);
	head->crc_a = readl(address+=4);
	head->image_size_a = readl(address+=4);
	head->slotBoffset = readl(address+=4);
	head->crc_b = readl(address+=4);
	head->image_size_b = readl(address+=4); 

#ifdef DEBUGTHISFILE
	printf("\n crc_a= %lu SIZE_A= %ld slotA= %ld crc_b= %lu SIZE_B= %ld slotB= %ld STACK_ADDR= %ld RESET_HAND_ADDR = %ld DEST_ADDR= %ld TIME= %ld MAGIC= %ld ID= %ld SER= %ld SANITY= %ld\n", 
	head->crc_a, head->image_size_a, head->slotAoffset, head->crc_b, head->image_size_b, head->slotBoffset, head->stack_addr,
	head->reset_handler_addr, head->dest_ram_addr, head->wait_time, head->magic_num, head->prod_id, head->ser_num, head->sanity_check);
#endif 
	
	
	if(head->sanity_check != SANITY_CHECK){		// invalid header file
		*uerrno = SANITY_CRERROR;
		goto failure;	
	}
	
	
	if((head->image_size_a <= 0)||(head->slotAoffset == 0) || (head->image_size_b <= 0) || (head->prod_id == 0 ) || (head->slotBoffset == 0)||
	(head->stack_addr == 0)|| (head->reset_handler_addr == 0)||(head->wait_time == 0)||(head->magic_num == 0)){
		*uerrno = PARAM_CRERROR;
		goto failure;	//  incorrect boot params
	}

	if((head->image_size_a == 0xFFFFFFFF) || (head->image_size_b == 0xFFFFFFFF) || ( head->prod_id == 0xFFFFFFFF ) || 
	(head->slotAoffset == 0xFFFFFFFF)||(head->slotBoffset == 0xFFFFFFFF)||(head->stack_addr == 0xFFFFFFFF) ||
	(head->reset_handler_addr == 0xFFFFFFFF)||(head->wait_time == 0xFFFFFFFF)||(head->magic_num == 0xFFFFFFFF)){
		*uerrno = PARAM_CRERROR;
		goto failure;	//  incorrect boot params
	}


	if((head->prod_id == 0 ) || (head->prod_id == 0xFFFFFFFF)){
		*uerrno = PARAM_CRERROR;
		goto failure;	//  incorrect boot params
	}

///////////////////////////////////////////// return ok
	return 0;
///////////////////////////////////////////// return error	
	failure:
		return  (ulong)*uerrno;
}


int ErrorHandler(char *pattr, ulong uerrno)
{
	int res;
	char  low_uerrno = 0, *slot = NULL, bootable_attribute = *pattr;
	char str[10] = {'\0'};
	
#ifdef DEBUGTHISFILE
	printf("\nuerrno: 0x%08lX\n",uerrno);
#endif
	_assert(uerrno);								// check if there is wrong ErrorHandler call 
	
	low_uerrno = (char)(uerrno & 0x000000FF);
				
	switch(low_uerrno)
	{

		case IMAGE_RERUN:						/*try to up runing core not critical, may continue work*/
			printf("# Auxiliary core is already up.\n");
			res = CMD_RET_SUCCESS;
			break;
		case IMAGEaCRC_NCERROR:						// bad CRC error
			slot = "A\0";
		case IMAGEbCRC_NCERROR:
			if(strcmp(slot,"A\0") == 0){
				printf("# Invalid image CRC in A slot.\n");
				bootable_attribute &= 0xF1;  			// set A slot loadable/unsuccess/unactive
				if(bootable_attribute & 0x20)			// check if slot B is loadable & success 
					bootable_attribute |= 0x40;		//  make B slot active
				
				memset(str,'\0',10);
				snprintf(str, 10, "%d", bootable_attribute);
				env_set("bootaux#",  &str[0]);
				
				memset(str,'\0',10);
				snprintf(str, 10, "%02lx", uerrno);
				env_set("booterror#", &str[0]);
				
				//printf("Save anvironments vari.\n");
				env_save();
				do_reset(NULL,0,0,NULL);
			}
			else{
				//*slot="B\0";
				bootable_attribute &= 0x1F;			// set B slot loadable/unsuccess/unactive
				if(bootable_attribute & 0x02)			// chec if slot A is loadable & success
					bootable_attribute |= 0x04;		//  make A slot active

				memset(str,'\0',10);
				snprintf(str, 10, "%d", bootable_attribute);
				env_set("bootaux#",  &str[0]);
				
				memset(str,'\0',10);
				snprintf(str, 10, "%02lx", uerrno);
				env_set("booterror#", &str[0]);
				

				printf("# Invalid image CRC in B slot.\n");
				env_save();
				do_reset(NULL,0,0,NULL);
			}

		case CONSISTENT_CRERROR:
			printf("## Due there are not any consistent slots, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;		
		case BADATTR_CRERROR:							/* there is pre set critical error */
			printf("## Due boot attribut has invalid value, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		case PARAM_CRERROR:
			printf("## Due invalid header params, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		case SANITY_CRERROR:
			printf("## Invalid sanity check of header file, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		case TRANS_CRERROR:
			printf("## Due malfunction transfer image to RAM, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		case POST_CRERROR:
			printf("## Due internal error POST was failed, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		case TIMEOUT_CRERROR:
			printf("## Due timeout, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		case CORDWN_CRERROR:
			printf("## The attept start auxiliary_core failed, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		case PRODUCT_CRERROR:
			printf("## The firmware identificator mismatch, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		case ENV_CRERROR:
			printf("## Environment variable incorrect, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		case ECONNRFUSED_CRERROR:
			printf("## Connection refused, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		case FILESIZE_CRERROR:
			printf("## Wrong image size, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		case FLASHOPERATION_CRERROR:
			printf("## Flash operation failure, booting terminated.\n");
			res = CMD_RET_FAILURE;
			break;
		default:
			_assert(0);
			res = CMD_RET_FAILURE;
	}	
			memset(str,'\0',10);
			snprintf(str, 10, "%02lx", uerrno);
			env_set("booterror#", &str[0]);
			printf("Saveing boot error..\n");
			env_save();
	return res;
}

ulong Get_CRC(unsigned char* buffer, ulong bufsize)
{

    ulong  crc = 0xffffffff;
    int len;
    len = bufsize;

    for (int i = 0; i < len; i++){
        crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ buffer[i]];
    }

    return crc ^ 0xffffffff;
}


ulong crc32_table[] = {
    0,   	 1996959894,   3993919788,   2567524794,   124634137,   1886057615,   3915621685,   2657392035,
    249268274,    2044508324,   3772115230,   2547177864,   162941995,   2125561021,   3887607047,   2428444049,
    498536548,   1789927666,   4089016648,    2227061214,   450548861,    1843258603,   4107580753,   2211677639,
    325883990,   1684777152,   4251122042,    2321926636,   335633487,   1661365465,   4195302755,   2366115317,
    997073096,   1281953886,   3579855332,    2724688242,   1006888145,   1258607687,   3524101629,   2768942443,
    901097722,    1119000684,   3686517206,   2898065728,   853044451,   1172266101,   3705015759,   2882616665,
    651767980,   1373503546,   3369554304,    3218104598,   565507253,   1454621731,   3485111705,   3099436303,
    671266974,   1594198024,   3322730930,    2970347812,   795835527,    1483230225,   3244367275,   3060149565,
    1994146192,   31158534,    2563907772,    4023717930,   1907459465,   112637215,    2680153253,   3904427059,
    2013776290,   251722036,   2517215374,    3775830040,   2137656763,   141376813,   2439277719,    3865271297,
    1802195444,   476864866,   2238001368,    4066508878,   1812370925,   453092731,   2181625025,    4111451223,
    1706088902,   314042704,    2344532202,   4240017532,   1658658271,   366619977,   2362670323,    4224994405,
    1303535960,   984961486,   2747007092,    3569037538,   1256170817,   1037604311,   2765210733,   3554079995,
    1131014506,   879679996,   2909243462,    3663771856,   1141124467,   855842277,    2852801631,   3708648649,
    1342533948,   654459306,   3188396048,    3373015174,   1466479909,   544179635,     3110523913,  3462522015,
    1591671054,   702138776,   2966460450,    3352799412,   1504918807,   783551873,    3082640443,   3233442989,
    3988292384,   2596254646,    62317068,    1957810842,   3939845945,   2647816111,     81470997,    1943803523,
    3814918930,    2489596804,   225274430,   2053790376,   3826175755,   2466906013,   167816743,    2097651377,
    4027552580,   2265490386,    503444072,   1762050814,   4150417245,   2154129355,   426522225,    1852507879,
    4275313526,   2312317920,   282753626,    1742555852,   4189708143,   2394877945,   397917763,    1622183637,
    3604390888,   2714866558,   953729732,   1340076626,    3518719985,   2797360999,   1068828381,    1219638859,
    3624741850,   2936675148,   906185462,   1090812512,   3747672003,   2825379669,   829329135,      1181335161,
    3412177804,   3160834842,   628085408,   1382605366,   3423369109,   3138078467,   570562233,      1426400815,
    3317316542,   2998733608,   733239954,   1555261956,   3268935591,   3050360625,   752459403,     1541320221,
    2607071920,   3965973030,   1969922972,   40735498,    2617837225,   3943577151,   1913087877,      83908371,
    2512341634,   3803740692,   2075208622,   213261112,   2463272603,   3855990285,   2094854071,     198958881,
    2262029012,   4057260610,   1759359992,   534414190,   2176718541,    4139329115,   1873836001,    414664567,
    2282248934,   4279200368,   1711684554,   285281116,   2405801727,   4167216745,   1634467795,     376229701,
    2685067896,   3608007406,   1308918612,   956543938,   2808555105,   3495958263,   1231636301,     1047427035,
    2932959818,   3654703836,   1088359270,   936918000,   2847714899,   3736837829,   1202900863,     817233897,
    3183342108,   3401237130,   1404277552,   615818150,   3134207493,   3453421203,   1423857449,     601450431,
    3009837614,   3294710456,   1567103746,   711928724,    3020668471,  3272380065,   1510334235,     755167117
};




