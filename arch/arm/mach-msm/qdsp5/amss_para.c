/*
 * Author: Markinus
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/err.h>

#include <asm/mach-types.h>
#include <mach/amss_para.h>

// The struckt array with the AMSS default Values, it have to have the same order and size as the enum with AMSS IDs in the Heaader File!
struct amss_value amss_def_para[] = {
	{AUDMGR_PROG_VERS,0, "rs30000013:e94e8f0c"},  
	{AUDMGR_PROG, 0x30000013, ""},  
	{AUDMGR_VERS, 0xe94e8f0c, ""},  
	{AUDMGR_CB_PROG_VERS,0, "rs31000013:21570ba7"},  
	{AUDMGR_CB_PROG, 0x31000013, ""},  
	{AUDMGR_CB_VERS, 0x21570ba7, ""},  
	{TIME_REMOTE_MTOA_PROG, 0x3000005d, ""},  
	{TIME_REMOTE_MTOA_VERS, 0, ""}, 
	{RPC_TIME_REMOTE_MTOA_NULL, 0, ""}, 
	{RPC_TIME_TOD_SET_APPS_BASES, 1, ""},  
	{PM_LIBPROG, 0x30000061, ""},  
	{PM_LIBVERS, 0x10001, ""},  
	{RPC_SND_PROG, 0x30000002, ""},
	{RPC_SND_VERS, 0x0, ""},  
	{SND_SET_DEVICE_PROC, 1, ""},  
	{SND_SET_VOLUME_PROC, 2, ""},  
	{RPC_ADSP_RTOS_ATOM_PROG_VERS, 0, ""},
	{RPC_ADSP_RTOS_ATOM_PROG, 0x3000000a, ""},
	{RPC_ADSP_RTOS_ATOM_VERS, 0x0, ""},  
	{RPC_ADSP_RTOS_MTOA_PROG, 0x3000000b, ""},
	{RPC_ADSP_RTOS_MTOA_VERS, 0x0, ""},  
	{RPC_ADSP_RTOS_ATOM_NULL_PROC, 0, ""},
	{RPC_ADSP_RTOS_MTOA_NULL_PROC, 0, ""},
	{RPC_ADSP_RTOS_APP_TO_MODEM_PROC, 1, ""},
	{RPC_ADSP_RTOS_MODEM_TO_APP_PROC, 1, ""},
	{RPC_DOG_KEEPALIVE_NULL, 0, ""},  
	{RPC_DOG_KEEPALIVE_BEACON, 1, ""},  
	{DOG_KEEPALIVE_PROG, 0x30000015, ""},
	{DOG_KEEPALIVE_VERS, 0, ""},  
	{RPC_PDAPI_PROG, 0x3000005b, ""},
	{RPC_PDAPI_CB_PROG, 0x3100005b, ""},
};


// Now the version spezificly values
struct amss_value amss_6120_para[] = {
	{AUDMGR_PROG_VERS,0, "rs30000013:00000000"},  
	{AUDMGR_VERS, 0x0, ""},  
	{AUDMGR_CB_PROG_VERS,0, "rs31000013:00000000"},  
	{AUDMGR_CB_VERS, 0x00000000, ""},  
	{RPC_SND_VERS, 0xaa2b1a44, ""},  
	{SND_SET_DEVICE_PROC, 2, ""},  
	{SND_SET_VOLUME_PROC, 3, ""},  
	{RPC_ADSP_RTOS_ATOM_PROG_VERS, 0, "rs3000000a:00000000"},
	{RPC_ADSP_RTOS_ATOM_VERS, 0x0, ""},  
	{RPC_ADSP_RTOS_MTOA_VERS, 0x0, ""},  
};

struct amss_value amss_6210_para[] = {
	{AUDMGR_PROG_VERS,0, "rs30000013:46255756"},  
	{AUDMGR_VERS, 0x46255756, ""},  
	{AUDMGR_CB_PROG_VERS,0, "rs31000013:5fa922a9"},  
	{AUDMGR_CB_VERS, 0x5fa922a9, ""},  
	{TIME_REMOTE_MTOA_VERS, 0, ""},  
	{RPC_TIME_TOD_SET_APPS_BASES, 2, ""},  
	{PM_LIBVERS, 0x0, ""},  
	{RPC_SND_VERS, 0x94756085, ""},  
	{SND_SET_DEVICE_PROC, 2, ""},  
	{SND_SET_VOLUME_PROC, 3, ""},  
	{RPC_ADSP_RTOS_ATOM_PROG_VERS, 0, "rs3000000a:20f17fd3"},
	{RPC_ADSP_RTOS_ATOM_VERS, 0x20f17fd3, ""},  
	{RPC_ADSP_RTOS_MTOA_VERS, 0x75babbd6, ""},  
	{RPC_DOG_KEEPALIVE_BEACON, 1, ""},  
	{DOG_KEEPALIVE_VERS, 0, ""},  
};

struct amss_value amss_6220_para[] = {
	{TIME_REMOTE_MTOA_VERS, 0x731fa727, ""},  
	{RPC_TIME_TOD_SET_APPS_BASES, 2, ""},  
	{PM_LIBVERS, 0xfb837d0b, ""},  
	{RPC_ADSP_RTOS_ATOM_PROG_VERS, 0, "rs3000000a:71d1094b"},
	{RPC_ADSP_RTOS_ATOM_VERS, 0x71d1094b, ""},  
	{RPC_ADSP_RTOS_MTOA_VERS, 0xee3a9966, ""},  
	{RPC_DOG_KEEPALIVE_BEACON, 2, ""},  
	{DOG_KEEPALIVE_VERS, 0x731fa727, ""},  
};


struct amss_value amss_5200_para[] = {
	{AUDMGR_PROG_VERS,0, "rs30000013:00000000"},  
	{AUDMGR_VERS, 0x0, ""},  
	{AUDMGR_CB_PROG_VERS,0, "rs31000013:5fa922a9"},  
	{AUDMGR_CB_VERS, 0x5fa922a9, ""},  
	{RPC_SND_VERS, 0x0, ""},  
	{SND_SET_DEVICE_PROC, 1, ""},  
	{SND_SET_VOLUME_PROC, 2, ""},  
	{RPC_ADSP_RTOS_ATOM_PROG_VERS, 0, "rs3000000a:00000000"},
	{RPC_ADSP_RTOS_ATOM_VERS, 0x0, ""},  
	{RPC_ADSP_RTOS_MTOA_VERS, 0x0, ""},  
};

struct amss_value amss_6150_para[] = {
	{AUDMGR_PROG_VERS,0, "rs30000013:00000000"},  
	{AUDMGR_VERS, 0x0, ""},  
	{RPC_ADSP_RTOS_ATOM_PROG_VERS, 0, "rs3000000a:00000000"},
};

// Function to init of the struct and get the Values, Init in first call.

int amss_get_value(int id, uint32_t *numval, char* strval, size_t size)
{
	static struct amss_value *active_para = NULL;
	struct amss_value *mach_para;
	static uint8_t init = 0, i;
	uint32_t nbr_para = 0;
	if(!init) { // First run, init the struct
	  pr_err("INIT AMSS PARA!, size: %d\n", sizeof(amss_def_para));
		// Initializes the default patameters
		active_para = kmalloc(sizeof(amss_def_para), GFP_KERNEL);
		memcpy(active_para, &amss_def_para, sizeof(amss_def_para));
		// Get the right struct
		switch(__machine_arch_type) {
			case MACH_TYPE_HTCTOPAZ:
			case MACH_TYPE_HTCRHODIUM:
				mach_para = amss_6120_para;
				nbr_para = ARRAY_SIZE(amss_6120_para);
				break;
			case MACH_TYPE_HTCRAPHAEL:
			case MACH_TYPE_HTCDIAMOND:
			case MACH_TYPE_HTCBLACKSTONE:
				mach_para = amss_5200_para;
				nbr_para = ARRAY_SIZE(amss_5200_para);
				break;
			case MACH_TYPE_HTCRAPHAEL_CDMA:
			case MACH_TYPE_HTCRAPHAEL_CDMA500:
			case MACH_TYPE_HTCDIAMOND_CDMA:
				mach_para = amss_6150_para;
				nbr_para = ARRAY_SIZE(amss_6150_para);
				break;
			default:
				printk(KERN_ERR "Unsupported device for adsp driver\n");
				strval = "";
				numval = 0;
				return -ENODEV;
				break;
		}
		// Set the versionspezificly values		
		for(i=0; i<nbr_para; i++) {
			active_para[mach_para[i].id].numval = mach_para[i].numval;
			active_para[mach_para[i].id].strval = kmalloc(sizeof(mach_para[i].strval)+1, GFP_KERNEL);
			strcpy(active_para[mach_para[i].id].strval, mach_para[i].strval);
		}

		init = 1;
	}
	// Struct ready, get Value and exit
	if(active_para[id].id != id) {	// Check the array integrity
		pr_err("AMSS Array mismatch! Check the parameters!\n");
		strval = "";
		numval = 0;
		return -EINVAL;
	}
	
	*numval = active_para[id].numval;
	memcpy (strval, active_para[id].strval, size);
	pr_info              ("AMSS ASK: %d   GET:  %x    :%s\n", id, *numval, strval);

	return 0;
	
}


uint32_t amss_get_num_value(int id)
{
	char* str = "";
	uint32_t num = 0;
	amss_get_value(id, &num, str, 0);
	return num;
}
  
void amss_get_str_value(int id, char* str, size_t size)
{
	uint32_t num = 0;
	amss_get_value(id, &num, str, size);
}
 
