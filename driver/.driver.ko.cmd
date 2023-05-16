cmd_/mnt/driver/driver.ko := ld -r -m elf_x86_64 -z noexecstack --build-id=sha1  -T scripts/module.lds -o /mnt/driver/driver.ko /mnt/driver/driver.o /mnt/driver/driver.mod.o;  true
