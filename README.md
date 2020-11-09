# nfs-mount-in-userns

Experiment with NFS mount in a user namespace

This uses the new mount API in Linux 5.2. Instead of using the `mount()` system call, we use:
- `fsopen()`
- `fsconfig()`
- `fsmount()`
- `move_mount()`

For more information, read:
- [LWN article](https://lwn.net/Articles/802096/)
- [Linux documentation](https://www.kernel.org/doc/Documentation/filesystems/mount_api.txt)

## Steps

- Create a unix socketpair
- Fork a child process
- The child process joins different network, user and mount namespaces
- The child process opens the NFS filesystem driver with `fsopen` and get a file descriptor `sfd`
- The child process sends the `sfd` file descriptor to the parent process through the socketpair
- The parent process, still in the host namespace, finishes the mount with `fsconfig`, `fsmount` and `move_mount`

This results in a NFS mount on host mount namespace but with a
[struct fs_context](https://github.com/torvalds/linux/blob/v5.6/include/linux/fs_context.h#L97-L98)
configured with a different user and network namespaces.

## Output

```
Running ./nfs-mount-in-userns
strace: Process 4022 attached
[pid  4022] fsopen("nfs4", FSOPEN_CLOEXEC) = 6
[pid  4022] +++ exited with 0 +++
--- SIGCHLD {si_signo=SIGCHLD, si_code=CLD_EXITED, si_pid=4022, si_uid=0, si_status=0, si_utime=0, si_stime=0} ---
fsconfig(7, FSCONFIG_SET_STRING, "source", "127.0.0.1:/server", 0) = 0
fsconfig(7, FSCONFIG_SET_STRING, "vers", "4.2", 0) = 0
fsconfig(7, FSCONFIG_SET_STRING, "addr", "127.0.0.1", 0) = 0
fsconfig(7, FSCONFIG_SET_STRING, "clientaddr", "127.0.0.1", 0) = 0
fsconfig(7, FSCONFIG_CMD_CREATE, NULL, NULL, 0) = 0
fsmount(7, FSMOUNT_CLOEXEC, 0)          = 6
move_mount(6, "", AT_FDCWD, "/mnt/nfs", MOVE_MOUNT_F_EMPTY_PATH) = 0
+++ exited with 0 +++
./nfs-mount-in-userns returned 0
last dmesg line about nfs4_create_server
[55258.702256] nfs4_create_server: Using creds from non-init userns
459 55 0:40 / /mnt/nfs rw,relatime shared:187 - nfs4 127.0.0.1:/server rw,vers=4.2,rsize=524288,wsize=524288,namlen=255,hard,proto=tcp,timeo=600,retrans=2,sec=sys,clientaddr=127.0.0.1,local_lock=none,addr=127.0.0.1

+ : 'Files on the NFS server:'
+ ls -lani /server/
total 20
1048578 drwxr-xr-x.  5    0    0 4096 Nov 10 09:19 .
      2 dr-xr-xr-x. 21    0    0 4096 Nov  9 14:25 ..
1048582 drwx------.  2    0    0 4096 Nov 10 09:19 dir-0
1048583 drwx------.  2 1000 1000 4096 Nov 10 09:19 dir-1000
1048584 drwx------.  2 3000 3000 4096 Nov 10 09:19 dir-3000
1048579 -rw-------.  1    0    0    0 Nov 10 09:19 file-0
1048580 -rw-------.  1 1000 1000    0 Nov 10 09:19 file-1000
1048581 -rw-------.  1 3000 3000    0 Nov 10 09:19 file-3000

+ : 'Files on the NFS mountpoint (from container PoV):'
+ nsenter -U -m -n -t 4002 sh -c 'ls -lani /mnt/nfs'
total 20
1048578 drwxr-xr-x. 5     0     0 4096 Nov 10 09:19 .
 786433 drwxr-xr-x. 3 65534 65534 4096 May 16 16:08 ..
1048582 drwx------. 2     0     0 4096 Nov 10 09:19 dir-0
1048583 drwx------. 2 65534 65534 4096 Nov 10 09:19 dir-1000
1048584 drwx------. 2 65534 65534 4096 Nov 10 09:19 dir-3000
1048579 -rw-------. 1     0     0    0 Nov 10 09:19 file-0
1048580 -rw-------. 1 65534 65534    0 Nov 10 09:19 file-1000
1048581 -rw-------. 1 65534 65534    0 Nov 10 09:19 file-3000

+ : 'Files on the NFS mountpoint (from host PoV):'
+ ls -lani /mnt/nfs/
total 20
1048578 drwxr-xr-x. 5       1000       1000 4096 Nov 10 09:19 .
 786433 drwxr-xr-x. 3          0          0 4096 May 16 16:08 ..
1048582 drwx------. 2       1000       1000 4096 Nov 10 09:19 dir-0
1048583 drwx------. 2 4294967294 4294967294 4096 Nov 10 09:19 dir-1000
1048584 drwx------. 2 4294967294 4294967294 4096 Nov 10 09:19 dir-3000
1048579 -rw-------. 1       1000       1000    0 Nov 10 09:19 file-0
1048580 -rw-------. 1 4294967294 4294967294    0 Nov 10 09:19 file-1000
1048581 -rw-------. 1 4294967294 4294967294    0 Nov 10 09:19 file-3000
```

## Requirements

- Linux 5.10.0-rc3+
- https://marc.info/?l=linux-nfs&m=160433928122163&w=2
- https://marc.info/?l=linux-nfs&m=160433928122163&w=2
