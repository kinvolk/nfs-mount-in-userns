/* Code picked from samples/vfs/test-fsmount.c */
// SPDX-License-Identifier: GPL-2.0-or-later
/* fd-based mount test.
 *
 * Copyright (C) 2017 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <linux/mount.h>
#include <sys/wait.h>

#define FSOPEN_CLOEXEC		0x00000001

#ifndef __NR_fsopen
#define __NR_fsopen -1
#endif
#ifndef __NR_fsmount
#define __NR_fsmount -1
#endif
#ifndef __NR_fsconfig
#define __NR_fsconfig -1
#endif
#ifndef __NR_move_mount
#define __NR_move_mount -1
#endif

static inline int fsopen(const char *fs_name, unsigned int flags)
{
	return syscall(__NR_fsopen, fs_name, flags);
}

static inline int fsmount(int fsfd, unsigned int flags, unsigned int ms_flags)
{
	return syscall(__NR_fsmount, fsfd, flags, ms_flags);
}

static inline int fsconfig(int fsfd, unsigned int cmd,
			   const char *key, const void *val, int aux)
{
	return syscall(__NR_fsconfig, fsfd, cmd, key, val, aux);
}

static inline int move_mount(int from_dfd, const char *from_pathname,
			     int to_dfd, const char *to_pathname,
			     unsigned int flags)
{
	return syscall(__NR_move_mount,
		       from_dfd, from_pathname,
		       to_dfd, to_pathname, flags);
}

static void check_messages(int fd)
{
	char buf[4096];
	int err, n;

	err = errno;

	for (;;) {
		n = read(fd, buf, sizeof(buf));
		if (n < 0)
			break;
		n -= 2;

		switch (buf[0]) {
		case 'e':
			fprintf(stderr, "Error: %*.*s\n", n, n, buf + 2);
			break;
		case 'w':
			fprintf(stderr, "Warning: %*.*s\n", n, n, buf + 2);
			break;
		case 'i':
			fprintf(stderr, "Info: %*.*s\n", n, n, buf + 2);
			break;
		}
	}

	errno = err;
}

static __attribute__((noreturn))
void mount_error(int fd, const char *s) {
	check_messages(fd);
	fprintf(stderr, "%s: %m\n", s);
	exit(1);
}

static int
sendfd(int sockfd, int baggagefd) {
	struct msghdr msg;
	struct iovec iov[1];
	char c = 0;
	union {
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(int))];
	} control_un;
	struct cmsghdr *cmptr;
	int ret;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_len = CMSG_LEN(sizeof(int));
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	*((int *) CMSG_DATA(cmptr)) = baggagefd;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov[0].iov_base = &c;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	ret = sendmsg(sockfd, &msg, 0);
	if (ret == 1)
		return 0;
	else
		return -1;
}

static int
receivefd(int sockfd, int *baggagefd) {
	struct msghdr msg;
	struct iovec iov[1];
	ssize_t n;
	char c;

	union {
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof (int))];
	} control_un;
	struct cmsghdr *cmptr;

	msg.msg_control  = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov[0].iov_base = &c;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	n = recvmsg(sockfd, &msg, 0);
	if (n <= 0)
		return n;

	if ( (cmptr = CMSG_FIRSTHDR(&msg)) != NULL && cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {
		assert(cmptr->cmsg_level == SOL_SOCKET);
		assert(cmptr->cmsg_type == SCM_RIGHTS);
		*baggagefd = *((int *) CMSG_DATA(cmptr));
	} else {
		*baggagefd = -1;
	}
	return 0;
}

int
get_sfd(char *netns_path, char *userns_path) {
	int netns_fd = open(netns_path, O_RDONLY);
	assert(netns_fd != -1);

	int userns_fd = open(userns_path, O_RDONLY);
	assert(userns_fd != -1);

	int sockfd[2];
	int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd);

	int childpid;
	if ((childpid = fork()) == 0) {
		// child
		close(sockfd[0]);

		ret = setns(netns_fd, 0);
		if (ret == -1) fprintf(stderr, "Error: %s\n", strerror(errno));
		assert(ret == 0);

		ret = setns(userns_fd, 0);
		if (ret == -1) fprintf(stderr, "Error: %s\n", strerror(errno));
		assert(ret == 0);

		int sfd;
		sfd = fsopen("nfs", FSOPEN_CLOEXEC);
		assert(sfd != -1);

		ret = sendfd(sockfd[1], sfd);
		assert(ret == 0);

		exit(0);
	}

	// parent
	close(sockfd[1]);

	int status;
	waitpid(childpid, &status, 0);
	if (WIFEXITED(status) == 0) {
		fprintf(stderr, "Error: child did not terminate\n");
		exit(1);
	}
	int fd_from_child;
	if ( (status = WEXITSTATUS(status)) == 0) {
		ret = receivefd(sockfd[0], &fd_from_child);
		assert(ret == 0);
	} else {
		fprintf(stderr, "Error: child returned %d\n", status);
		exit(1);
	}
	close(sockfd[0]);

	return fd_from_child;
}

void
finish_mount(int sfd) {
	int ret;
	int mfd;

	// mount("127.0.0.1:/tmp/nfsserver", "/mnt/nfs", "nfs", 0, "hard,vers=4.2,addr=127.0.0.1,clientaddr=127.0.0.1")
	// mount("127.0.0.1:/tmp/nfsserver", "/mnt/nfs", "nfs", 0, "hard,addr=127.0.0.1,vers=3,proto=tcp,mountvers=3,mountproto=udp,mountport=20048") = 0

	ret = fsconfig(sfd, FSCONFIG_SET_FLAG, "ro", NULL, 0);
	if (ret == -1) mount_error(sfd, "ro");
	assert(ret == 0);

	ret = fsconfig(sfd, FSCONFIG_SET_STRING, "source", "127.0.0.1:/tmp/nfsserver", 0);
	if (ret == -1) mount_error(sfd, "source");
	assert(ret == 0);

	ret = fsconfig(sfd, FSCONFIG_SET_FLAG, "hard", NULL, 0);
	if (ret == -1) mount_error(sfd, "hard");
	assert(ret == 0);

	ret = fsconfig(sfd, FSCONFIG_SET_STRING, "vers", "3", 0);
	if (ret == -1) mount_error(sfd, "vers");
	assert(ret == 0);

	ret = fsconfig(sfd, FSCONFIG_SET_STRING, "proto", "tcp", 0);
	if (ret == -1) mount_error(sfd, "proto");
	assert(ret == 0);

	ret = fsconfig(sfd, FSCONFIG_SET_STRING, "mountvers", "3", 0);
	if (ret == -1) mount_error(sfd, "mountvers");
	assert(ret == 0);

	ret = fsconfig(sfd, FSCONFIG_SET_STRING, "mountproto", "udp", 0);
	if (ret == -1) mount_error(sfd, "mountproto");
	assert(ret == 0);

	ret = fsconfig(sfd, FSCONFIG_SET_STRING, "mountport", "20048", 0);
	if (ret == -1) mount_error(sfd, "mountport");
	assert(ret == 0);

	ret = fsconfig(sfd, FSCONFIG_SET_STRING, "addr", "127.0.0.1", 0);
	if (ret == -1) mount_error(sfd, "addr");
	assert(ret == 0);

	ret = fsconfig(sfd, FSCONFIG_SET_STRING, "clientaddr", "127.0.0.1", 0);
	if (ret == -1) mount_error(sfd, "clientaddr");
	assert(ret == 0);

	ret = fsconfig(sfd, FSCONFIG_CMD_CREATE, NULL, NULL, 0);
	if (ret == -1) mount_error(sfd, "create");
	assert(ret == 0);

	mfd = fsmount(sfd, FSMOUNT_CLOEXEC, 0);
	if (mfd == -1) fprintf(stderr, "Error: %s\n", strerror(errno));
	assert(mfd >= 0);

	ret = close (sfd);
	assert(ret == 0);

	ret = move_mount(mfd, "", AT_FDCWD, "/mnt/nfs", MOVE_MOUNT_F_EMPTY_PATH);
	assert(ret == 0);

}

int
main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "%s /proc/PID/ns/net /proc/PID/ns/user\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int sfd;

	sfd = get_sfd(argv[1], argv[2]);
	assert(sfd >= 0);

	finish_mount(sfd);
	return 0;
}

