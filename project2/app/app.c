#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define DEV_MAJOR 242 
#define DEV_MINOR 0
#define DEV_NAME "/dev/dev_driver"
#define SET_OPTION _IOW(DEV_MAJOR,1,int*)
#define COMMAND _IOW(DEV_MAJOR,2,int*)


int main(int argc, char **argv)
{
    int i;
	int dev;

    // user input exceptions
    // ./app TIMER_INTERVAL[1-100] TIMER_CNT[1-100] TIMER_INIT[0001-8000]
	if(argc!=4) {
		printf("Invalid Value Arguments! needs 3 arguments\n");
		return -1;
	}		
	int interval=atoi(argv[1]);
    if(interval<1 || interval>100)
    {
        printf("Invalid Value Arguments! TIMER_INTERVAL[1-100]\n");
        return -1;
    }
    int count=atoi(argv[2]);
    if(count<1 || count>100)
    {
        printf("Invalid Value Arguments! TIMER_CNT[1-100]\n");
        return -1;
    }
    int init,initsize=strlen(argv[3]),flag=0;
    for(i=0;i<initsize;i++)
    {
        if('0'<argv[3][i] && argv[3][i]<='8')
            flag++;
        else if(argv[3][i]>'8')
        {
            flag=-1000;
            break;
        }
        else
            continue;
    }
	if(initsize!=4 || flag!=1)
    {
        printf("Invalid Value Arguments! TIMER_INIT[0001-8000]\n");
        return -1;
    }
    init=atoi(argv[3]);
    // open
	dev = open(DEV_NAME, O_WRONLY);
	if (dev<0) {
		printf("Device open error : %s\n",DEV_NAME);
		return -1;
	}

    // user app is writing so _IOW used
    ioctl(dev,SET_OPTION,&init);       /* pass initialize value */
    int pass[2]={interval,count};
	ioctl(dev,COMMAND,pass);           /* pass others */
	close(dev);
	
	return 0;
}
