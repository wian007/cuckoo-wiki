[After having gained root access to the Nest and SSHing into it ](https://github.com/cuckoo-nest/cuckoo_loader), understanding how the Linux-powered display unit contacts and interacts with the backplate was paramount. 

Nest's thermostat control runs through the `nlclient` service.

The faceplate is mounted as a device in Linux at `/dev/ttyO2` - that's a letter 'O' and not a zero.

We originally tried the steps described at https://web.archive.org/web/20141023183423/http://wiki.gtvhacker.com/index.php/Nest_Hacking#Backplate_initialisation to intercept the communication from `nlclient` to the backplate. 

if `strace` had been installed on the Nest, then backplate communications can be decoded from a (filtered) strace log using [NestDecode.pl](https://web.archive.org/web/20141023183423/http://luke.dashjr.org/programs/freeabode/kindling/NestDecode.pl), or decoded in real time using [nest-intercept.c](https://web.archive.org/web/20141023183423/http://luke.dashjr.org/programs/freeabode/kindling/nest-intercept.c). 

The original `nest-intercept.c` tool, which used `ptrace`, did not work, leading to a deeper investigation.

The core of the problem is a logical contradiction that we were unable to solve:

1.  **Fact**: A high-volume, continuous stream of data is available from `/dev/ttyO2`. We proved this by building a simple C program (`cuckoo_capture`) that opens the device and successfully reads and decodes the protocol data in real-time.

2.  **Fact**: The main `nlclient` process (PID `2797`) holds this device open on file descriptor `61`. We proved this by inspecting the `/proc/2797/fd/` directory.

3.  **Fact**: No observation method could detect the I/O operation.
    - **`ptrace` Failure**: A `ptrace`-based tool, modeled after `nest-intercept.c`, was attached to the main PID and every single one of its threads. It was configured to watch for `read()`, `write()`, and `ioctl()` syscalls. It saw no activity on `fd 61`.
    - **`/proc` Polling Failure**: A second tool (`cuckoo_proc_monitor`) was built to poll the `/proc/2797/task/*/syscall` file for every thread at maximum speed (a busy-loop). This tool also saw no thread entering the `read()` syscall on `fd 61`, proving the syscalls were not just fast, but were simply not being reported by the kernel's process status files.
    - **Memory Map (`mmap`) Failure**: We hypothesized that the process was using memory-mapped I/O. However, an inspection of `/proc/2797/maps` showed no mapping for `/dev/ttyO2`, proving this theory incorrect.
    - **`LD_PRELOAD` Failure**: Our final attempt was to inject a shared library to hook the `read()` function directly, bypassing `ptrace`. This is a technique hinted at in the `nest-intercept.c` source code. This failed with an `internal error` from the system's dynamic linker, even when using a minimalist library and placing it in a trusted directory (`/nestlabs/lib/`). 


If `nlclient` is running, for example if `/etc/init.d/nestlabs start` was run to start the service, then `nlclient` talks directly with the backplate. 

When nlclient is already running then we can steal the device tty and listen, but if we stop nlclient then we can't get the backplate to do more than send its serial and firmware version. 

Doing `cat /dev/ttyO2` will steal control of the backplate and you'll see the messages that would have been being sent to nlclient, such as printing the firmware version and serial number, but telling it to start sending the sensor data back with nlclient stopped has not yet worked, and while stealing then after a minute then nlclient will get mad and change the display to say that the backplate isn't connected, even though it is.

That let us get to where we can decode what the backplate is sending, and by reading 
[https://wiki.exploitee.rs/index.php/Nest_Hacking](https://wiki.exploitee.rs/index.php/Nest_Hacking "https://wiki.exploitee.rs/index.php/Nest_Hacking") we saw a described startup procedure which didn't work to get the right initialization commands to get the backplate started. 

Since we know that the backplate mounts at `/dev/ttyO2`, the next adventure was finding a way to intercept what nlclient is sending back and forth to /dev/ttyO2, such as socat and tee?
The goal was to get an "interceptor TTY" to pretend to be the  /dev/ttyO2. Ideally, we would tell Linux to move the real device to another/dev mount point and then mount something in its place

# TTYproxy

[jfarre20](https://github.com/jfarre20) developed a C application called `ttyproxy` that redirects tty input and output, that we could mount at the known dev location. The source code, instructions, and a binary are in [attachments/ttyproxy](attachments/ttyproxy). 

The observations of the protocol are in [Protocol](Protocol.md)





