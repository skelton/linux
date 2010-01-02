/*
* board-htcraphael-gps.c:  support for RAPH100 (AMSS 5200) MSM7201A GPS
*
* Copyright 2009 by Tyler Hall <tylerwhall@gmail.com>
* Modified version of gps.c by Steven Walter <stevenrwalter@gmail.com>
*
* Licensed under GNU GPLv2
*/

#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/syscalls.h>
#include <asm/div64.h>
#include <mach/msm_rpcrouter.h>
#include <linux/delay.h>
#include <linux/rfkill.h>
#include <asm/mach-types.h>

#include "vogue_gps.h"
#include "nmea.h"

#define RPC_PDAPI_PROG          0x3000005b
#define RPC_PDAPI_CB_PROG       0x3100005b

struct rpc_ids {
	uint32_t client_init;
	uint32_t client_pd_reg;
	uint32_t client_pa_reg;
	uint32_t client_lcs_reg;
	uint32_t client_xtra_reg;
	uint32_t client_ext_status_reg;
	uint32_t client_act;
	uint32_t client_deact;
	uint32_t get_position;
	uint32_t end_session;
} ids;

struct rpc_ids ids_6120 = {
	.client_init		= 2,
	.client_pd_reg		= 4,
	.client_pa_reg		= 5,
	.client_lcs_reg		= 6,
	.client_xtra_reg	= 7,
	.client_ext_status_reg	= 7,
	.client_act		= 9,
	.client_deact		= 0xa,
	.get_position		= 0xb,
	.end_session		= 0xc,
};
struct rpc_ids ids_5200 = {
	.client_init		= 3,
	.client_pd_reg		= 5,
	.client_pa_reg		= 6,
	.client_lcs_reg		= 7,
	.client_xtra_reg	= 8,
	.client_ext_status_reg	= 9,
	.client_act		= 0xa,
	.client_deact		= 0xb,
	.get_position		= 0xc,
	.end_session		= 0xd,
};

#define RPC_PDSM_ATL_PROG          0x3000001d
#define RPC_PDSM_ATL_CB_PROG       0x3100001d
//#define L2_PROXY_REG            4

/* TODO: replace with msm_rpc_call_reply() values */
#define PD_CLIENT_ID            0xda3
#define LCS_CLIENT_ID           0x1b59          	/* 7001 */
#define XTRA_CLIENT_ID          0x1f40          	/* 8000 */

/* constants */
#define INIT_PD                 0x2
#define INIT_LCS                0x4
#define INIT_XTRA               0xb

#define NUMFIX                  0x3b9ac9ff      	/* 999999999 */
#define FIX_ACCURACY            0x32            	/* 50m, 0x1e = 30m */
#define SESSION_TIMEOUT         2

static struct msm_rpc_endpoint *ept;
static struct rfkill *gps_rfk;

static void gps_disable() {
	struct {
		struct rpc_request_hdr hdr;
		uint32_t msg[4];
	} msg;
	int rc;

	msg.msg[0] = cpu_to_be32 (0xa);
	msg.msg[1] = cpu_to_be32 (0);
	msg.msg[2] = cpu_to_be32 (0);
	msg.msg[3] = cpu_to_be32 (PD_CLIENT_ID);
	rc = msm_rpc_call (ept, ids.end_session, &msg,
			4*4 + sizeof (struct rpc_request_hdr), 15 * HZ);
	if (rc < 0)
		printk ("ERROR: %s:%d %d\n", __func__, __LINE__, rc);
}


static int handle_pdapi_rpc_call(struct msm_rpc_server *server,
			   struct rpc_request_hdr *req, unsigned len) {
	printk("%s req->procedure=0x%x\n",__FUNCTION__,req->procedure);
	return 0;
}

static int handle_pdsm_atl_rpc_call(struct msm_rpc_server *server,
			   struct rpc_request_hdr *req, unsigned len) {
	 printk("%s req->procedure=0x%x\n",__FUNCTION__,req->procedure);
	return 0;
}

static struct msm_rpc_server pdapi_rpc_server = {
	.prog = RPC_PDAPI_CB_PROG,
	.vers = 0,
	.rpc_call = handle_pdapi_rpc_call,
};

static struct msm_rpc_server pdsm_atl_rpc_server = {
	.prog = RPC_PDSM_ATL_CB_PROG,
	.vers = 0,
	.rpc_call = handle_pdsm_atl_rpc_call,
};


static void gps_enable (void) {
	struct {
		struct rpc_request_hdr hdr;
		uint32_t msg[28];
	} msg;
	int rc;

	struct {
		struct rpc_reply_hdr hdr;
		uint32_t retval;
	} rep;

	/* register CB */
	msm_rpc_create_server(&pdapi_rpc_server);
	msm_rpc_create_server(&pdsm_atl_rpc_server);

	/* init pd */
	printk ("init_pd\n");
	msg.msg[0] = cpu_to_be32 (INIT_PD);
	rc = msm_rpc_call_reply (ept, ids.client_init, &msg, 
			1*4 + sizeof (struct rpc_request_hdr),
			&rep, sizeof(rep), 5 * HZ);
	if (rc < 0)
		printk ("ERROR: %s:%d %d\n", __func__, __LINE__, rc);

	printk ("%s:%d init_pd ret=0x%x\n", __func__, __LINE__, be32_to_cpu(rep.retval));

	/* pd_reg */
	printk ("pd_reg\n");
	msg.msg[0] = cpu_to_be32 (PD_CLIENT_ID);
	msg.msg[1] = cpu_to_be32 (0x0);
	msg.msg[2] = cpu_to_be32 (0x0);
	msg.msg[3] = cpu_to_be32 (0x0);
#if (CONFIG_MSM_AMSS_VERSION == 6120) || (CONFIG_MSM_AMSS_VERSION == 6125)
	msg.msg[4] = cpu_to_be32 (0xf3f0ffff);
#elif (CONFIG_MSM_AMSS_VERSION == 5200) || (CONFIG_MSM_AMSS_VERSION == 6150)
	msg.msg[4] = cpu_to_be32 (0xf310ffff);
#else
#error "Unknown AMSS version"
#endif
	msg.msg[5] = cpu_to_be32 (0xffffffff);
	rc = msm_rpc_call_reply (ept, ids.client_pd_reg, &msg,
			6*4 + sizeof (struct rpc_request_hdr),
			&rep, sizeof(rep), 5 * HZ);
	if (rc < 0)
		printk ("ERROR: %s:%d %d\n", __func__, __LINE__, rc);

	printk ("%s:%d pd_reg ret=0x%x\n", __func__, __LINE__, be32_to_cpu(rep.retval));

	/* ext_status_reg */
	printk ("ext_status_reg\n");
	msg.msg[0] = cpu_to_be32 (PD_CLIENT_ID);
	msg.msg[1] = cpu_to_be32 (0x0);
	msg.msg[2] = cpu_to_be32 (0x1);
	msg.msg[3] = cpu_to_be32 (0x0);
	msg.msg[4] = cpu_to_be32 (0x4);
	msg.msg[5] = cpu_to_be32 (0xffffffff);
	rc = msm_rpc_call_reply (ept, ids.client_ext_status_reg,
			&msg, 6*4 + sizeof (struct rpc_request_hdr),
			&rep, sizeof(rep), 5 * HZ);
	if (rc < 0)
		printk ("ERROR: %s:%d %d\n", __func__, __LINE__, rc);

	printk ("%s:%d ext_status_reg ret=0x%x\n", __func__, __LINE__, be32_to_cpu(rep.retval));

	/* pd_act */
	printk ("pd_act\n");
	msg.msg[0] = cpu_to_be32 (PD_CLIENT_ID);
	rc = msm_rpc_call_reply (ept, ids.client_act, &msg,
			1*4 + sizeof (struct rpc_request_hdr), 
			&rep, sizeof(rep), 5 * HZ);
	if (rc < 0)
		printk ("ERROR: %s:%d %d\n", __func__, __LINE__, rc);

	printk ("%s:%d pd_act ret=0x%x\n", __func__, __LINE__, be32_to_cpu(rep.retval));

#if 0 /* AGPS */

	/* pa_reg */
	msg.msg[0] = cpu_to_be32 (PD_CLIENT_ID);
	msg.msg[1] = cpu_to_be32 (0x0);
	msg.msg[2] = cpu_to_be32 (0x2);
	msg.msg[3] = cpu_to_be32 (0x0);
#if (CONFIG_MSM_AMSS_VERSION == 6120) || (CONFIG_MSM_AMSS_VERSION == 6125)
	msg.msg[4] = cpu_to_be32 (0x07ffefe0);
#elif (CONFIG_MSM_AMSS_VERSION == 5200) || (CONFIG_MSM_AMSS_VERSION == 6150)
	msg.msg[4] = cpu_to_be32 (0x003fefe0);
#else
#error "Unknown AMSS version"
#endif
	msg.msg[5] = cpu_to_be32 (0xffffffff);
	rc = msm_rpc_call (ept, CLIENT_PD_REG, &msg,
			6*4 + sizeof (struct rpc_request_hdr),
			15 * HZ);
	if (rc < 0)
		printk ("%s:%d %d\n", __func__, __LINE__, rc);

	/* init xtra */
	msg.msg[0] = cpu_to_be32 (INIT_XTRA);
	rc = msm_rpc_call (ept, CLIENT_INIT, &msg,
			1*4 + sizeof (struct rpc_request_hdr),
			15 * HZ);
	if (rc < 0)
		printk ("%s:%d %d\n", __func__, __LINE__, rc);

	/* xtra_reg */
	msg.msg[0] = cpu_to_be32 (XTRA_CLIENT_ID);
	msg.msg[1] = cpu_to_be32 (0x0);
	msg.msg[2] = cpu_to_be32 (0x3);
	msg.msg[3] = cpu_to_be32 (0x0);
	msg.msg[4] = cpu_to_be32 (0x7);
	msg.msg[5] = cpu_to_be32 (0xffffffff);
	rc = msm_rpc_call (ept, CLIENT_XTRA_REG, &msg,
			6*4 + sizeof (struct rpc_request_hdr),
			15 * HZ);
	if (rc < 0)
		printk ("%s:%d %d\n", __func__, __LINE__, rc);

	/* xtra_act */
	msg.msg[0] = cpu_to_be32 (XTRA_CLIENT_ID);
	rc = msm_rpc_call (ept, CLIENT_ACT, &msg,
			1*4 + sizeof (struct rpc_request_hdr),
			15 * HZ);
	if (rc)
		printk ("%s:%d %d\n", __func__, __LINE__, rc);

	/* init lcs */
	msg.msg[0] = cpu_to_be32 (INIT_LCS);
	rc = msm_rpc_call (ept, CLIENT_INIT,
			&msg, 1*4 + sizeof (struct rpc_request_hdr),
			15 * HZ);
	if (rc < 0)
		printk ("%s:%d %d\n", __func__, __LINE__, rc);

	/* lcs_reg */
	msg.msg[0] = cpu_to_be32 (LCS_CLIENT_ID);
	msg.msg[1] = cpu_to_be32 (0x0);
	msg.msg[2] = cpu_to_be32 (0x7);
	msg.msg[3] = cpu_to_be32 (0x0);
	msg.msg[4] = cpu_to_be32 (0x3f0);
	msg.msg[5] = cpu_to_be32 (0x8);
	rc = msm_rpc_call (ept, CLIENT_LCS_REG, &msg,
			6*4 + sizeof (struct rpc_request_hdr),
			15 * HZ);
	if (rc < 0)
		printk ("%s:%d %d\n", __func__, __LINE__, rc);

	/* lcs_act */
	msg.msg[0] = cpu_to_be32 (LCS_CLIENT_ID);
	rc = msm_rpc_call (ept, CLIENT_ACT, &msg,
			1*4 + sizeof (struct rpc_request_hdr),
			15 * HZ);
	if (rc)
		printk ("%s:%d %d\n", __func__, __LINE__, rc);

#endif
}

void rfkill_switch_all(enum rfkill_type type, enum rfkill_state state);
static int gps_set_power(void *data, enum rfkill_state state)
{
	switch (state) {
	case RFKILL_STATE_ON:
		if(!ept) {
			ept = msm_rpc_connect (RPC_PDAPI_PROG, 0, MSM_RPC_UNINTERRUPTIBLE);
			if (!ept || IS_ERR (ept)) {
				printk ("msm_rpc_connect failed (%d)\n", (int) ept);
				return -1;
			}
		}
		gps_enable();
		break;
	case RFKILL_STATE_OFF:
		gps_disable();
		break;
	case RFKILL_STATE_HARD_BLOCKED://???
		break;
	}
	return 0;
}

static int gps_probe (struct platform_device *pdev) {
	int rc = 0;
	printk("GPS rfkill probe\n");
	switch(__machine_arch_type) {
		case MACH_TYPE_HTCTOPAZ:
		case MACH_TYPE_HTCRHODIUM:
			memcpy(&ids, &ids_6120, sizeof(ids));
			break;
		case MACH_TYPE_HTCRAPHAEL:
		case MACH_TYPE_HTCDIAMOND:
		case MACH_TYPE_HTCBLACKSTONE:
			memcpy(&ids, &ids_5200, sizeof(ids));
			break;
		default:
			printk(KERN_ERR "Unsupported device for gps driver\n");
			return -1;
			break;
	}

	/* default to bluetooth off */
	rfkill_switch_all(RFKILL_TYPE_GPS, RFKILL_STATE_OFF);
	//Kills ARM9 :-)
	//gps_set_power(NULL, RFKILL_STATE_OFF);

	gps_rfk = rfkill_allocate(&pdev->dev, RFKILL_TYPE_GPS);
	if (!gps_rfk)
		return -ENOMEM;

	gps_rfk->name = "MSM-GPS";
	gps_rfk->state = RFKILL_STATE_OFF;
	/* userspace cannot take exclusive control */
	gps_rfk->user_claim_unsupported = 1;
	gps_rfk->user_claim = 0;
	gps_rfk->data = NULL;  // user data
	gps_rfk->toggle_radio = gps_set_power;

	rc = rfkill_register(gps_rfk);

	if (rc)
		rfkill_free(gps_rfk);
	return rc;
}


static struct platform_driver gps_driver = {
	.probe = gps_probe,
	.driver = {
		.name = "gps_rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init gps_rfkill_init (void)
{
    printk("GPS rfkill init\n");
    return platform_driver_register (&gps_driver);
}

late_initcall (gps_rfkill_init);
