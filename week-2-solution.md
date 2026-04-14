# Solution to the Week 2 Activity

```sh
# Populate jail directory
mkdir jail
cd jail
wget https://github.com/pbrucla/container-lab/raw/refs/heads/main/image.tar.gz
tar -xzf image.tar.gz
rm image.tar.gz

# Unshare the user, PID, mount namespaces (and map current user to root in new user namespace)
unshare --map-root-user -Umpf /bin/bash

# Make the jail directory a mount point
mount --bind ~/jail ~/jail

# Pivot root
mkdir old
pivot_root ~/jail ~/jail/old
exec chroot . sh

# Mount procfs
mount -t proc none /proc

# Unmount old root directory
umount -l /old
rmdir /old
```