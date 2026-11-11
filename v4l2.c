#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <stdlib.h>
struct camera_mode
{
    int index;
    unsigned char description[32];
    int width;
    int high;
    int fps;
    struct camera_mode* next;
};
struct list_head
{
    struct camera_mode* head;
    struct camera_mode* tail;
};
struct list_head Head;
void camera_list_add(int index,unsigned char description[32],int width,int high,int fps)
{
    struct camera_mode *p=malloc(sizeof(struct camera_mode));
    p->index=index;strcpy(p->description,description);p->next=NULL;p->width=width;p->high=high;p->fps=fps;
    if(Head.head==NULL){Head.head=p,Head.tail=p;return;}
    Head.tail->next=p;Head.tail=p;
    return;
}
void camera_list_destory()
{
    if(Head.head==NULL){return;}
    struct camera_mode* p;
    while(1)
    {
        p=Head.head;
        if(Head.head->next==NULL){free(p);Head.head=NULL;Head.tail=NULL;return;}
        Head.head=Head.head->next;
        free(p);
        
        
    }
}
void look_camera_list_data()
{
    if(Head.head==NULL){return;}
    struct camera_mode* p=Head.head;
    while(1)
    {
        printf("mode=%d %s size(%d,%d) %ddps\n",p->index,p->description,p->width,p->high,p->fps);
        if(p->next==NULL){return;}
        p=p->next;
    }
}
void mode_check(char* path)
{
    int fd=open(path,O_RDWR);
    if(fd==-1){perror("open");return;}
    int j=0;
    while(1)
    {
        struct v4l2_fmtdesc a={0};
        struct v4l2_frmsizeenum b={0};
        struct v4l2_frmivalenum c={0};
        a.index=j;
        a.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        int res=ioctl(fd,VIDIOC_ENUM_FMT,&a);
        if(res<0){if(errno==EINVAL)break;perror("ioctl");break;}
        b.pixel_format=a.pixelformat;
        c.pixel_format=a.pixelformat;
        int m=0;
        while(1)
        {
            b.index=m;
            int res=ioctl(fd,VIDIOC_ENUM_FRAMESIZES,&b);
            if(res<0){if(errno==EINVAL)break;perror("ioctl");break;}
            int n=0;
            c.width  = b.discrete.width;
            c.height = b.discrete.height;
                while(1)
                {
                    c.index = n;
                    int res=ioctl(fd,VIDIOC_ENUM_FRAMEINTERVALS,&c);
                    if(res<0){if(errno==EINVAL)break;perror("ioctl");break;}
                    camera_list_add(a.index,a.description,b.discrete.width,b.discrete.height,c.discrete.denominator);
                    n++;
                }
            m++;
        }
        // printf("index=%d\n",a.index);
        // printf("flags=%d\n",a.flags);
        // char *p=&a.pixelformat;
        // for(int i=0;i<4;i++)
        // printf("pixelformat[%d]=%c\n",i,p[i]);
        // printf("reserved=%d\n",a.reserved[0]);
        
        j++;
    }
    look_camera_list_data();
    camera_list_destory();
    close(fd);  
}
int main(int argc, char* argv[])
{
    if(argc!=2){printf("use error:./... path\n");return 0;}
    mode_check(argv[1]);
}
