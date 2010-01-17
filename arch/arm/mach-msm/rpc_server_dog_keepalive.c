/* arch/arm/mach-msm/rpc_server_dog_keepalive.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Author: Iliyan Malchev <ibm@android.com>
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <mach/msm_rpcrouter.h>
#include <mach/amss_para.h>

#define DOG_KEEPALIVE_PROG	0x30000015

/* dog_keepalive server definitions */


static int handle_rpc_call(struct msm_rpc_server *server,
			   struct rpc_request_hdr *req, unsigned len)
{
	if (req->procedure == amss_get_num_value(RPC_DOG_KEEPALIVE_NULL))
		return 0;
	else if (req->procedure == amss_get_num_value(RPC_DOG_KEEPALIVE_BEACON)) {
		printk(KERN_INFO "DOG KEEPALIVE PING\n");
		return 0;
	}
	else 
		return -ENODEV;
	
}

static struct msm_rpc_server rpc_server = {
	.rpc_call = handle_rpc_call,
};

static int __init rpc_server_init(void)
{
	rpc_server.prog = DOG_KEEPALIVE_PROG;
	rpc_server.vers = amss_get_num_value(DOG_KEEPALIVE_VERS);
	return msm_rpc_create_server(&rpc_server);
}


module_init(rpc_server_init);
