#!/bin/bash

echo 65535 > /proc/sys/sunrpc/nfs_debug
echo 65535 > /proc/sys/sunrpc/nfsd_debug
echo 65535 > /proc/sys/sunrpc/nlm_debug
echo 65535 > /proc/sys/sunrpc/rpc_debug

umount /mnt/nfs 2>/dev/null || true
/bin/rm -f /tmp/sleeper.pid

su -c "unshare --user -m --propagation=slave --map-root-user --fork sh -c 'echo \$\$ > /tmp/sleeper.pid ; cat /proc/self/uid_map; exec sleep infinity'" -- $(id -un 1000) &
#unshare --user --map-root-user --fork sh -c 'echo $$ > /tmp/sleeper.pid ; exec sleep infinity' &

sleep 0.5

echo "Files on the server:"
mkdir -p /server/
rm -rf /server/*
touch /server/file-{0,1000,3000}
mkdir /server/dir-{0,1000,3000}
chown 0:0 /server
chown 0:0 /server/{dir,file}-0
chown 1000:1000 /server/{dir,file}-1000
chown 3000:3000 /server/{dir,file}-3000
chmod 600 /server/file-{0,1000,3000}
chmod 700 /server/dir-{0,1000,3000}
ls -lan /server

echo "Clear dmesg"
dmesg -c &>/dev/null

sleep 0.5

PID=$(cat /tmp/sleeper.pid)
echo Running ./nfs-mount-in-userns
PID_TO_JOIN=$PID
strace -f -s 64 -e fsopen,fsconfig,fsmount,move_mount,mount \
./nfs-mount-in-userns /proc/$PID_TO_JOIN/ns/net /proc/$PID_TO_JOIN/ns/user /proc/$PID_TO_JOIN/ns/mnt
echo "./nfs-mount-in-userns returned $?"

echo "last dmesg line about nfs4_create_server"
dmesg|grep nfs4_create_server|tail -1

cat /proc/self/mountinfo | grep /mnt/nfs

set -x

: "Files on the NFS server:"
ls -lani /server/

: "Files on the NFS mountpoint (from container PoV):"
nsenter -U -m -n -t $PID sh -c "ls -lani /mnt/nfs"

: "Files on the NFS mountpoint (from host PoV):"
ls -lani /mnt/nfs/

set +x

#echo "Modules for NFS:"
#lsmod | grep -i nfs

umount /mnt/nfs
echo "Killing sleeper"
kill -9 $PID &>/dev/null || true
/bin/rm -f /tmp/sleeper.pid
sleep 0.1
echo OK
