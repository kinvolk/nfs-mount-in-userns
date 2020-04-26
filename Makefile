new-mount-nfs: new-mount-nfs.c
	gcc -Wall -o new-mount-nfs new-mount-nfs.c

test:
	@echo unshare --user --map-root-user --fork --net
	@echo 'export PID=229974 ; sudo strace -f -s 64 ./new-mount-nfs /proc/$$PID/ns/net /proc/$$PID/ns/net'

# As user:
# unshare --user --map-root-user --fork --mount --propagation slave
#
# As root:
# sudo $PWD/new-mount-nfs /proc/$PID/ns/net /proc/$PID/ns/user /proc/$PID/ns/mnt
