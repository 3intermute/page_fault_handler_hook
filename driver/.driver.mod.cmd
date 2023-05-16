cmd_/mnt/driver/driver.mod := printf '%s\n'   driver.o | awk '!x[$$0]++ { print("/mnt/driver/"$$0) }' > /mnt/driver/driver.mod
