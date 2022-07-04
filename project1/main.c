#include <sys/types.h> 
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/mman.h> 
#include <signal.h> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <linux/input.h>

#define BUFF_SIZE 64
#define KEY_RELEASE 0
#define KEY_PRESS 1
#define FPGA_BASE_ADDRESS 0x08000000
#define LED_ADDR 0x16
#define MAX_BUTTON 9

void input_process(int );
void output_process(int );
void main_process(int );

int main(void)
{
    int* shmaddr; int shmid;
    shmid = shmget(IPC_PRIVATE,512, IPC_CREAT|0644);
    if(shmid == -1) { 
        perror("shmget"); 
        exit(1); 
    }  
    switch(fork()) { 
        case -1: 
            perror("first fork"); 
            exit(1); 
            break; 
        case 0: 
            input_process(shmid);
            break; 
        default: 
            switch(fork()){
                case -1:
                    perror("second fork"); 
                    exit(1); 
                    break;
                case 0:
                    output_process(shmid);
                    break;
                default:
                    main_process(shmid);
            }
    } 
    return 0;
}

void input_process(int shmid)
{
    int*shmaddr=(int*)shmat(shmid,(char*)NULL,0);
    memset(shmaddr,0,512);
    struct input_event ev[BUFF_SIZE];
    int fd,rd,value,size=sizeof(struct input_event);
    int fd2,rd2,value2,size2;    
    unsigned char push_sw_buff[MAX_BUTTON]; size2=sizeof(push_sw_buff);
    int mode=1; int switch_temp=0;

    char* device = "/dev/input/event0";
    if((fd = open (device, O_RDONLY|O_NONBLOCK)) == -1) {
    printf ("%s is not a vaild device\\n", device);
    }
    char* switchh = "/dev/fpga_push_switch";
    if((fd2 = open (switchh, O_RDWR)) == -1) {
    printf ("%s is not a vaild device\\n", switchh);
    }
    shmaddr[1]=1; /* initial mode=1 not needed due to line 69 but just in case */
    while(1)
    {
         usleep(200000);
        // switch read
        switch_temp=0;
        read(fd2,&push_sw_buff,size2);
        if(push_sw_buff[0]==1) switch_temp=1;
        else if(push_sw_buff[1]==1) 
        {
            if(push_sw_buff[2]==1)
                switch_temp=23;
            else
                switch_temp=2;
        }
        else if(push_sw_buff[2]==1) switch_temp=3;
        else if(push_sw_buff[3]==1) switch_temp=4;
        else if(push_sw_buff[4]==1)
        {
            if(push_sw_buff[5]==1)
                switch_temp=56;
            else
                switch_temp=5;
        } 
        else if(push_sw_buff[5]==1) switch_temp=6;
        else if(push_sw_buff[6]==1) switch_temp=7;
        else if(push_sw_buff[7]==1) 
        {
            if(push_sw_buff[8]==1)
                switch_temp=89;
            else
                switch_temp=8;
        }
        else if(push_sw_buff[8]==1) switch_temp=9;
        if(switch_temp==0){
        if((rd=read(fd,ev,size*BUFF_SIZE))==-1)
        {
            //printf("read error?\n");
            continue;
        }
        value=ev[0].value;
       
        if(value==KEY_PRESS)
        {
            mode=shmaddr[1]; 
            // back
            if(ev[0].code==158)     
            {
                shmaddr[1]=0;      /* shared memory info at the bottom of the file */
                exit(0);
            }
            // vol up
            else if(ev[0].code==115)    
            {
               shmaddr[0]=1;            /* clearflag */
                mode++; 
                if(mode==5) mode=1;
            }
            // vol down
            else if(ev[0].code==114)    
            {
               shmaddr[0]=1;
                mode--;
                if(mode==0) mode=4;
            }
            else ;  /* sth like prog */
        }
        }
	printf("mode:%d switch:%d\n",mode,switch_temp);     /* do not erase!! */
        
        shmaddr[1]=mode;
        shmaddr[2]=switch_temp;
        
    }
}

void output_process(int shmid)
{
    int*shmaddr=(int*)shmat(shmid,(char*)NULL,0);
    
    int fd_fnd,fd_lcd,fd_led,fd_dot;
    unsigned long* fpga_addr=0; unsigned char* led_addr=0;
    char fnd[4];
    unsigned char fpga_dot[2][10]={{0x0c,0x1c,0x1c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x1e},{0x1c,0x36,0x63,0x63,0x63,0x7F,0x7F,0x63,0x63,0x63}};    /* 1, A*/

    // device open
    fd_led=open("/dev/mem",O_RDWR | O_SYNC);
    if(fd_led<0){
        perror("/dev/mem open error");
        exit(1);
    }
    fpga_addr=(unsigned long*)mmap(NULL,4096,PROT_READ | PROT_WRITE, MAP_SHARED,fd_led,FPGA_BASE_ADDRESS);
    if(fpga_addr==MAP_FAILED)
    {
        printf("mmap error!\n");
        close(fd_led);
        exit(1);
    }
    led_addr=(unsigned char*)((void*)fpga_addr+LED_ADDR);

    fd_lcd=open("/dev/fpga_text_lcd",O_WRONLY);
    if(fd_lcd<0){
        perror("lcd open error");
        exit(1);
    }

    fd_fnd=open("/dev/fpga_fnd",O_WRONLY);
    if(fd_fnd<0){
        perror("lcd open error");
        exit(1);
    }

    fd_dot=open("/dev/fpga_dot",O_WRONLY);
    if(fd_dot<0){
        perror("lcd open error");
        exit(1);
    }
    
    while(1)
    {
        if(shmaddr[1]==0)
        {
            // device all off
            memset(fnd,0,sizeof(fnd));
            write(fd_fnd,fnd,sizeof(fnd));
            char str[33]; unsigned char dot_erase[10]; memset(dot_erase,0x00,sizeof(dot_erase));
            memset(str,' ',sizeof(str));
            str[32]=0;
            write(fd_lcd,str,32);
            *led_addr=0;
            write(fd_dot,dot_erase,sizeof(dot_erase));
            close(fd_led); close(fd_lcd); close(fd_fnd); close(fd_dot);
            munmap(led_addr,4096);
            exit(0);
        }
        else if(shmaddr[1]==1)
        {
            // fnd
            int hour=shmaddr[3],min=shmaddr[4];
            fnd[0]=hour/10; fnd[1]=hour%10; fnd[2]=min/10; fnd[3]=min%10;
            write(fd_fnd,fnd,sizeof(fnd));
            // led
            int ledd=shmaddr[10];
            usleep(450000);
            *led_addr=(char)ledd;
            // initialize others
            char str[33]; unsigned char dot_erase[10]; memset(dot_erase,0x00,sizeof(dot_erase));
            memset(str,' ',sizeof(str));
            str[32]=0;
            write(fd_lcd,str,32);
            write(fd_dot,dot_erase,sizeof(dot_erase));
        }
        else if(shmaddr[1]==2)
        {
            int counter_type=shmaddr[10],res=shmaddr[3];
            int hund,ten,one,led;
            res=res%(counter_type*counter_type*counter_type);
            hund=res/(counter_type*counter_type); res=res%(counter_type*counter_type);
            ten=res/counter_type; 
            one=res%counter_type;
            fnd[0]=0; fnd[1]=hund; fnd[2]=ten; fnd[3]=one;
            write(fd_fnd,fnd,sizeof(fnd));
            if(counter_type==10) led=64;
            else if(counter_type==8) led=32;
            else if(counter_type==4) led=16;
            else if(counter_type=2) led=128;
            *led_addr=(char)led;
            // initialize others
            char str[33]; unsigned char dot_erase[10]; memset(dot_erase,0x00,sizeof(dot_erase));
            memset(str,' ',sizeof(str));
            str[32]=0;
            write(fd_lcd,str,32);
            write(fd_dot,dot_erase,sizeof(dot_erase));
        }
        else if(shmaddr[1]==3)
        {
            int count=shmaddr[6]; int type=shmaddr[5];
            char str[33]; 
            int i;
            for(i=0;i<32;i++) str[i]=shmaddr[30+i]; 
            str[32]=0;
            
            count=count%10000;
            fnd[0]=count/1000; count=count%1000;
            fnd[1]=count/100; count=count%100;
            fnd[2]=count/10; fnd[3]=count%10;
            write(fd_fnd,fnd,sizeof(fnd));

            write(fd_lcd,str,32);

            int size1=sizeof(fpga_dot[0]),size2=sizeof(fpga_dot[1]);
            if(type==0)
                write(fd_dot,fpga_dot[1],size2);
            else
                write(fd_dot,fpga_dot[0],size1);
            *led_addr=0;
        }
        else if(shmaddr[1]==4)
        {
            int count=shmaddr[13],i;
            count=count%10000;
            fnd[0]=count/1000; count=count%1000;
            fnd[1]=count/100; count=count%100;
            fnd[2]=count/10; fnd[3]=count%10;
            write(fd_fnd,fnd,sizeof(fnd));

            unsigned char dot_print[10];
            for(i=0;i<10;i++) dot_print[i]=shmaddr[70+i];
            write(fd_dot,dot_print,sizeof(dot_print));

            char str[33];
            memset(str,' ',sizeof(str));
            str[32]=0;
            write(fd_lcd,str,32);
            *led_addr=0;
        }
        else ;
    }
    /*
    shmdt((int*)shmaddr); 
    shmctl(shmid, IPC_RMID, (struct shmid_ds *)NULL);
    */
}

void main_process(int shmid)
{
    int*shmaddr=(int*)shmat(shmid,(char*)NULL,0);
    // mode1
    time_t original_time=time(NULL); time_t temp;
    shmaddr[101]=0; shmaddr[0]=1; shmaddr[6]=0;
    int timeflag=0,timeflag2=0;
    // mode2
    int counter_type=10,counter_res=0;
    // mode3
    char str[32];
    char arr[10][3]={{'@','@','@'},{'.','Q','Z'},{'A','B','C'},{'D','E','F'},{'G','H','I'},{'J','K','L'},{'M','N','O'},{'P','R','S'},{'T','U','V'},{'W','X','Y'}};
    int idx=0;
    // mode4
    char arr2[10];
    int board[10][7]; 
    typedef struct _cursor{
        int x;
        int y;
    } Cursor;
    Cursor cur;
    while(1)
    {
        int switch_num;
        // printf("mode:%d switch:%d shmaddr[0]:%d\n",shmaddr[1],shmaddr[2],shmaddr[0]);
        // back 
         usleep(200000);
        if(shmaddr[1]==0)
        {
            shmaddr[1]=0;
            exit(0);
        }
        // mode 1
        else if(shmaddr[1]==1)
        {
            switch_num=shmaddr[2];
            printf("changemode?: %d switchnum:%d\n",shmaddr[101],switch_num);   /* do not erase !! */
            if(shmaddr[0]==1)   /* initialize if another mode was changed to mode 1 */
            {
                shmaddr[101]=0;
                temp=original_time;
                shmaddr[0]=0;
            }
            if(switch_num==1)
            {
                if(shmaddr[101]==0) /* make it changeable */
                {
                    shmaddr[101]=1;
                }
                else    /* save mode */
                {
                    shmaddr[101]=0;
		            original_time=temp;
                }
            }
            else if(switch_num==2)
            {
                if(shmaddr[101]==1)
                    temp=time(NULL);        /* reset to current board time */     /* needs to press switch 1 again to save it */
            }
            else if(switch_num==3)
            {
                if(shmaddr[101]==1)
                {
                    temp+=3600;
                }
            }
            else if(switch_num==4)
            {
                if(shmaddr[101]==1)
                {
                    temp+=60;
                }
            }
            else ;
            if(shmaddr[101]==0){
            struct tm* t=localtime(&temp);
            //printf("hour:%d min:%d\n",t->tm_hour,t->tm_min);
            shmaddr[3]=t->tm_hour; shmaddr[4]=t->tm_min;
            shmaddr[10]=128;
            }
            else    /* needs the led 3,4 to go on and off */
            {
                if(timeflag==0)
                {
                    shmaddr[10]=32;
                    timeflag=1;
                }
                else
                {
                    shmaddr[10]=16;
                    timeflag=0;
                }
            }
        }
        // mode 2
        else if(shmaddr[1]==2)
        {
            switch_num=shmaddr[2];
            if(shmaddr[0]==1)   /* initialize if another mode was changed to mode 2 */
            {
                counter_res=0;
                shmaddr[0]=0;
                counter_type=10;
            }
            if(switch_num==1)
            {
                if(counter_type==10) counter_type=8;
                else if(counter_type==8) counter_type=4;
                else if(counter_type==4) counter_type=2;
                else if(counter_type==2) counter_type=10;
            }
            else if(switch_num==2)
            {
                counter_res+=(counter_type*counter_type);
            }
            else if(switch_num==3)
            {
                counter_res+=counter_type;
            }
            else if(switch_num==4)
            {
                counter_res+=1;
            }
            else ;

            shmaddr[3]=counter_res;
            shmaddr[10]=counter_type;
        }
        // mode 3
        else if(shmaddr[1]==3)
        {
            switch_num=shmaddr[2];
            int type=shmaddr[5];   /* type0 is english, type1 is number */
            int same_press=shmaddr[7];  
            int prev_press=shmaddr[8];

            if(shmaddr[0]==1)   /* initialize if another mode was changed to mode 3 */
            {
                memset(str,' ',sizeof(str)); 
                shmaddr[0]=0;
                shmaddr[6]=0;
                type=0;
                prev_press=0;   /* previously pressed number*/
                same_press=0;   /* how many same press being made <=2 */
                idx=0;          
            }
            (shmaddr[6])++;     /* counting the switch */
            if(switch_num==23)
            {
                memset(str,' ',sizeof(str)); 
                idx=0;
            }
            else if(switch_num==56)
            {
                type=1-type;
            }
            else if(switch_num==89)
            {
                str[idx++]=' ';
            }
            else if(switch_num>0)
            {
                if(type==0){    
                // should be changed if pressed the same one
                if((switch_num==prev_press) && (idx>0))
                {
                    same_press++;
                    str[idx-1]=arr[switch_num][same_press%3];
                }
                else
                {
                    if(idx==32) /* needs to slide */
                    {
                        int i;
                        char temp[32];
                        for(i=0;i<31;i++) temp[i]=str[i+1];
                        for(i=0;i<31;i++) str[i]=temp[i];  
                        idx--;
                    }
                    same_press=0;
                    if(same_press==0)
                        str[idx++]=arr[switch_num][same_press];
                }
                }
                else    /* number just add to the end */
                {
                    if(idx==32) /* needs to slide */
                    {
                        int i;
                        char temp[32];
                        for(i=0;i<31;i++) temp[i]=str[i+1];
                        for(i=0;i<31;i++) str[i]=temp[i];  
                        idx--;
                    }
                    str[idx++]='0'+switch_num;
                }
                shmaddr[8]=switch_num;  /* previous button save */
            }
            else (shmaddr[6])--;    /* if switch==-1 should decrease again */
            
            shmaddr[5]=type;
            int i;
            for(i=0;i<32;i++)
            {
                shmaddr[30+i]=str[i];
            }
        }
        // mode 4
        else if(shmaddr[1]==4)
        {
            switch_num=shmaddr[2];
            int cur_print_flag=shmaddr[102];    /* whether cursor should be printed */
            if(shmaddr[0]==1)   /* initialize if another mode was changed to mode 4 */
            {
                int i,j;
                shmaddr[0]=0;
                cur.x=0; cur.y=0;
                for(i=0;i<10;i++){
                    for(j=0;j<7;j++)
                        board[i][j]=0;
                }
                memset(arr2,0,sizeof(arr2));
                shmaddr[13]=0;
                cur_print_flag=1;
            }
            (shmaddr[13])++;    /* switch count(same routine as mode3) */
            if(switch_num==2)
            {
                board[cur.x][cur.y]=shmaddr[11];    /* restore cursor data */
                (cur.x)--;
                if(cur.x==-1)
                    cur.x=9;
            }
            else if(switch_num==4)
            {
                board[cur.x][cur.y]=shmaddr[11];    /* restore cursor data */
                (cur.y)--;
                if(cur.y==-1)
                    cur.y=6;
            }
            else if(switch_num==6)
            {
                board[cur.x][cur.y]=shmaddr[11];    /* restore cursor data */
                (cur.y)++;
                if(cur.y==7)
                    cur.y=0;
            }
            else if(switch_num==8)
            {
                board[cur.x][cur.y]=shmaddr[11];    /* restore cursor data */
                (cur.x)++;
                if(cur.x==10)
                    cur.x=0;
            }
            else if(switch_num==1)
            {
                int i,j;
                for(i=0;i<10;i++){
                    for(j=0;j<7;j++)
                        board[i][j]=0;
                }
                memset(arr2,0,sizeof(arr2));
                cur.x=0; cur.y=0;
                // not sure if to reset the fnd count
                shmaddr[13]=0;
                shmaddr[102]=1;
            }
            else if(switch_num==3)
            {
                if(cur_print_flag==0)
                    cur_print_flag=1;
                else
                    cur_print_flag=0;
            }
            else if(switch_num==5)
            {
                board[cur.x][cur.y]=1;
            }
            else if(switch_num==7)
            {
                int i,j;
                for(i=0;i<10;i++){
                    for(j=0;j<7;j++)
                        board[i][j]=0;
                }
                memset(arr2,0,sizeof(arr2));
            }
            else if(switch_num==9)
            {
                int i,j;
                for(i=0;i<10;i++){
                    for(j=0;j<7;j++)
                    {
                        if(board[i][j]==0)
                            board[i][j]=1;
                        else
                            board[i][j]=0;
                    }
                }
            }
            else
            {
                (shmaddr[13])--;
            }
            if(switch_num>0) shmaddr[11]=board[cur.x][cur.y];   /* save it(data erased due to cursor light going on and off) */
            // cursor blinking implementation
            if(cur_print_flag==1)
            {
                    if(timeflag2==0)
                    {
                        board[cur.x][cur.y]=1;
                        timeflag2=1;
                    }
                    else
                    {
                        board[cur.x][cur.y]=0;
                        timeflag2=0;
                    }
            }
            
            shmaddr[102]=cur_print_flag;
            // dot matrix copy to shmaddr
            int m;
            for(m=0;m<10;m++)
            {
                char temp[8];
                sprintf(temp,"%d%d%d%d%d%d%d",board[m][0],board[m][1],board[m][2],board[m][3],board[m][4],board[m][5],board[m][6]);
                arr2[m]=strtol(temp,0,2);
            }
            for(m=0;m<10;m++)
            {
                shmaddr[m+70]=arr2[m];
            }
        }
        else
        {
            
        }
        shmaddr[2]=-1;
        usleep(300000);
    }
}

/* shmaddr
0-clearflag due to mode change   1-mode   2-switch input
3-fnd time(hour)    4-fnd time(min)
5-english or number flag    6-switch count in mode3     7-same press number     8-prev button
10-led
11- leftover cursor flag    13-switch count in mode4
30~61: mode3 string
70~79: mode4 dot matrix
101- mode1 continue flag
102- mode4 continue flag
*/
