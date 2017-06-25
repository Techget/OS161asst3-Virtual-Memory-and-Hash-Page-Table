# OS161asst3-Virtual-Memory-and-Hash-Page-Table
You will need to increase the amount of physical memory to run some of the provided tests. Update ~/cs3231/root/sys161.conf so that the ramsize is as follows
31	mainboard  ramsize=16777216  cpus=1

configure your new sources, and build and install the user-level libraries and binaries.
% cd ~/cs3231/asst3-src
% ./configure
% bmake
% bmake install

You have to reconfigure your kernel before you can use the framework provided to do this assignment. The procedure for configuring a kernel is the same as before, except you will use the ASST3 configuration file:
% cd ~/cs3231/asst3-src/kern/conf	
% ./config ASST3

compile
% cd ../compile/ASST3
% bmake depend
% bmake
% bmake install

Test command, run test on OS161.
```
OS/161 kernel [? for menu]: km1
Starting kmalloc test...
kmalloc test done
Operation took 0.185939280 seconds
OS/161 kernel [? for menu]:

OS/161 kernel [? for menu]: km2
Starting kmalloc stress test...
kmalloc stress test done
Operation took 0.947937200 seconds
OS/161 kernel [? for menu]:

OS/161 kernel [? for menu]: km3 200000
kmalloctest3: 200000 objects, 782 pointer blocks
kmalloctest3: 11080000 bytes allocated
kmalloctest3: passed
Operation took 234.073916000 seconds
OS/161 kernel [? for menu]:

OS/161$ /testbin/huge

OS/161$ /testbin/faulter

OS/161$ /testbin/triplehuge

OS/161$ /testbin/crash

OS/161$ /testbin/parallelvm
```
