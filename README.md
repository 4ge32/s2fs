# sifs - Simple filesystem on Linux

dd if=/dev/zero of=img bs=512 count=100

mount -o loop -t sifs img mnt
