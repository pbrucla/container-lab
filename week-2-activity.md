# Week 2 Activity: Creating your own Jail via the Command Line

In this introductory activity, we want to showcase what you can do with regards to containerization, just via the command line! You will go part-way through commentin. 

We hope that this activity will inspire you to work on coding a low-level container runtime (well, as far as we get) in C, which will take place over the rest of the quarter :)

## Access to Development Server

See [server-instructions.md](./server-instructions.md) for instructions on how to gain ssh access to the server. 

We will create and enter a jail in your home directory. 

## Unsharing your User and Mount Namespace

To see your current namespaces, do

```
ls -la /proc/self/ns
```

To unshare namespaces

```
unshare -U --map-root-user -m
```

## Create the jail directory (and make it a Mount Point )

```
mkdir -p ~/jail
cd ~
mount --bind jail jail
```

## Bind Mount /lib, /lib64 and /bin for Linker and Binaries

```
mkdir -p jail/bin jail/lib/x86_64-linux-gnu jail/lib64
mount --bind /bin jail/bin
mount --bind /lib/x86_64-linux-gnu jail/lib/x86_64-linux-gnu
mount --bind /lib64 jail/lib64
```

## Pivot Root

```
mkdir -p jail/old
pivot_root jail jail/old
```

## Change Directory into the Container / Jail

This is important otherwise your shell will still be able to traverse around the old directory structure!

```
cd /
hash -r
```

The second line is because bash will cache some of the results. 


## Mount Other Directories (e.g. /proc)

```
mkdir -p /proc /usr
mount --rbind /old/proc /proc
mount --rbind /old/usr /usr
```

## Unmount the Old Directory

```
umount -l /old
rmdir old
```

The -l flag indicates that the mount is **lazy** (TODO: elaborate). 

This completes the making of the jail!

## Realize that there is more work to be done!

If you run the following command

```
ps aux
```

the processes of the host will still show up! This is because we didn't unshare the PID namespace for this jail. 

## Acknowledgements

This activity was partially inspired (and expanded upon) by some of the challenges in [pwn.college](pwn.college)'s Sandboxing module in the System Security Dojo. 