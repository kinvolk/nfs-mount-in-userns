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
//#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <linux/mount.h>
#include <linux/fcntl.h>



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
void mount_error(int fd, const char *s)
{
	check_messages(fd);
	fprintf(stderr, "%s: %m\n", s);
	exit(1);
}

int
main(int argc, char **argv) {
	int ret;
	int sfd;
	int mfd;

	// mount("127.0.0.1:/tmp/nfsserver", "/mnt/nfs", "nfs", 0, "hard,vers=4.2,addr=127.0.0.1,clientaddr=127.0.0.1")
	// mount("127.0.0.1:/tmp/nfsserver", "/mnt/nfs", "nfs", 0, "hard,addr=127.0.0.1,vers=3,proto=tcp,mountvers=3,mountproto=udp,mountport=20048") = 0

	sfd = fsopen("nfs", FSOPEN_CLOEXEC);
	assert(sfd >= 0);

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
	if (mfd == -1)
		fprintf(stderr, "Error: %s\n", strerror(errno));
	assert(mfd >= 0);

	ret = close (sfd);
	assert(ret == 0);

	ret = move_mount(mfd, "", AT_FDCWD, "/mnt/nfs", MOVE_MOUNT_F_EMPTY_PATH);
	assert(ret == 0);


	return 0;
}

