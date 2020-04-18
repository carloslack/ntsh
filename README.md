# Wally

    LKM that hides itself and system processes without relying on syscall hooking.

## Usage

    Commands are sent to /proc/wally as ROOT, exemple:
        echo hide >/proc/wally

    hide:  hide the module from lsmod/rmmod. Key to unhide is output from ring buffer
        echo hide >/proc/wally
        dmesg

    <key>: unhide the module
        echo "<random key from ring buffer>" >/proc/wally

    <PID>: hide/unhide process. Suppose there is a PID 28172
        hide:
            echo 28172 >/proc/wally
        unhide if the process is hidden:
            echo 28172 >/proc/wally

    list: list hidden processes via ring buffer
        echo list >/proc/wally
        dmesg

    The LKM can only be unloaded if it is not hidden


![](demo.gif)
