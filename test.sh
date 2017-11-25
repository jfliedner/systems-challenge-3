umount mnt
make clean
make
rm -r mnt
mkdir mnt
gdb --args ./nufs -s -f mnt data.nufs
