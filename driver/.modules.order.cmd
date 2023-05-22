cmd_/mnt/driver/modules.order := {   echo /mnt/driver/pswap.ko; :; } | awk '!x[$$0]++' - > /mnt/driver/modules.order
