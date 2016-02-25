#include <assert.h>
#include <stdbool.h>
#include <malloc.h>
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
#include <poll.h>
#include <lkl.h>
#include <sys/epoll.h>
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
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	strncpy(ifr.ifr_name, "tun_phh", IFNAMSIZ);

	if( ioctl(hostFd, TUNSETIFF, (void *) &ifr) < 0 )
		exit(__LINE__);
	return hostFd;
}

static int lkl_tun() {
	long ret = lkl_sys_mknod("/tun", LKL_S_IFCHR | 0600, LKL_MKDEV(10, 200));
	if (ret) {
		fprintf(stderr, "can't start kernel: %s\n", lkl_strerror(ret));
		exit(1);
	}
	long lklTun = lkl_sys_open("/tun", O_RDWR|O_NONBLOCK, 0666);
	if (lklTun < 0) {
		fprintf(stderr, "can't open tun: %s\n", lkl_strerror(lklTun));
		exit(__LINE__);
	}

	struct lkl_ifreq ifr;
	bzero(&ifr, sizeof ifr);
	ifr.ifr_ifru.ifru_flags = LKL_IFF_TUN | LKL_IFF_NO_PI;
	strncpy(ifr.ifr_ifrn.ifrn_name, "tun_phh", LKL_IFNAMSIZ);

	if( (ret = lkl_sys_ioctl(lklTun, LKL_TUNSETIFF, (long) &ifr)) < 0 ){
		fprintf(stderr, "can't open tun: %s\n", lkl_strerror(ret));
		exit(__LINE__);
	}

	return lklTun;
}

static void setup_tproxy() {
	int fd = lkl_sys_socket(LKL_AF_INET, LKL_SOCK_RAW, LKL_IPPROTO_RAW);
	int s, ret;

	struct lkl_ipt_getinfo info;
	{
		strcpy(info.name, "mangle");
		s = sizeof(info);
		ret = lkl_sys_getsockopt(fd, SOL_IP, LKL_IPT_SO_GET_INFO, (char*)&info, &s);
		if(ret<0)
			exit(__LINE__);
	}

	s = info.size + sizeof(struct lkl_ipt_get_entries);
	struct lkl_ipt_get_entries *e = memalign(16, s);
	{
		bzero(e, s);
		strcpy(e->name, "mangle");
		e->size = info.size;
		ret = lkl_sys_getsockopt(fd, SOL_IP, LKL_IPT_SO_GET_ENTRIES, (char*)e, &s);
		if(ret<0) {
			fprintf(stderr, "can't: %s\n", lkl_strerror(ret));
			exit(__LINE__);
		}
	}

	//Construct the new rule
	int newEntryLen = sizeof(struct lkl_ipt_entry) +
		sizeof(struct lkl_xt_entry_target) +
		sizeof(struct lkl_xt_tproxy_target_info_v1);
	newEntryLen += 7;
	newEntryLen &= 0xfff8;
	struct lkl_ipt_entry *newEntry = calloc(newEntryLen, 1);
	{
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
	}

	//Now replace the iptable
	s = info.size + sizeof(struct lkl_ipt_replace) + newEntryLen;
	struct lkl_ipt_replace *r = memalign(16, s);
	{
		bzero(r, s);
		strcpy(r->name, "mangle");
		r->valid_hooks = info.valid_hooks;
		r->num_entries = info.num_entries+1;
		r->size = info.size + newEntryLen;
		//Do I really have to do that?
		r->num_counters = info.num_entries;
		r->counters = memalign(16, sizeof(r->counters[0])*info.num_entries);

		long offset = 0;
		long origOff = 0;

		struct lkl_ipt_entry *origEntry = &e->entrytable[0];
		int hookId = 0;
		for(unsigned i=0; i<info.num_entries; ++i) {
			origEntry = ((void*)&e->entrytable[0] + origOff);
			if(hookId < (NF_INET_NUMHOOKS-1) && origOff >= info.hook_entry[hookId+1])
				hookId++;

			if(origOff == info.hook_entry[NF_INET_PRE_ROUTING]) {
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
	}

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

static void setup_route() {
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

static void stuff() {
#if 1
	dump_file("/proc/devices");
	dump_file("/proc/misc");
	dump_file("/proc/filesystems");
	dump_file("/proc/net/ip_tables_targets");
	dump_file("/proc/net/ip_tables_names");
	dump_file("/proc/net/ip_tables_matches");
	dump_file("/proc/mounts");
	dump_file("/proc/meminfo");
	dump_file("/proc/interrupts");
#endif

	write_bool("/proc/sys/net/ipv4/ip_forward", 1);
	write_bool("/proc/sys/net/ipv4/conf/default/rp_filter", 0);
	write_bool("/proc/sys/net/ipv4/conf/all/rp_filter", 0);
	write_bool("/proc/sys/net/ipv4/conf/tun_phh/rp_filter", 0);
}

static int setup_listener() {
	int tcpFd = lkl_sys_socket(LKL_AF_INET, LKL_SOCK_STREAM, 0);
	struct lkl_sockaddr_in sin;
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(2000);
	long ret = lkl_sys_bind(tcpFd, (struct lkl_sockaddr*)&sin, sizeof(sin));
	if (ret < 0) {
		fprintf(stderr, "can't bind: %s\n", lkl_strerror(ret));
		exit(1);
	}

	int value = 1;
	lkl_sys_setsockopt(tcpFd, SOL_IP, LKL_IP_TRANSPARENT, (char*)&value, sizeof(value));

	lkl_sys_listen(tcpFd, 10);
#if 1
	int flags = lkl_sys_fcntl64(tcpFd, LKL_F_GETFL, 0);
	ret = lkl_sys_fcntl64(tcpFd, LKL_F_SETFL, flags | LKL_O_NONBLOCK);
#else
	int flags = lkl_sys_fcntl(tcpFd, LKL_F_GETFL, 0);
	ret = lkl_sys_fcntl(tcpFd, LKL_F_SETFL, flags | LKL_O_NONBLOCK);
#endif
	if(ret < 0) {
		fprintf(stderr, "Can't nonblock tcpfd: %s\n", lkl_strerror(ret));
		exit(1);
	}

	return tcpFd;
}

#define N_CONNECTS 3
#define MAX_CLIENTS 1024
struct client {
	int client;
	int remote[N_CONNECTS];
	int remoteConnected;
	int valid;
} clients[MAX_CLIENTS];
int tcpFd;
int hostEPoll;
int lklEPoll;

void phh_host_init(int hostTun) {
	hostEPoll = epoll_create(42);

	struct epoll_event e;
	e.events = EPOLLIN;
	e.data.ptr = NULL;
	epoll_ctl(hostEPoll, EPOLL_CTL_ADD, hostTun, &e);
}

struct hostWorker {
	int hostFd;
	int lklTun;
};

void phh_host_work(struct hostWorker* hw) {
	struct epoll_event events[10];

	int n = epoll_wait(hostEPoll, events, sizeof(events)/sizeof(events[0]), -1);
	for(int i=0; i<n; ++i) {
		if(events[i].data.ptr == NULL) {
			//It is the tun fd
			char buffer[1600];
			int len = read(hw->hostFd, buffer, sizeof(buffer));
			if(len >= 0)
				lkl_sys_write(hw->lklTun, buffer, len);
			continue;
		}

		//We are either in a connecting fd, or in a connected fd
		long v = (long)events[i].data.ptr;
		v -= (v- (long) clients)%sizeof(struct client);

		int *fd = (int*) events[i].data.ptr;
		struct client *c = (struct client*) v;
		assert(c->valid);

		if(c->remoteConnected != -1) {
			fprintf(stderr, "Got an info from a connected fd %x!\n", events[i].events);

			//This most probably means we got two ACK at the same time for the same connection
			if(events[i].events & EPOLLOUT &&
					*fd == -1)
				continue;

			//We are in a connected fd
			assert(c->remoteConnected == *fd);
			if(events[i].events & EPOLLRDHUP) {
				while(1) {
					char buffer[64*1024];
					int l = read(*fd, buffer, sizeof(buffer));
					lkl_sys_write(c->client, buffer, l);
					if(!l)
						break;
				}
				close(c->remoteConnected);
				lkl_sys_close(c->client);
				c->valid = 0;
				continue;
			}
			assert(events[i].events & EPOLLIN);
			//TODO: check c->client has enough buffer place
			char buffer[64*1024];
			int l = read(*fd, buffer, sizeof(buffer));
			lkl_sys_write(c->client, buffer, l);
			fprintf(stderr, "Read %d bytes\n", l);
			continue;
		}

		int err;
		socklen_t s = sizeof(err);
		getsockopt(*fd, SOL_SOCKET, SO_ERROR, &err, &s);
		fprintf(stderr, "Got an info from a connecting fd (status %d)!\n", err);

		if(err == 0) {
			//We are in a connecting=>ed fd
			for(int j=0; j<N_CONNECTS; ++j) {
				if(c->remote[j] != -1 && c->remote[j] != *fd) {
					close(c->remote[j]);
					c->remote[j] = -1;
				}
			}
			c->remoteConnected = *fd;

			struct epoll_event e;
			bzero(&e, sizeof(struct epoll_event));
			e.events = EPOLLIN | EPOLLRDHUP;
			e.data.ptr = events[i].data.ptr;
			epoll_ctl(hostEPoll, EPOLL_CTL_MOD, *fd, &e);

			e.events = EPOLLIN | EPOLLRDHUP;
			e.data.ptr = c;
			lkl_sys_epoll_ctl(lklEPoll, EPOLL_CTL_ADD, c->client, (struct lkl_epoll_event*)&e);
		} else {
			//We are in a connecting=>dead fd
			fprintf(stderr, "Got a dying socket!\n");
			close(*fd);
			*fd = -1;
			int completlyDead = 1;
			for(int j=0; j<N_CONNECTS; ++j) {
				if(c->remote[j] != -1)
					completlyDead = 0;
			}
			if(completlyDead) {
				lkl_sys_close(c->client);
				c->valid = 0;
				c->client = -1;
			}
		}
	}
}

void *phh_host_thread(struct hostWorker* hw) {
	long ret = lkl_create_syscall_thread();
	if (ret < 0)
		fprintf(stderr, "%s: %s\n", __func__, lkl_strerror(ret));
	while(1) {
		phh_host_work(hw);
	}
	return NULL;
}

int client_connect(int lklFd, int connId) {
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | LKL_O_NONBLOCK);
	struct sockaddr_in sin;
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(80);
	sin.sin_addr.s_addr = inet_addr("77.154.221.251");

	int ret = connect(fd, (struct sockaddr*)&sin, sizeof(sin));
	fprintf(stderr, "Connect returned %d\n", ret);

	return fd;
}

void handle_new_connection(int lklFd) {
	int myClient = -1;
	for(int i=0; i<MAX_CLIENTS; ++i) {
		if(!clients[i].valid) {
			myClient = i;
			break;
		}
	}
	assert(myClient != -1);

	clients[myClient].valid = 1;
	clients[myClient].client = lklFd;
	clients[myClient].remote[0] = client_connect(lklFd, 0);
	clients[myClient].remote[1] = client_connect(lklFd, 1);
	for(int i=2; i<N_CONNECTS; ++i)
		clients[myClient].remote[i] = -1;
	clients[myClient].remoteConnected = -1;

	struct epoll_event e0,e1;
	bzero(&e0, sizeof(struct epoll_event));
	bzero(&e1, sizeof(struct epoll_event));
	e0.events = EPOLLOUT;
	e1.events = EPOLLOUT;

	e0.data.ptr = &(clients[myClient].remote[0]);
	e1.data.ptr = &(clients[myClient].remote[1]);
	epoll_ctl(hostEPoll, EPOLL_CTL_ADD, clients[myClient].remote[0], &e0);
	epoll_ctl(hostEPoll, EPOLL_CTL_ADD, clients[myClient].remote[1], &e1);
}

void phh_lkl_init(int lklTun, int tcpFd) {
	lklEPoll = lkl_sys_epoll_create(42);

	struct epoll_event e;
	e.events = EPOLLIN;
	e.data.u64 = 0;
	lkl_sys_epoll_ctl(lklEPoll, EPOLL_CTL_ADD, lklTun, (struct lkl_epoll_event*)&e);

	e.events = EPOLLIN;
	e.data.u64 = 1;
	lkl_sys_epoll_ctl(lklEPoll, EPOLL_CTL_ADD, tcpFd, (struct lkl_epoll_event*)&e);
}

void phh_lkl_work(int hostFd, int lklTun) {
	struct epoll_event events[10];
	int n = lkl_sys_epoll_wait(lklEPoll, (struct lkl_epoll_event*)events, sizeof(events)/sizeof(events[0]), -1);
	for(int i=0; i<n; ++i) {
		if(events[i].data.u64 == 0) {
			//lklTun
			char buffer[1600];
			long len = lkl_sys_read(lklTun, buffer, sizeof(buffer));
			if(len >= 0)
				write(hostFd, buffer, len);
			continue;
		}
		if(events[i].data.u64 == 1) {
			int cfd = lkl_sys_accept(tcpFd, NULL, NULL);
			assert(cfd >= 0);
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

			handle_new_connection(cfd);
			continue;
		}

		struct client *c = (struct client*) events[i].data.ptr;
		if(events[i].events & EPOLLRDHUP) {
			lkl_sys_close(c->client);
			close(c->remoteConnected);
		} else if(events[i].events & EPOLLIN) {
			char buffer[64*1024];
			int l = lkl_sys_read(c->client, buffer, sizeof(buffer));
			int ret = write(c->remoteConnected, buffer, l);
			fprintf(stderr, "Read %d, written %d\n", l, ret);
		}
	}
}

int main(int argc, char **argv)
{
	int hostFd;
	if(argc>1)
		hostFd = atoi(argv[1]);
	else
		hostFd = host_init();
	fprintf(stderr, "host tun fd = %d, %s\n", hostFd, argv[1]);
	char buf[512];
	readlink("/proc/self/fd/27", buf, sizeof(buf));
	fprintf(stderr, "host tun fd = %s\n", buf);

	//Make printk silent
	//lkl_host_ops.print = NULL;

	start_lkl();

	long lklTun = lkl_tun();

	int netid = lkl_ifindex("tun_phh");
	lkl_if_up(netid);
	lkl_if_set_mtu(netid, 1500);
	lkl_if_set_ipv4(netid, inet_addr("192.168.42.1"), 29);

	lkl_if_up(lkl_ifindex("lo"));

	stuff();
	setup_route();
	setup_tproxy();

	tcpFd = setup_listener();
	fprintf(stderr, "Listening...\n");
	phh_host_init(hostFd);
	phh_lkl_init(lklTun, tcpFd);

	struct hostWorker hw;
	hw.hostFd = hostFd;
	hw.lklTun = lklTun;

	pthread_t t;
	pthread_create(&t, NULL, phh_host_thread, (void*)&hw);

	//HACK: lkl_create_syscall_thread must be called without having a blocking syscall
	sleep(1);
	while(1) {
		phh_lkl_work(hostFd, lklTun);
	}

	stop_lkl();

	return 0;
}
