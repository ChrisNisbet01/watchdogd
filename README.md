# watchdogd

## A simple watchdog implementation for use on linux.

This application perdiocially runs 'test' executables in the specified directory (default "/etc/watchdog.d") and services the watchdog device ("/dev/watchdog") if all executables return an exit code of success (0).
It was created as an alternative to other watchdog implementations used on some distributions, which do no functionality tests, and simply stroke the watchdog periodically and provided no way to add tests to confirm the device is working as intended.
This seemed a little useless, so this application was born.

### Notes
- When determining the time to perform the next test tun the time taken to execute all of the 'test' 
applications is subtracted from the keepalive time, so if the keepalive time is 15 seconds and the previous
test run took 5 seconds, the next run will take place in 10 seconds time. 
- If a test run takes more than the keepalive time, the next run takes place immediately.
- If there are no tests in the specified test directory (or the directory doesn't exist), the application simply strokes the watchdog every keepalive seconds.
- Basic OpenWRT Makefile have been included to assist with use on OpenWRT. No support for automatically starting/running the application has been provided. Note that procd handles the watchdog on OpenWRT.

Usage:
```
# watchdogd -h
watchdogd -e <test_directory. [-w <sec>] [-k <sec>] [-d] [-h]
A basic watchdog daemon that strokes the watchdog after ensuring
applications in the test directory all return success (0).
Options:
	-e <test_directory> - the directory tests are found in (default: "/etc/watchdog.d")
	-w <secs>           - set the watchdog counter to <sec> seconds (default: 90 secs)
	-k <secs>           - set the keepalive time to <sec> seconds (default: 42 secs)
	-d                  - disable the watchdog when this program exits
	-h                  - write this help message and exit
```

### Required libraries
- [OpenWRT libubox](git://git.openwrt.org/project/libubox.git)
- [ubus_utils](https://github.com/ChrisNisbet01/libubus_utils) library

### Making the application
- mkdir build
- cd build
- cmake ..
- make
