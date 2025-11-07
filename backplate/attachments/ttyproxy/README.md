- Make the real kernel tty available as /dev/ttyO2.real.
-  Run ttyproxy which allocates a PTY and forwards bytes between that PTY master and /dev/ttyO2.real, logging hex traffic. 
 - ttyproxy prints the PTY slave (e.g. /dev/pts/2)
 - Bind-mount the PTY slave over /dev/ttyO2. nlclient opens /dev/ttyO2 as usual but now talks to your proxy.
 - When finished, unmount, stop proxy, restore original device node.

# Preparation

You must first have a root shell on the thermostat's Linux environment and know how to stop/start the Nest service (e.g. `/etc/init.d/nestlabs start|stop`)

----
## Send the Binary
On the build host, you must have netcat (`nc`) for transferring the ttyproxy binary
SSH to the Nest and start a simple server to receive the file:
`nc -l -p 51234 > /tmp/ttyproxy`

On the build host. send the cuckoo binary via netcat:
`cat ./ttyproxy | nc -w1 NEST_IP_ADDR 51234`

-----
## Make Executable
`chmod +x /tmp/ttyproxy`


# Step-by-step setup & run

Tip: run these as root. 
Run each command in order; copy/paste blocks as a whole so mount/unmount steps aren’t missed.

## 1. Inspect the real tty device major:minor

This identifies the kernel hardware node so we can create a stable name for it.
```
# on tstat (run as root)
cat /sys/class/tty/ttyO2/dev
# output example: 253:2
```
Note the MAJ:MIN numbers (use them in step 2 if needed).

## 2. Create /dev/ttyO2.real

```
# replace 253 2 with the values you saw above if different
mknod /dev/ttyO2.real c 253 2
chmod 660 /dev/ttyO2.real
chgrp dialout /dev/ttyO2.real 2>/dev/null || true
```

## 3. Stop the Nest client 

```
#(so it won't race for /dev/ttyO2)
/etc/init.d/nestlabs stop
```

## 4. Start the proxy and capture the printed PTY slave
 - Run ttyproxy in background or foreground. If you run in background; it prints `SLAVE=/dev/pts/X` and advice on stderr
 - if you want to read the SLAVE line in foreground, run without & then Ctrl-C after noting it
 - It prints SLAVE=/dev/pts/X. 
 - Save that path. Keep note of the /dev/pts/X printed — you need it next.

```
cd /tmp
./ttyproxy -r /dev/ttyO2.real -b 115200 -L /var/log/nl_intercept.log &

#SLAVE=/dev/pts/2
#[ttyproxy] Bind-mount onto /dev/ttyO2:  mount --bind /dev/pts/2 /dev/ttyO2
```

## 5. Create a target path for bind-mount and mount the PTY onto /dev/ttyO2

If /dev/ttyO2 doesn’t exist or is currently wrong, create a placeholder and bind:

```
#If /dev/ttyO2 was incorrectly mounted to another pts, unmount it first:
umount /dev/ttyO2 2>/dev/null || true

# use the slave path printed by ttyproxy (example /dev/pts/2)
SLAVE=/dev/pts/2

# make sure nlclient is stopped
pkill nlclient 2>/dev/null || true

# ensure /dev/ttyO2 exists as a file to bind onto
rm -f /dev/ttyO2
touch /dev/ttyO2

# bind the pts node to /dev/ttyO2
mount --bind "$SLAVE" /dev/ttyO2

# set perms so nlclient (or group dialout) can open the tty
chmod 660 "$SLAVE" /dev/ttyO2
chgrp dialout "$SLAVE" /dev/ttyO2 2>/dev/null || true

# sanity check
ls -l /dev/ttyO2 "$SLAVE"
mount | grep /dev/ttyO2 || true
```

## 6. Start nlclient 
it will now use the PTY which your proxy controls

```
# start the client as your system normally does
/etc/init.d/nestlabs start

# or systemctl start nlclient 2>/dev/null 
# or /nestlabs/sbin/nlclient -config /nestlabs/etc/client.config -config /nestlabs/etc/Display/Display-2/client.config &
```

## 7. Watch the log

The proxy logs to `/var/log/nl_intercept.log` by default. Tail it to see hex traffic:
```
tail -f /var/log/nl_intercept.log
```

You should see lines like:
```
2025-11-06 18:38:56 REAL->NLCLIENT d5d5aa96...
2025-11-06 18:39:00 NLCLIENT->REAL d5aa96...
```

# Clean up & restore system to normal (do this when done)

Follow these exact steps (order matters) to remove the MITM and restore `/dev/ttyO2` to the original hardware.

```
# 1. Stop nlclient and ttyproxy
pkill nlclient 2>/dev/null || /etc/init.d/nestlabs stop 2>/dev/null || true
pkill ttyproxy 2>/dev/null || true
sleep 0.2

# 2. Unmount the bind (free /dev/ttyO2)
umount /dev/ttyO2 2>/dev/null || true

# 3. Remove dangling path
rm -f /dev/ttyO2

# 4. Restore the real device node
# (only if /dev/ttyO2.real exists)
if [ -e /dev/ttyO2.real ]; then
  mv -f /dev/ttyO2.real /dev/ttyO2
  chmod 660 /dev/ttyO2
  chgrp dialout /dev/ttyO2 2>/dev/null || true
fi

# 5. Start the normal service
/etc/init.d/nestlabs start

If umount still says busy, find and kill the process holding /dev/ttyO2:

fuser /dev/ttyO2 2>/dev/null || lsof /dev/ttyO2 2>/dev/null
```
