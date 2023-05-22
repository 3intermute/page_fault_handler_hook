cmd_/mnt/driver/pswap.ko := ld -r -m elf_x86_64 -z noexecstack --build-id=sha1  -T scripts/module.lds -o /mnt/driver/pswap.ko /mnt/driver/pswap.o /mnt/driver/pswap.mod.o;  true
