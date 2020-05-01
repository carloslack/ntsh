# Nothing to See Here - ntsh

    LKM that hides itself and system processes without relying on syscall hooking.

## Usage

    Commands are sent to /proc/ntsh as ROOT, exemple:
        echo hide >/proc/ntsh

    hide:  hide the module from lsmod/rmmod. Key to unhide is output from ring buffer
        echo hide >/proc/ntsh
        dmesg

    <key>: unhide the module
        echo "<random key from ring buffer>" >/proc/ntsh

    <PID>: hide/unhide process. Suppose there is a PID 28172
        hide:
            echo 28172 >/proc/ntsh
        unhide if the process is hidden:
            echo 28172 >/proc/ntsh

    list: list hidden processes via ring buffer
        echo list >/proc/ntsh
        dmesg

    The LKM can only be unloaded if it is not hidden


![](demo.gif)