#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <linux/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <lkl.h>
#include <lkl_host.h>
#include <usbip_protocol.h>

int lkl_register_usbip(int port, int sock, int devid, int speed) {
	int fd = lkl_sys_open("/sys/devices/platform/vhci_hcd/attach", O_WRONLY, 0666);
	char *str = NULL;
	int l = asprintf(&str, "%u %u %u %u", port, sock, devid, speed);
	lkl_sys_write(fd, str, l);
	lkl_sys_close(fd);
	free(str);
	return 0;
}

typedef struct {
	int hostFd;
	int lklFd;
} hostReaderWork;

void *hostReader(void* arg) {
	lkl_create_syscall_thread();
	hostReaderWork *v = (hostReaderWork*)arg;
	while(1) {
		char buf[8192];
		int l = read(v->hostFd, buf, sizeof(buf));
		if(l <= 0) {
			fprintf(stderr, "Got a %d read\n", l);
			perror("read");
			exit(__LINE__);
		}
		lkl_sys_write(v->lklFd, buf, l);
	}
}

int usbip_connect(int lklFd) {
	int ret, l;
	int hostFd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_addr = {
			.s_addr = htonl(INADDR_LOOPBACK),
		},
		.sin_port = htons(3240),
	};
	ret = connect(hostFd, (struct sockaddr*)&sin, sizeof(sin));
	if(ret) exit(__LINE__);

	struct op_common c = {
		.version = htons(0x111),
		.code = htons(OP_REQ_IMPORT),
		.status = 0,
	};
	write(hostFd, &c, sizeof(c));

	struct op_import_request r = {
		.busid = "3-7",
	};
	write(hostFd, &r, sizeof(r));


	read(hostFd, &c, sizeof(c));
	struct op_import_reply reply;
	l = read(hostFd, &reply, sizeof(reply));
	fprintf(stderr, "Got device = %d %s:%s %d %d\n", l,
			reply.udev.path, reply.udev.busid,
			htonl(reply.udev.busnum), htonl(reply.udev.devnum));

	int devid = htonl(reply.udev.busnum) << 16 | htonl(reply.udev.devnum);
	lkl_register_usbip(3, lklFd, devid, htonl(reply.udev.speed));
	lkl_sys_close(lklFd);

	return hostFd;
}

void *mounter(void *t) {
	int dev = makedev(8, 1);
	lkl_sys_mknod("/sda1", LKL_S_IFBLK|0600, dev);
	while(1) {
		int fd = lkl_sys_open("/sda1", O_RDONLY, 0600);
		if(fd>=0) {
			fprintf(stderr, "Opened sda!\n");
			lkl_sys_close(fd);
			break;
		}
		struct timespec ts = { 0, 1000*1000*10};
		lkl_sys_nanosleep(&ts, NULL);
	}

	lkl_sys_mkdir("/mnt", 0600);
	lkl_sys_mount("/sda1", "/mnt", "vfat", 0, NULL);

	int err = 0;
	void *p = lkl_opendir("/mnt/", &err);
	if(!p) exit(__LINE__);
	while(1) {
		struct lkl_linux_dirent64 *d = lkl_readdir(p);
		fprintf(stderr, "File %s\n", d->d_name);
	}
	lkl_closedir(p);
}

int main() {
	int ret;

	ret = lkl_start_kernel(&lkl_host_ops, 16 * 1024 * 1024, "");
	if (ret) {
		fprintf(stderr, "can't start kernel: %s\n", lkl_strerror(ret));
		exit(__LINE__);
	}
	lkl_create_syscall_thread();

	lkl_mount_sysfs();
	lkl_mount_proc();

	int fds[2];
	ret = lkl_sys_socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
	if (ret) {
		fprintf(stderr, "can't socketpair: %s\n", lkl_strerror(ret));
		exit(__LINE__);
	}

	int lklFd = fds[1];
	int hostFd = usbip_connect(fds[0]);

	pthread_t p;
	hostReaderWork w = { hostFd, lklFd };
	pthread_create(&p, NULL, hostReader, &w);

	pthread_t p1;
	pthread_create(&p, NULL, mounter, NULL);

	while(1) {
		char buf[8192];
		int l = lkl_sys_read(lklFd, buf, sizeof(buf));
		write(hostFd, buf, l);
	}


	return 0;
}
