# sifs - Simple filesystem on Linux

dd if=/dev/zero of=img bs=4096 count=100
make
insmod sifs.ko
mount -o loop -t sifs img mnt

rmmod sifs.ko
