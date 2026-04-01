# Week 2 Activity: Creating your own Jail via the Command Line

In this introductory activity, we want to showcase what you can do with regards to containerization, just via the command line!
We hope that this activity will inspire you to work on coding a low-level container runtime (well, as far as we get) in C, which will take place over the rest of the quarter :)

## Access to Development Server

See [server-instructions.md](./server-instructions.md) for instructions on how to gain ssh access to the server. 

## High-Level Overview
Our goal for this activity will be to create a jail that only allows you to access files in `~/jail`.

## Creating and mounting the jail directory
To create our jail, we first need to create the actual jail directory.

**Action Item**: Create the empty directory `~/jail`.
You'll know this succeeded if running the command `ls -l ~/jail` outputs `total 0`.

Since we eventually want to use `~/jail` as our root mount, we have to make it a mountpoint.
This can be done by creating a *bind mount* from the jail onto itself.
To do this, we use the command `mount --bind [SOURCE] [DESTINATION]`, where in this case both the source and destination are `~/jail`.

**Action Item**: Run the command `mount --bind ~/jail ~/jail`.
You should see the error message `must be superuser to use mount`.

## Making a new namespace
We don't have permissions to mount directories!
It turns out we need the `CAP_SYS_ADMIN` capability in the current namespace to be able to mount.
First, let's inspect our current namespaces using the `/proc` directory.
The directory `/proc/self` will contain a lot of information about the current process, including its namespaces!

**Action Item**: Run the command `ls -la /proc/self/ns`.
You should see many different namespaces, such as `cgroup -> 'cgroup:[4026531835]'` (your exact namespace ID will likely differ).
You can run the command `readlink /proc/self/ns/*` to get a cleaner output.
Note the output of this command down for later (specifically the `user` and `mnt` namespaces), as we'll reference them again soon.

To work around our lack of privileges, we need to create a new *user namespace* to make ourselves the superuser (`root`), and a new mount namespace to allow ourselves to create mounts.
Creating new namespaces can be done with the `unshare` command, which has a lot of different CLI options.
Read the manpage by running `man unshare`, or by [viewing an online version](https://man7.org/linux/man-pages/man1/unshare.1.html), and find CLI flags to do the following:
1. Create a new user namespace
3. Create a new mount namespace
2. Map the current user to the root user inside the user namespace

**Action Item**: Run the `unshare` command with the proper flags.
If you run the `whoami` command afterwards, it should output `root`.
If you run `readlink /proc/self/ns/*`, you should see that the `mnt` and `user` namespaces are different.

> ![NOTE]
> Technically, the flag for creating a new user namespace isn't needed, since the flag for mapping the root user implies it.

Now, we can actually make the jail a mountpoint.

**Action Item**: Run `mount --bind ~/jail ~/jail` again.
There shouldn't be any error message this time.
If you run `mount -l | grep jail` afterwards, you should see a mount on `/home/[USERNAME]/jail`.

## Populating the jail
We've got our jail directory, but it isn't usable yet!
The shell (`/bin/sh`) is an executable, but it's not accessible from within `~/jail`, so if we switched into the jail now, we wouldn't be able to run anything.
To ensure our jail is actually usable, we'll bind mount `/bin` (where the system's executables are stored) and `/lib` and `/lib64` (where the required libraries for the executables are stored) into the jail.

**Action Item**: Make the directories `~/jail/bin`, `~/jail/sbin`, `~/jail/lib`, `~/jail/lib64`, and `~/jail/proc`.
Then, bind mount `/bin`, `/sbin`, `/lib`, `/lib64`, and `/proc` into them.
Note: For `~/jail/proc`, you should use the flag `--rbind` instead of `--bind` to recursively bind the submounts (simply using `--bind` should cause an error message anyways).
If you run `mount -l | grep jail` afterwards, you should see all of the 6 bind mounts you've created (5 in this step and 1 from before).

## Pivoting into the jail
We're now ready to pivot our root mount into the jail.
To do this, we'll use the `pivot_root [NEW_ROOT] [PUT_OLD]` command.

**Action Item**: Make the `~/jail/old` directory.
Change directory into `~/jail`.
Pivot root into `.`, putting the old root in `./old`.
If you run `ls -l`, you should only see the 6 directories in `~/jail`.

> ![IMPORTANT]
> If `ls` throws an error about "command not found" or "error while loading shared libraries", run `export PATH=/bin:/sbin` and `export LD_LIBRARY_PATH=/lib64:/lib` to fix your environment.

Now, we complete the pivot with a chroot to update the root directory, since some `pivot_root` implementations don't do so.

**Action Item**: Run the command `exec chroot . sh` to replace your current shell with a chrooted one.
If you run `ls /`, you should see the same 6 directories from before.

## Unmount the old directory
We are now ready to unmount the old root directory to complete the jail.

**Action Item**: Run the commands:
```
umount -l /old
rmdir old
```
If you run `ls /`, you should no longer see the `/old` directory.
If you run `mount -l` to list every mount, the list of mounts should be very short, and there shouldn't be any mounts on `/old`.

> ![NOTE]
> The `-l` flag to `umount` is a *lazy unmount*: if the directory is in use, it'll simply hide it from the filesystem and complete the unmount once it's not in use anymore.

This completes the making of the jail!

## Realize that there is more work to be done!

If you run `ps aux`, you will be able to see the processes of the host.
This is because we didn't unshare the PID namespace for this jail.
There are many other namespaces we didn't unshare properly, like the network namespace, so there's a lot more to work on throughout this quarter!
Additionally, the lazy unmount of the old mount is theoretically not needed and is a hacky workaround, so there should be a way to get it to work properly.

## Acknowledgements

This activity was partially inspired (and expanded upon) by some of the challenges in [pwn.college](pwn.college)'s Sandboxing module in the System Security Dojo. 