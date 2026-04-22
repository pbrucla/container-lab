# Week 4 activity: cgroup

In this week's activity we will implement cgroup support for our container runtime.
We will build upon the code from last week and implement some additional functions, mainly `init_cgroup_parent` and `init_cgroup_child`.
Most of the information can be found in the slides, but below we will introduce two helper programs that are useful when completing this week's activity.

## `delegate-cgroup`

Typically, the cgroup v2 hierarchy that is mounted on a system can only be modified by a privileged user, so our unprivileged container runtime cannot make any changes to it directly.
We need to invoke a helper program called `delegate-cgroup` that creates a cgroup and delegates it to our main program, which allows us to manipulate a subtree in the hierarchy.
This program is installed on the server and available in the `PATH`.
It is a setuid executable and owned by root, which means that it will run as a privileged process (with effective user ID == 0) even if the process that spawned it is unprivileged.

It creates a sub-cgroup under the root cgroup and delegates it to the unprivileged user by changing the ownership of certain files and directories in the hierarchy.
It also moves its parent process (which is assumed to be our main program) to the delegated cgroup.
Additionally, it prints the path of the delegated cgroup to its standard output, which we will need when initializing cgroups from the main program.
You can use the `popen` function to run `delegate-cgroup` as a child process and obtain a file stream from which you can read its output.
Note that since `popen` runs the provided command in a shell, it is necessary to run the program using `exec delegate-cgroup` so that the parent process of `delegate-cgroup` is actually our process instead of the shell.

## `test-limits`

This program can be run **inside** the jail to test if the cgroup resource limits are setup correctly.
You should compile it with `make build/test-limits` and place the binary in your jail directory.
Note that since our jail currently does not support dynamically-linked binaries, this program is linked statically with [musl libc](https://musl.libc.org/).

It performs the following tests:
* Verifying that the cgroup namespace is unshared correctly by checking the contents of `/proc/self/cgroup`.
The test fails if the current cgroup is not the root cgroup of the cgroup namespace.
* Trying to spawn 40 child processes using `fork`.
The test fails if more than 20 processes can be successfully spawned.
* Trying to allocate 40 MB of memory using `malloc`.
The test fails if the allocations succeed without invoking the OOM killer (receiving `SIGKILL`).
* Putting the program into a busy loop for 1 second.
The test fails if the CPU time is more than 40% of the elapsed wall time (400 ms).
