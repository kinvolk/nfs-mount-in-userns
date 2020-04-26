nfs-mount-in-userns: nfs-mount-in-userns.c
	gcc -Wall -o nfs-mount-in-userns nfs-mount-in-userns.c

test:
	@echo unshare --user --map-root-user --fork --net
	@echo 'export PID=229974 ; sudo strace -f -s 64 ./nfs-mount-in-userns /proc/$$PID/ns/net /proc/$$PID/ns/net'

# As user:
# unshare --user --map-root-user --fork --mount --propagation slave
#
# As root:
# sudo $PWD/nfs-mount-in-userns /proc/$PID/ns/net /proc/$PID/ns/user /proc/$PID/ns/mnt
