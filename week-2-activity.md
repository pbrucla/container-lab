# Week 2 Activity: Creating your own Jail via the Command Line

In this introductory activity, we want to showcase what you can do with containers, just via the command line!
We hope that this activity will inspire you to work on a low-level container runtime (well, as far as we get) in C, which will take place over the rest of the quarter :)

## Access to Development Server

See [server-instructions.md](./server-instructions.md) for instructions on how to gain ssh access to the server.

## High-Level Overview
Our goal for this activity will be to create a jail that only allows you to access files in `~/jail`.

## Preparing the jail directory
To create our jail, we first need to create the root directory for the jail.

**Action Item**: Create the empty directory `~/jail`.
You'll know this succeeded if running the command `ls -l ~/jail` outputs `total 0`.
Then, populate the directory with a basic file structure by untaring the provided rootfs image `image.tar.gz` in `~/jail`.
You should see a `bin` directory with binaries of core command-line utilities from [BusyBox](https://busybox.net), including the shell (`bin/sh`).

Since we eventually want to use `~/jail` as our root mount, we have to make it a *mountpoint* (recall that this is one of the requirements for `pivot_root`).
This can be done by creating a *bind mount* from the jail onto itself.
To do this, we use the command `mount --bind [SOURCE] [DESTINATION]`, where in this case both the source and destination are `~/jail`.

**Action Item**: Run the command `mount --bind ~/jail ~/jail`.
You should see the error message `must be superuser to use mount`.

## Making new namespaces
We don't have permissions to mount directories!
It turns out we need the `CAP_SYS_ADMIN` capability in the current namespace to be able to mount.
First, let's inspect our current namespaces using the `/proc` directory.
The directory `/proc/self` contains a lot of information about the current process, including its namespaces!

**Action Item**: Run the command `ls -l /proc/self/ns`.
You should see many different namespaces, such as `cgroup -> 'cgroup:[4026531835]'` (your exact namespace ID will likely differ).
You can run the command `readlink /proc/self/ns/*` to get a cleaner output.
Note the output of this command down for later (specifically the `user` and `mnt` namespaces), as we'll reference them again soon.

To work around our lack of privileges, we need to create a new *user namespace* to make ourselves the superuser (`root`), and a new mount namespace to allow ourselves to create mounts.
Creating new namespaces can be done with the `unshare` command, which has a lot of different CLI options.
Read the manpage by running `man unshare`, or by [viewing an online version](https://man7.org/linux/man-pages/man1/unshare.1.html), and find CLI flags to do the following:
1. Create a new user namespace
2. Create a new mount namespace
3. Create a new PID namespace
4. Map the current user to the root user inside the new user namespace

**Action Item**: Run the `unshare` command with the proper flags.

> [!TIP]
> The `-f` (`--fork`) flag is needed for `unshare` to work properly.
> Try to run the command without `-f` and see what error you would encounter. Why does the error occur?
> Also, take a look at the `--propagation` flag of `unshare`. What is its default value and why does it matter?

If you run the `whoami` command afterwards, it should output `root`.
If you run `readlink /proc/self/ns/*`, you should see that the `mnt` and `user` namespace IDs are different from the values that you noted down before.

> [!NOTE]
> Technically, the flag for creating a new user namespace isn't needed, since the flag for mapping the root user implies it.

Now, we can actually make `~/jail` a mountpoint.

**Action Item**: Run `mount --bind ~/jail ~/jail` again.
There shouldn't be any error message this time.

Let's look at the mounts in the current mount namespace, which we can get from the `/proc/self/mountinfo` file.
If you run `grep jail /proc/self/mountinfo`, you should see a line with `/home/[USERNAME]/jail`.
Note that each line in the file identifies a mount.
The number in the first column is the mount ID and the second column is the ID of its parent mount.

**Optional**: Find the parent mount of the mount on `/home/[USERNAME]/jail` using these two IDs.

## Pivoting into the jail
We're now ready to pivot our root mount into the jail.
To do this, we'll use the `pivot_root [NEW_ROOT] [PUT_OLD]` command, which will change the root mount of the current mount namespace.
Note that `NEW_ROOT` and `PUT_OLD` are directory names.

**Action Item**: Make the `~/jail/old` directory and change directory into `~/jail`.
Pivot root into `.`, putting the old root in `./old`.
If you run `ls -l`, you should only see the directories that are previously in `~/jail` (plus `old`).

> [!IMPORTANT]
> If running `ls` after `pivot_root` causes a "command not found" error, run `export PATH=/bin` to fix your environment.

> [!NOTE]
> The exact way that the `pivot_root` command works is a bit nuanced.
> The `pivot_root` syscall changes the root mount of the calling process's mount namespace, which in this case is the `pivot_root` command.
> Historically, there is no guarantee that the root directory of other running processes in the mount namespace is changed, so the root directory of the shell might not be updated after `pivot_root` exits, necessitating the use of `chroot` below.
> The Linux implementation happens to update the root directory of other processes, and this behavior is now considered to be standardized.

**Action Item**: Run the command `exec chroot . sh` to replace your current shell instance with the one from the rootfs image (BusyBox), and make sure that its root directory is set to the new root.
If you run `ls /`, you should see the same directories from before, which means that we successfully restricted the accessible files in the jail by changing the root mount!

Currently the `/proc` directory is empty, so we have to mount an instance of the `proc` filesystem (`procfs`) on `/proc` for various system utilities like `ps` to work.

**Action Item**: Use the `mount` command with the appropriate flags to mount an instance of `procfs` on `/proc`.

After mounting `/proc`, run the command `ps aux` to list all processes, and there should only be two processes shown: the shell (`sh`) and the `ps` program itself.
This means that PIDs of processes outside of the jail's PID namespace are no longer visible :^)

## Unmounting the old directory
We are now ready to unmount the old root directory to complete the jail.

**Action Item**: Run the following commands to unmount the old root and remove the directory for it:
```sh
umount -l /old
rmdir /old
```
If you run `ls /`, you should no longer see the `/old` directory.
If you run `cat /proc/self/mountinfo` to list every mount, the list of mounts should be very short, and there shouldn't be any mounts on `/old`.

> [!NOTE]
> The `-l` flag to `umount` is a *lazy unmount*: if the mount is in use, it'll simply hide it from the filesystem and complete the unmount once it's not in use anymore.
> This is needed because there are submounts in the root mount which will make an eager unmount fail.

This completes the making of the jail!

## Acknowledgements

This activity was partially inspired (and expanded upon) by some of the challenges in [pwn.college](https://pwn.college)'s Sandboxing module in the System Security Dojo.
