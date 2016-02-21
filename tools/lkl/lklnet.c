#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <lkl.h>
#include <lkl_host.h>
#include <lkl/linux/if_tun.h>
#include <lkl/linux/if.h>
#include <lkl/linux/netlink.h>
#include <linux/if_tun.h>
#include <linux/if.h>
#include <sys/utsname.h>
//#include <linux/netfilter_ipv4/ip_tables.h>
#include <lkl/linux/netlink.h>
#include <lkl/linux/rtnetlink.h>
#include <lkl/linux/netfilter.h>
#include <lkl/linux/netfilter/x_tables.h>
#include <lkl/linux/netfilter/xt_TPROXY.h>
#include <lkl/linux/netfilter_ipv4/ip_tables.h>

static void start_lkl(void)
{
	long ret;

	ret = lkl_start_kernel(&lkl_host_ops, 16 * 1024 * 1024, "");
	if (ret) {
		fprintf(stderr, "can't start kernel: %s\n", lkl_strerror(ret));
		exit(1);
	}

	lkl_mount_sysfs();
	lkl_mount_proc();
}

static void stop_lkl(void)
{
	int ret;

	ret = lkl_sys_chdir("/");
	if (ret)
		fprintf(stderr, "can't chdir to /: %s\n", lkl_strerror(ret));
	lkl_sys_halt();
}

static int lkl_ifindex(const char* ifname) {
        struct lkl_ifreq ifr;
        int sock, ret;

        sock = lkl_sys_socket(LKL_AF_INET, LKL_SOCK_DGRAM, 0);
        if (sock < 0)
                return sock;

        strncpy(ifr.lkl_ifr_name, ifname, LKL_IFNAMSIZ);
        ret = lkl_sys_ioctl(sock, LKL_SIOCGIFINDEX, (long)&ifr);
        lkl_sys_close(sock);

        return ret < 0 ? ret : ifr.lkl_ifr_ifindex;
}

static int host_init() {
	struct ifreq ifr;
	int hostFd;
        if( (hostFd = open("/dev/net/tun", O_RDWR|O_NONBLOCK)) < 0 )
                exit(__LINE__);

        bzero(&ifr, sizeof ifr);
        ifr.ifr_flags = IFF_TUN; 
        strncpy(ifr.ifr_name, "tun_phh", IFNAMSIZ);

        if( ioctl(hostFd, TUNSETIFF, (void *) &ifr) < 0 )
		exit(__LINE__);
	return hostFd;
}

static void setup_tproxy() {
	//TODO:
	//Get TPROXY supported revision:
	//socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	//getsockopt(fd, SOL_IP, IPT_SO_GET_REVISION_MATCH, struct xt_get_revision);

	int fd = lkl_sys_socket(LKL_AF_INET, LKL_SOCK_RAW, LKL_IPPROTO_RAW);

	struct lkl_ipt_getinfo info;
	strcpy(info.name, "mangle");
	int s = sizeof(info);
	int ret = lkl_sys_getsockopt(fd, SOL_IP, LKL_IPT_SO_GET_INFO, (char*)&info, &s);
	if(ret<0)
		exit(__LINE__);

	fprintf(stderr, "Got %d entries\n", info.num_entries);
	s = info.size + sizeof(struct lkl_ipt_get_entries);
	struct lkl_ipt_get_entries *e = aligned_alloc(16, s);
	bzero(e, s);
	strcpy(e->name, "mangle");
	e->size = info.size;
	ret = lkl_sys_getsockopt(fd, SOL_IP, LKL_IPT_SO_GET_ENTRIES, (char*)e, &s);
	if(ret<0) {
		fprintf(stderr, "can't: %s\n", lkl_strerror(ret));
		exit(__LINE__);
	}

	//Construct the new rule
	int newEntryLen = sizeof(struct lkl_ipt_entry) +
		sizeof(struct lkl_xt_entry_target) +
		sizeof(struct lkl_xt_tproxy_target_info_v1);
	newEntryLen += 7;
	newEntryLen &= 0xfff8;
	struct lkl_ipt_entry *newEntry = calloc(newEntryLen, 1);
	strcpy(newEntry->ip.iniface,             "tun_phh");
	strcpy((char*)newEntry->ip.iniface_mask, "xxxxxxx"); // strlen(tun_phh)
	newEntry->ip.proto = LKL_IPPROTO_TCP;
	newEntry->target_offset = sizeof(struct lkl_ipt_entry);
	newEntry->next_offset = newEntryLen;
	struct lkl_xt_entry_target *newEntryTarget = (struct lkl_xt_entry_target*) &newEntry->elems[0];
	strcpy(newEntryTarget->u.user.name, "TPROXY");
	newEntryTarget->u.user.revision = 1;
	newEntryTarget->u.user.target_size = sizeof(struct lkl_xt_tproxy_target_info_v1) + sizeof(struct lkl_xt_entry_target);
	newEntryTarget->u.user.target_size += 7;
	newEntryTarget->u.user.target_size &= 0xfff8;
	struct lkl_xt_tproxy_target_info_v1 *tproxy = (struct lkl_xt_tproxy_target_info_v1*)&newEntryTarget->data[0];
	tproxy->lport = htons(2000);

	//Now replace the iptable
	s = info.size + sizeof(struct lkl_ipt_replace) + newEntryLen;
	struct lkl_ipt_replace *r = aligned_alloc(16, s);
	bzero(r, s);
	strcpy(r->name, "mangle");
	r->valid_hooks = info.valid_hooks;
	r->num_entries = info.num_entries+1;
	r->size = info.size + newEntryLen;
	//Do I really have to do that?
	r->num_counters = info.num_entries;
	r->counters = aligned_alloc(16, sizeof(r->counters[0])*info.num_entries);

	long offset = 0;
	long origOff = 0;

	struct lkl_ipt_entry *origEntry = &e->entrytable[0];
	int hookId = 0;
	for(unsigned i=0; i<info.num_entries; ++i) {
		origEntry = ((void*)&e->entrytable[0] + origOff);
		if(hookId < (NF_INET_NUMHOOKS-1) && origOff >= info.hook_entry[hookId+1])
			hookId++;

		if(origOff == info.hook_entry[NF_INET_PRE_ROUTING]) {
			fprintf(stderr, "Got at %p, here I come!\n", (void*)origOff);
			// Here we want to add a new rule
			r->hook_entry[hookId] = info.hook_entry[hookId] + offset;
			memcpy( (void*)&r->entries[0] + origOff + offset, newEntry, newEntryLen);
			offset = newEntryLen;
			r->underflow[hookId] = info.underflow[hookId] + offset;
		} else {
			//Simply copy this section
			r->hook_entry[hookId] = info.hook_entry[hookId] + offset;
			r->underflow[hookId] = info.underflow[hookId] + offset;
		}
		memcpy( (void*)&r->entries[0] + origOff + offset, origEntry, origEntry->next_offset);
		origOff += origEntry->next_offset;
	}

#if 0
	entry = &r->entries[0];
	for(unsigned i=0; i<r->num_entries; ++i) {
		printf("New entry @ %p\n", (long)entry - (long)&r->entries[0]);
		printf("\tCounters: %dB, %dP\n", entry->counters.bcnt, entry->counters.pcnt);
		if(entry->target_offset != sizeof(struct lkl_ipt_entry)) {
			struct lkl_xt_entry_match *m = entry + sizeof(struct lkl_ipt_entry);
			printf("\tMatch name is %s\n", m->u.user.name);
		}

		struct lkl_xt_entry_target *t = ipt_get_target(entry);
		printf("\tTarget name is %s, size %d\n", t->u.user.name, t->u.user.target_size);
		if(strcmp(t->u.user.name, LKL_XT_STANDARD_TARGET) == 0) {
			struct lkl_xt_standard_target *t2 = t;
			printf("\t\tStandard target verdict = %x\n", t2->verdict);
		} else if(strcmp(t->u.user.name, LKL_XT_ERROR_TARGET) == 0) {
			struct lkl_xt_error_target *t2 = t;
			printf("\t\tError target name = %s\n", t2->errorname);
		}
		entry = ((void*)entry + entry->next_offset);
	}
#endif

	ret = lkl_sys_setsockopt(fd, SOL_IP, LKL_IPT_SO_SET_REPLACE, (char*)r, s);
	if(ret<0) {
		fprintf(stderr, "can't: %s\n", lkl_strerror(ret));
		exit(__LINE__);
	}
	lkl_sys_close(fd);
}

static void dump_file(const char *path) {
	fprintf(stderr, "---- Starting dumping of %s\n", path);
	int fd = lkl_sys_open(path, O_RDONLY, 0666);
	if(fd<0) exit(__LINE__);
	char v[1024*256];
	int ret = lkl_sys_read(fd, v, sizeof(v));
	if(ret<0) exit(__LINE__);
	write(2, v, ret);
	lkl_sys_close(fd);
	fprintf(stderr, "---- Done dumping\n");
}

static void write_bool(const char *path, int v) {
	int fd = lkl_sys_open(path, O_WRONLY, 0644);
	if(fd<0)
		exit(__LINE__);
	int ret = lkl_sys_write(fd, v ? "1\n" : "0\n", 2);
	if(ret<0)
		exit(__LINE__);
	lkl_sys_close(fd);
}

static void stuff() {
#if 0
	dump_file("/proc/devices");
	dump_file("/proc/misc");
	dump_file("/proc/filesystems");
	dump_file("/proc/net/ip_tables_targets");
	dump_file("/proc/net/ip_tables_names");
	dump_file("/proc/net/ip_tables_matches");
	dump_file("/proc/mounts");
	dump_file("/proc/meminfo");
#endif

	write_bool("/proc/sys/net/ipv4/ip_forward", 1);
	write_bool("/proc/sys/net/ipv4/conf/default/rp_filter", 0);
	write_bool("/proc/sys/net/ipv4/conf/all/rp_filter", 0);
	write_bool("/proc/sys/net/ipv4/conf/tun_phh/rp_filter", 0);

	dump_file("/proc/sys/net/ipv4/ip_forward");

	// ip -f inet route add local default dev lo
	int fd = lkl_sys_socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	int ret;

	struct sockaddr_nl snl;
	bzero(&snl, sizeof(snl));
	snl.nl_family = AF_NETLINK;
	ret = lkl_sys_bind(fd, (struct lkl_sockaddr*)&snl, sizeof(snl));
	if(ret<0) exit(__LINE__);

	struct {
		struct lkl_nlmsghdr hdr;
		struct lkl_rtmsg msg;

		struct lkl_rtattr gwAttr;
		uint32_t iface;
	} rtMsg = {
                .hdr = {
                        .nlmsg_len = sizeof(rtMsg),
                        .nlmsg_type = LKL_RTM_NEWROUTE,
                        .nlmsg_flags = NLM_F_CREATE | NLM_F_EXCL | NLM_F_REQUEST,
                        .nlmsg_seq = 0xf0000000,
                        .nlmsg_pid = 0,
                },
                .msg = {
                        .rtm_family = LKL_AF_INET,
                        .rtm_dst_len = 0,
                        .rtm_src_len = 0,
                        .rtm_tos = 0,
                        .rtm_table = LKL_RT_TABLE_MAIN,
                        .rtm_protocol = LKL_RTPROT_BOOT,
                        .rtm_scope = LKL_RT_SCOPE_HOST,
                        .rtm_type = LKL_RTN_LOCAL,
                        .rtm_flags = 0,
                },
                .gwAttr = {
                        .rta_len = sizeof(struct lkl_rtattr)+sizeof(uint32_t),
                        .rta_type = LKL_RTA_OIF,
                },
                .iface = lkl_ifindex("lo"),
	};
	lkl_sys_write(fd, (char*)&rtMsg, sizeof(rtMsg));
	lkl_sys_close(fd);
}

int main(int argc, char **argv)
{
	int hostFd;
	if(argc>1)
		hostFd = atoi(argv[1]);
	else
		hostFd = host_init();

	start_lkl();
	long ret = lkl_sys_mknod("/tun", LKL_S_IFCHR | 0600, LKL_MKDEV(10, 200));
	if (ret) {
		fprintf(stderr, "can't start kernel: %s\n", lkl_strerror(ret));
		exit(1);
	}
	long lklTun = lkl_sys_open("/tun", O_RDWR|O_NONBLOCK, 0666);
	if (lklTun < 0) {
		fprintf(stderr, "can't open tun: %s\n", lkl_strerror(ret));
		exit(1);
	}

	struct lkl_ifreq ifr;
	bzero(&ifr, sizeof ifr);
	ifr.ifr_ifru.ifru_flags = LKL_IFF_TUN; 
	strncpy(ifr.ifr_ifrn.ifrn_name, "tun_phh", LKL_IFNAMSIZ);

	if( (ret = lkl_sys_ioctl(lklTun, LKL_TUNSETIFF, (long) &ifr)) < 0 ){
		fprintf(stderr, "can't open tun: %s\n", lkl_strerror(ret));
		exit(1);
	}

	int netid = lkl_ifindex("tun_phh");
	fprintf(stderr, "Interface has id = %d\n", netid);
	lkl_if_up(netid);
	lkl_if_set_mtu(netid, 1500);
	lkl_if_set_ipv4(netid, inet_addr("192.168.42.1"), 29);

	netid = lkl_ifindex("dummy0");
	lkl_if_up(netid);
	lkl_if_set_mtu(netid, 1500);
	lkl_if_set_ipv4(netid, inet_addr("192.168.0.2"), 24);

	netid = lkl_ifindex("lo");
	fprintf(stderr, "Lookbackup ifindex = %d\n", netid);
	lkl_if_up(netid);

	stuff();

	int tcpFd = lkl_sys_socket(LKL_AF_INET, LKL_SOCK_STREAM, 0);
	struct lkl_sockaddr_in sin;
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(2000);
	ret = lkl_sys_bind(tcpFd, (struct lkl_sockaddr*)&sin, sizeof(sin));
	if (lklTun < 0) {
		fprintf(stderr, "can't bind: %s\n", lkl_strerror(ret));
		exit(1);
	}

	int value = 1;
	lkl_sys_setsockopt(tcpFd, SOL_IP, LKL_IP_TRANSPARENT, (char*)&value, sizeof(value));

	lkl_sys_listen(tcpFd, 10);
	int flags = lkl_sys_fcntl(tcpFd, LKL_F_GETFL, 0);
	ret = lkl_sys_fcntl(tcpFd, LKL_F_SETFL, flags | LKL_O_NONBLOCK);
	if(ret < 0) {
		fprintf(stderr, "Can't nonblock tcpfd: %s\n", lkl_strerror(ret));
		exit(1);
	}

	setup_tproxy();

	while(1) {
		char buffer[1600];
		int len;
		
		len = read(hostFd, buffer, sizeof(buffer));
		if(len >= 0)
			lkl_sys_write(lklTun, buffer, len);

		len = lkl_sys_read(lklTun, buffer, sizeof(buffer));
		if(len >= 0)
			write(hostFd, buffer, len);

		int cfd = lkl_sys_accept(tcpFd, NULL, NULL);
		if(cfd>=0) {
			struct sockaddr_in sin;
			int s = sizeof(sin);
			int ret = lkl_sys_getsockname(cfd, (struct lkl_sockaddr*)&sin, &s);
			fprintf(stderr, "Got new connection %d %d.%d.%d.%d %d!\n",
					ret,
					sin.sin_addr.s_addr & 0xff,
					sin.sin_addr.s_addr >> 8 & 0xff,
					sin.sin_addr.s_addr >> 16 & 0xff,
					sin.sin_addr.s_addr >> 24 & 0xff,
					htons(sin.sin_port));
			lkl_sys_write(cfd, "Hello\n", 6);
			lkl_sys_close(cfd);
		}
		
	}

	stop_lkl();

	return 0;
}
