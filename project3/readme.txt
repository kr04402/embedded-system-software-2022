driver name: /dev/stopwatch
major number: 242

order
(host)
0. kernel compile settings
1. make in app, module folder
2. adb push app data/local/tmp, adb push stopwatch.ko /data/local/tmp
(board)
3. insmod stopwatch.ko
4. mknod /dev/stopwatch c 242 0
5. ./app
6. rmmod stopwatch