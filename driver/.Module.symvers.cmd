cmd_/mnt/driver/Module.symvers := sed 's/ko$$/o/' /mnt/driver/modules.order | scripts/mod/modpost -m -a  -o /mnt/driver/Module.symvers -e -i Module.symvers   -T -
