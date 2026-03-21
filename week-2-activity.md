Steps

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