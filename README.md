# Basic compiling stuff 

(refer to http://cgi.cse.unsw.edu.au/~cs3231/17s1/assignments/asst3/)

Building a Kernel

    You first have to configure your source tree. This step sets up some information for later compilation, such as the location of various directories, the compiler to use, and compilation flags to supply.

    % cd ~/cs3231/asst0-src
    % ./configure

    Now you must configure the kernel itself. This step prepares for a compilation of the specific set of files that make up the warmup kernel.

    % cd ~/cs3231/asst0-src/kern/conf
    % ./config ASST0

    The next task is to build the kernel. Note the bmake depend step. This step works out which include files are required by which source files so the source files can be recompiled if the include files change. Remember to "bmake depend" whenever building a new kernel or when you have added or removed include files. If you're not sure, just run it!

    % cd ../compile/ASST0
    % bmake depend
    % bmake

    Now install the kernel.

    % bmake install

    In addition to the kernel, you have to build the user-level utilities (they will be installed automatically):

    % cd ~/cs3231/asst0-src
    % bmake

Running your Kernel
If you have made it this far, your have built and installed the entire OS. Now it is time to run it.

    Download the sample sys161-asst0.conf and install it as ~/cs3231/root/sys161.conf.
    Change to the root directory of your OS.

    % cd ~/cs3231/root

    Now run system/161 (the machine simulator) on your kernel.

    % sys161 kernel

    Power off the machine by typing q at the menu prompt. 

Using GDB

Note: the version of GDB used for these assignments is os161-gdb.
Modifying your Kernel
We will now go through the steps required to modify and rebuild your kernel. We will add a new file to the sources. The file contains a function we will call from existing code. We need to add the file to the kernel configuration, re-config the kernel, and the rebuild again.

    Begin by downloading hello.c and place it in kern/main/.
    Find an appropriate place the in the kernel code, and add a call to complex_hello() (defined in hello.c) to print out a greeting (Hint: main.c in kern/main/ is very appropriate). It should appear immediately before the prompt.
    Since we added new file to the kernel code, we need to add it to the kernel configuration in order to build it. Edit kern/conf/conf.kern appropriately to include hello.c. Again, look for main.c as a hint as to syntax.
    When we change the kernel config, we need to re-configure the kernel again.

    % cd ~/cs3231/asst0-src/kern/conf
    % ./config ASST0

    Now we can rebuild the kernel.

    % cd ../compile/ASST0
    % bmake depend
    % bmake
    % bmake install

    Run your kernel as before. Note that the kernel will panic with an error message.
    Use GDB to find the bug (Hint: the display, break, and step commands will be very useful).
    Edit the file containing the bug, recompile as before and re-run to see the welcome message. 

Note: If you simply modify a file in an already configed source tree, you can simply run bmake again to rebuild, followed by bmake install. You only need to reconfigure it if you add or remove a file from the config, and you only need to bmake depend if you add (or modify) a #include directive. 




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
