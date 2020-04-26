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

## Limitations

- NFS UDP port currently hardcoded. You will need to adapt it for your system before trying it
- Although the in-kernel NFS driver uses the network namespace configured in this manner, it doesn't use the user namespaces for uid translations. We'll still need to patch the kernel.

## Requirements

- Linux 5.6 (see ["NFS: Add fs_context support"](https://github.com/torvalds/linux/commit/f2aedb713c284429987dc66c7aaf38decfc8da2a))
