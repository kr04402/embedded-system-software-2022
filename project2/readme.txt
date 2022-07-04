20161564 WOOCHEOLKWAK
driver name: /dev/dev_driver
major number: 242
order
(host)
0. kernel compile settings
1. make in app, module folder
2. adb push app data/local/tmp, adb push module.ko data/local/tmp
(board)
3. insmod module.ko
4. mknod /dev/dev_driver c 242 0
5. ./app TIMER_INTERVAL[1-100] TIMER_CNT[1-100] TIMER_INIT[0001-8000]
6. rmmod module