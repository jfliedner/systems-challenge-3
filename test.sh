make clean
make
mkdir mnt
gdb --args ./nufs -s -f mnt data.nufs
