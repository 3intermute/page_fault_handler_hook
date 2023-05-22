cmd_/mnt/driver/pswap.mod := printf '%s\n'   pswap.o | awk '!x[$$0]++ { print("/mnt/driver/"$$0) }' > /mnt/driver/pswap.mod
