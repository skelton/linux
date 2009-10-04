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
//#include <linux/android_power.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/syscalls.h>
#include <asm/div64.h>
#include <mach/msm_rpcrouter.h>
#include <linux/delay.h>

#include "vogue_gps.h"
#include "nmea.h"

#define RPC_PDAPI_PROG          0x3000005b
#define RPC_PDAPI_CB_PROG       0x3100005b

#define CLIENT_INIT             3
#define CLIENT_PD_REG           5
#define CLIENT_PA_REG           6
#define CLIENT_LCS_REG          7
#define CLIENT_XTRA_REG         8
#define CLIENT_EXT_STATUS_REG	9
#define CLIENT_ACT              0xa
#define GET_POSITION            0xc
#define END_SESSION             0xd

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

/* This function causes the ARM9/mDSP to calculate a new fix */
static void
pdsm_get_position (void)
{
    struct
  {
        struct rpc_request_hdr hdr;
         uint32_t msg[28];
     } msg;
    int rc;

    msg.msg[0] = cpu_to_be32 (0xa); /* unknown */
    msg.msg[1] = cpu_to_be32 (0x0);
    msg.msg[2] = cpu_to_be32 (0x1);
    msg.msg[3] = cpu_to_be32 (0x1);
    msg.msg[4] = cpu_to_be32 (0x1);
    msg.msg[5] = cpu_to_be32 (NUMFIX);
    msg.msg[6] = cpu_to_be32 (0x1);
    msg.msg[7] = cpu_to_be32 (0x0);
    msg.msg[8] = cpu_to_be32 (0x0);
    msg.msg[9] = cpu_to_be32 (0x0);
    msg.msg[10] = cpu_to_be32 (0x0);
    msg.msg[11] = cpu_to_be32 (0x0);
    msg.msg[12] = cpu_to_be32 (0x0);
    msg.msg[13] = cpu_to_be32 (0x0);
    msg.msg[14] = cpu_to_be32 (0x0);
    msg.msg[15] = cpu_to_be32 (0x0);
    msg.msg[16] = cpu_to_be32 (0x0);
    msg.msg[17] = cpu_to_be32 (0x0);
    msg.msg[18] = cpu_to_be32 (0x0);
    msg.msg[19] = cpu_to_be32 (0x0);
    msg.msg[20] = cpu_to_be32 (0x0);
    msg.msg[21] = cpu_to_be32 (0x0);
    msg.msg[22] = cpu_to_be32 (0x0);
    msg.msg[23] = cpu_to_be32 (0x0);
    msg.msg[24] = cpu_to_be32 (0x1);
    msg.msg[25] = cpu_to_be32 (FIX_ACCURACY);
    msg.msg[26] = cpu_to_be32 (SESSION_TIMEOUT);
    msg.msg[27] = cpu_to_be32 (PD_CLIENT_ID);
    rc =
    msm_rpc_call (ept, GET_POSITION, &msg,
		  (28 * 4) + sizeof (struct rpc_request_hdr), 15 * HZ);
    if (rc < 0)
        printk ("ERROR: %s:%d %d\n", __func__, __LINE__, rc);

}

//static android_suspend_lock_t gps_suspend_lock;

static void
gps_disable (void)
{
    struct
  {
        struct rpc_request_hdr hdr;
         uint32_t msg[4];
     } msg;
    int rc;

    msg.msg[0] = cpu_to_be32 (0xa);
    msg.msg[1] = cpu_to_be32 (0);
    msg.msg[2] = cpu_to_be32 (0);
    msg.msg[3] = cpu_to_be32 (PD_CLIENT_ID);
    rc =
    msm_rpc_call (ept, END_SESSION, &msg, 4*4 + sizeof (struct rpc_request_hdr), 15 * HZ);
    if (rc < 0)
        printk ("ERROR: %s:%d %d\n", __func__, __LINE__, rc);

//    android_unlock_suspend(&gps_suspend_lock);
}


static int handle_pdapi_rpc_call(struct msm_rpc_server *server,
			   struct rpc_request_hdr *req, unsigned len)
{
//	switch (req->procedure) {
	 printk("%s req->procedure=0x%x\n",__FUNCTION__,req->procedure);
//	}

 return 0;
}

static int handle_pdsm_atl_rpc_call(struct msm_rpc_server *server,
			   struct rpc_request_hdr *req, unsigned len)
{
//	switch (req->procedure) {
	 printk("%s req->procedure=0x%x\n",__FUNCTION__,req->procedure);
//	}

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


static void
gps_enable (void)
{
    struct
  {
        struct rpc_request_hdr hdr;
         uint32_t msg[28];
     } msg;
    int rc;

    struct
  {
        struct rpc_reply_hdr hdr;
         uint32_t retval;
     } rep;

    				/* The AMSS seems to get upset if we suspend while PDSM is active */
//    android_lock_suspend(&gps_suspend_lock);

   /* register CB */
    msm_rpc_create_server(&pdapi_rpc_server);
    msm_rpc_create_server(&pdsm_atl_rpc_server);

    /* init pd */
    printk ("init_pd\n");
    msg.msg[0] = cpu_to_be32 (INIT_PD);
    rc =
    msm_rpc_call_reply (ept, CLIENT_INIT, &msg, 1*4 + sizeof (struct rpc_request_hdr),
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
    msg.msg[4] = cpu_to_be32 (0xf310ffff);
    msg.msg[5] = cpu_to_be32 (0xffffffff);
    rc =
    msm_rpc_call_reply (ept, CLIENT_PD_REG, &msg, 6*4 + sizeof (struct rpc_request_hdr),
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
    rc =
    msm_rpc_call_reply (ept, CLIENT_EXT_STATUS_REG, &msg, 6*4 + sizeof (struct rpc_request_hdr),
		  &rep, sizeof(rep), 5 * HZ);
    if (rc < 0)
        printk ("ERROR: %s:%d %d\n", __func__, __LINE__, rc);

    printk ("%s:%d ext_status_reg ret=0x%x\n", __func__, __LINE__, be32_to_cpu(rep.retval));

    /* pd_act */
    printk ("pd_act\n");
    msg.msg[0] = cpu_to_be32 (PD_CLIENT_ID);
    rc =
    msm_rpc_call_reply (ept, CLIENT_ACT, &msg, 1*4 + sizeof (struct rpc_request_hdr), 
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
    msg.msg[4] = cpu_to_be32 (0x3fefe0);
    msg.msg[5] = cpu_to_be32 (0xffffffff);
    rc =
    msm_rpc_call (ept, CLIENT_PD_REG, &msg, 6*4 + sizeof (struct rpc_request_hdr),
		  15 * HZ);
    if (rc < 0)
        printk ("%s:%d %d\n", __func__, __LINE__, rc);

    /* init xtra */
    msg.msg[0] = cpu_to_be32 (INIT_XTRA);
    rc =
    msm_rpc_call (ept, CLIENT_INIT, &msg, 1*4 + sizeof (struct rpc_request_hdr),
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
    rc =
    msm_rpc_call (ept, CLIENT_XTRA_REG, &msg, 6*4 + sizeof (struct rpc_request_hdr),
		  15 * HZ);
    if (rc < 0)
        printk ("%s:%d %d\n", __func__, __LINE__, rc);

    /* xtra_act */
    msg.msg[0] = cpu_to_be32 (XTRA_CLIENT_ID);
    rc =
    msm_rpc_call (ept, CLIENT_ACT, &msg, 1*4 + sizeof (struct rpc_request_hdr), 15 * HZ);
    if (rc)
        printk ("%s:%d %d\n", __func__, __LINE__, rc);

    /* init lcs */
    msg.msg[0] = cpu_to_be32 (INIT_LCS);
    rc =
    msm_rpc_call (ept, CLIENT_INIT, &msg, 1*4 + sizeof (struct rpc_request_hdr),
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
    rc =
    msm_rpc_call (ept, CLIENT_LCS_REG, &msg, 6*4 + sizeof (struct rpc_request_hdr),
		  15 * HZ);
    if (rc < 0)
        printk ("%s:%d %d\n", __func__, __LINE__, rc);

    /* lcs_act */
    msg.msg[0] = cpu_to_be32 (LCS_CLIENT_ID);
    rc =
    msm_rpc_call (ept, CLIENT_ACT, &msg, 1*4 + sizeof (struct rpc_request_hdr), 15 * HZ);
    if (rc)
        printk ("%s:%d %d\n", __func__, __LINE__, rc);

#endif
    printk ("get_position ->\n");
    pdsm_get_position ();
}

enum GpsState
{
    GPS_STATE_OFF = 0,
    GPS_STATE_ON,
};

struct gps_kernel_state
{
    spinlock_t lock;
    atomic_t updates;
    wait_queue_head_t wq;
    struct miscdevice misc;
     enum GpsState state;

     struct gps_state user_state;
};

static struct gps_kernel_state global_state;
static struct gps_kernel_state *state = &global_state;

static void
handle_sat_data (int last, int index, int num, struct sat_info sats[])
{
    int i;

    spin_lock (&state->lock);

    for (i = 0; i < num; i++)
    {
          state->user_state.sat_state[i + index].sat_no
	           = sats[i].prn;
          state->user_state.sat_state[i + index].signal_strength
	           = 0;
      }

    if (last)
    {
          for (i = i + index; i < MAX_SATELLITES; i++)
	{
	        state->user_state.sat_state[i].sat_no = 0;
	        state->user_state.sat_state[i].signal_strength = 0;
	    }

          atomic_inc (&state->updates);
          wake_up (&state->wq);
      }

    spin_unlock (&state->lock);
}

static void
handle_pos_data (int32_t lat, int32_t lon, uint32_t time, float altitude,
		 float accuracy)
{

    				/* Drop the data is we're supposed to be off */
      if (!state->state)
        return;

    spin_lock (&state->lock);

    state->user_state.time = time;
    state->user_state.lat  =lat;
    state->user_state.lng  =lon;
                    printk ("time=%d lat=%d lng=%d\n",
					      state->user_state.time,
					      state->user_state.lat,
					      state->user_state.lng);

    atomic_inc (&state->updates);
    wake_up (&state->wq);

    spin_unlock (&state->lock);
}

static void
handle_bearing_speed (float bearing, float speed)
{
    				//don't do anything.  Need to extend the userspace interface :(
}

/* This thread exists to read the data that comes from AMSS */
static int
gps_rpc_thread (void *data)
{
    struct msm_rpc_endpoint *ept = (struct msm_rpc_endpoint *) data;
    struct rpc_request_hdr *hdr = NULL;

    				/* We're not going to do anything but ack.
				        * Kaiser data comes form NMEA, currently */
      while (!kthread_should_stop ())
    {
#if 0
          msm_rpc_read (ept, (void **) &hdr, -1, 15*HZ);

          rpc_ack (be32_to_cpu (hdr->xid));
          kfree (hdr);
#endif
//       printk("gps_rpc_thread called\n");
      }

    return 0;
}

static int
gps_nmea_thread (void *data)
{
    int fd;
    int count;
    char buf[256];
    struct nmea_state nmea;

#if 0
    fd = sys_open ("/dev/smd27", O_RDONLY, 0);
    if (fd < 0)
        printk ("Unable to open NMEA device %d\n", fd);
    nmea_init (&nmea, NULL, handle_pos_data, handle_sat_data,
		        handle_bearing_speed);
#endif

    while (!kthread_should_stop ())
    {
//          if (state->state == GPS_STATE_ON)
	      pdsm_get_position ();
	      msleep(1000);
#if 0          
      do
	{
	        count = sys_read (fd, buf, sizeof (buf));
	    }
      while (count == -EINTR);
          printk ("NMEA read.  Size %d\n", count);
          nmea_parse (&nmea, buf, count);
#endif
      }

//    sys_close (fd);

    return 0;
}

struct gps_fd_state
{
    int last_read;
};

/* Userspace interface functions */

/* Return the current position and signal data to userspace.  Ideally
 * this would block if there's no new information, but currently you
 * must use select() if you want that behavior */
static ssize_t
vogue_gps_read (struct file *file, char __user * buf,
		                size_t len, loff_t * off)
{
    struct gps_fd_state *fd_state =
    (struct gps_fd_state *) file->private_data;
    int updates;
    int ret;
    struct gps_state user_state;

    if (len < sizeof (struct gps_state))
        return -EINVAL;

    updates = atomic_read (&state->updates);
    if (updates == fd_state->last_read)
    {
          printk ("%s waiting\n", __func__);
          ret = wait_event_interruptible (state->wq,
					                atomic_read
					      (&state->updates) > updates);
          if (ret)
	      return ret;
      }
    fd_state->last_read = atomic_read (&state->updates);

    spin_lock (&state->lock);
    memcpy (&user_state, &state->user_state, sizeof (struct gps_state));
    spin_unlock (&state->lock);

    if (copy_to_user (buf, &user_state, sizeof (struct gps_state)))
        return -EFAULT;

    return sizeof (struct gps_state);
}

/* Support polling.  select() is your friend */
static unsigned int
vogue_gps_poll (struct file *file,
		                  struct poll_table_struct
		*poll)
{
    struct gps_fd_state *fd_state =
    (struct gps_fd_state *) file->private_data;

    poll_wait (file, &state->wq, poll);
    if (atomic_read (&state->updates) > fd_state->last_read)
        return POLLIN | POLLRDNORM;
    return 0;
}

static long
vogue_gps_ioctl (struct file *file, unsigned int cmd,
		                unsigned long arg)
{
    switch (cmd)
    {
        case VGPS_IOC_ENABLE:
            if (!state->state)
	{
	          state->state = GPS_STATE_ON;
	          gps_enable ();
	      }
            break;

        case VGPS_IOC_DISABLE:
            gps_disable ();
            state->state = GPS_STATE_OFF;
            break;

        case VGPS_IOC_NEW_FIX:
            pdsm_get_position ();
            break;

        case VGPS_IOC_INFO:
      {
	      struct gps_info *dst = (struct gps_info *) arg;
	      struct gps_info info;

	      info.version = GPS_VERSION;
	      info.correction_factor = 1;
	      if (copy_to_user (dst, &info, sizeof (struct gps_info)))
	          return -EFAULT;
	      break;
          }

        default:
            return -EINVAL;
      }

    return 0;
}


static int
vogue_gps_open (struct inode *inode, struct file *file)
{
    static int thread_started = 0;
    struct gps_fd_state *fd_state;
    int rc;

static struct msm_rpc_endpoint *rpc_cb_server_client;

    if (!thread_started)
    {
          			/* Only do this on the first open.  This is less than ideal */
//	    msm_rpc_register_server (NULL, RPC_PDAPI_CB_PROG, 0);

#if 0
	rpc_cb_server_client = msm_rpc_open();
	if (IS_ERR(rpc_cb_server_client)) {
		rpc_cb_server_client = NULL;
		rc = PTR_ERR(rpc_cb_server_client);
		printk("gps: could not create rpc server (%d)\n", rc);
		return -1;
	}

	rc = msm_rpc_register_server(rpc_cb_server_client,
				     RPC_PDAPI_CB_PROG,
				     0);
	if (rc) {
		printk("gps: could not register callback server (%d)\n", rc);
		return -1;
	}
#endif
          ept = msm_rpc_connect (RPC_PDAPI_PROG, 0, MSM_RPC_UNINTERRUPTIBLE);
          if (!ept || IS_ERR (ept))
	{
	        printk ("msm_rpc_connect failed (%d)\n", (int) ept);
	        return -1;
	    }

//          kthread_run (gps_rpc_thread, ept, "gps_thread");
//          kthread_run (gps_nmea_thread, ept, "gps_nmea_thread");
      }

    fd_state = kmalloc (sizeof (struct gps_fd_state), GFP_KERNEL);
    fd_state->last_read = 0;
    file->private_data = fd_state;

    return 0;
}

static int
vogue_gps_release (struct inode *inode, struct file *file)
{
    struct gps_fd_state *fd_state =
    (struct gps_fd_state *) file->private_data;

    kfree (fd_state);
    file->private_data = NULL;
    return 0;
}

static struct file_operations vogue_gps_fops = {
    .open = vogue_gps_open,
    .release = vogue_gps_release,
    .read = vogue_gps_read,
    .unlocked_ioctl = vogue_gps_ioctl,
    .poll = vogue_gps_poll,
};

static int
vogue_gps_probe (struct platform_device *pdev)
{
    int ret;

#if 0
    gps_suspend_lock.name = "gps";
    ret = android_init_suspend_lock (&gps_suspend_lock);
    if (ret)
    {
          printk (KERN_ERR
		      "vogue_gps: android_init_suspend_lock failed (%d)\n",
		              ret);
          return ret;
      }
#endif
    init_waitqueue_head (&state->wq);
    spin_lock_init (&state->lock);

    state->misc.name = "vogue_gps";
    state->misc.minor = MISC_DYNAMIC_MINOR;
    state->misc.fops = &vogue_gps_fops;
    ret = misc_register (&state->misc);
    if (ret)
    {
          printk (KERN_ERR "vogue_gps: misc_register failed (%d)\n",
		              ret);
          return ret;
      }

    return 0;
}


static struct platform_driver raphael_gps_driver = {
    .probe = vogue_gps_probe,
    .driver = {
		     .name = "raphael_gps",
		     .owner = THIS_MODULE,
		   },
};

static int __init
gps_init (void)
{
    printk("GPS init\n");

    return platform_driver_register (&raphael_gps_driver);
}

module_init (gps_init);

