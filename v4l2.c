#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <stdlib.h>
#include <sys/mman.h>
struct camera_mode
{
    int index;
    unsigned char description[32];
    int width;
    int high;
    int fps;
    int mode;
    struct camera_mode* next;
};
struct list_head
{
    struct camera_mode* head;
    struct camera_mode* tail;
};
struct list_head Head;
static int mode_max=0;
void camera_list_add(int index,unsigned char description[32],int width,int high,int fps)
{
    
    struct camera_mode *p=malloc(sizeof(struct camera_mode));
    p->index=index;strcpy(p->description,description);p->next=NULL;p->width=width;p->high=high;p->fps=fps;p->mode=mode_max;mode_max++;
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
        printf("mode=%d %s size(%d,%d) %ddps mode:%d\n",p->index,p->description,p->width,p->high,p->fps,p->mode);
        if(p->next==NULL){return;}
        p=p->next;
    }
}

void mode_check(int fd)
{
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
}
__uint8_t *buf[4]={NULL};
size_t size[4]={0};
void read_init(int fd)
{
    struct  v4l2_requestbuffers a={0};
    a.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    a.count=4;
    a.memory=V4L2_MEMORY_MMAP;
    int res=ioctl(fd,VIDIOC_REQBUFS,&a);
    if(res<0){perror("read_init fail");}
    struct v4l2_buffer b={0};
    b.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for(int i=0;i<4;i++)
    {
        b.index=i;
        res=ioctl(fd,VIDIOC_QUERYBUF,&b);
        if(res<0){perror("read int error");return;}
        buf[i]=mmap(NULL,b.length,PROT_READ|PROT_WRITE,MAP_SHARED,fd,b.m.offset);
        size[i]=b.length;
        res=ioctl(fd,VIDIOC_QBUF,&b);
        if(res<0){perror("read init error");return;}
    } 
    if(res<0){perror("read init error");return ;}
    printf("read init success\n");
    int c=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    res=ioctl(fd,VIDIOC_STREAMON,&c);
}
int lcd_w,lcd_h;
__uint8_t* buf_lcd;
int lcd_bpp;
void write_init(int fd2)
{
    struct fb_var_screeninfo a;
    int res=ioctl(fd2,FBIOGET_VSCREENINFO,&a);
    lcd_w=a.xres;
    lcd_h=a.yres;
    lcd_bpp=a.bits_per_pixel;
    printf("lcd_w:%d lcd_h:%d\n",lcd_w,lcd_h);
    buf_lcd=mmap(NULL,lcd_w*lcd_h*(lcd_bpp/8),PROT_READ|PROT_WRITE,MAP_SHARED,fd2,0);
    
}
struct camera_mode* cur_mode=NULL;
void set_camera_mode(int fd)
{
    int mode;
    cur_mode=Head.head;
    while(1)
    {
        printf("please input mode:");
        scanf("%d",&mode);
        if(mode<=mode_max){break;}
        printf("mode set error,");
    }
    while(cur_mode->mode!=mode){cur_mode=cur_mode->next;}
    struct v4l2_format fmt={0};
    fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width=cur_mode->width; 
    fmt.fmt.pix.height=cur_mode->high;
    // fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_MJPEG;
    // int res=ioctl(fd,VIDIOC_S_FMT,&fmt);
    // if(res<0){perror("ioctl");return;}
    // if(fmt.fmt.pix.pixelformat!=V4L2_PIX_FMT_MJPEG)
    // {
    //     printf("set fail,it not set pixelformat to V4L2_PIX_FMT_MHPEG\n");return;
    // }
    fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;
    int res=ioctl(fd,VIDIOC_S_FMT,&fmt);
    if(res<0){perror("ioctl");return;}
    if(fmt.fmt.pix.pixelformat!=V4L2_PIX_FMT_YUYV)
    {
        printf("set fail,it not set pixelformat to V4L2_PIX_FMT_YUYV\n");return;
    }
    printf("mode set success\n");
    read_init(fd);
}
int cur_index;
void yuyv_rgb()
{
    if(buf_lcd == NULL) return;
    if(cur_mode == NULL) return;
    int src_w = cur_mode->width;
    int src_h = cur_mode->high;
    if(src_w <= 0 || src_h <= 0) return;
    int x, y;
    for(y=0;y<lcd_h;y++)
    {
        int src_y=y*src_h/lcd_h;
        __uint8_t *line = buf[cur_index] + src_y * src_w * 2;
        for(x=0;x<lcd_w;x+=2)
        {
            int src_x = (x * src_w / lcd_w) & ~1;

            int y0=line[src_x*2+0];
            int u=line[src_x*2+1]-128;
            int y1=line[src_x*2+2];
            int v=line[src_x*2+3]-128;

            int r0=y0+((1436*v)>>10);
            int g0=y0-((352*u+731*v)>>10);
            int b0=y0+((1815*u)>>10);
            int r1=y1+((1436*v)>>10);
            int g1=y1-((352*u+731*v)>>10);
            int b1=y1+((1815*u)>>10);

            if(r0<0)r0=0;if(r0>255)r0=255;
            if(g0<0)g0=0;if(g0>255)g0=255;
            if(b0<0)b0=0;if(b0>255)b0=255;
            if(r1<0)r1=0;if(r1>255)r1=255;
            if(g1<0)g1=0;if(g1>255)g1=255;
            if(b1<0)b1=0;if(b1>255)b1=255;

            if(lcd_bpp==16)
            {
                __uint16_t *dst=(__uint16_t*)buf_lcd+y*lcd_w+x;
                dst[0]=((r0&0xF8)<<8)|((g0&0xFC)<<3)|(b0>>3);
                if(x+1<lcd_w)
                dst[1] = ((r1 & 0xF8) << 8) | ((g1 & 0xFC) << 3) | (b1 >> 3);
            }
            else if(lcd_bpp==32)
            {
                __uint32_t *dst=(__uint32_t*)buf_lcd+y*lcd_w+x;
                dst[0]=0xFF000000|(r0<<16)|(g0<<8)|b0;
                if(x+1<lcd_w)
                dst[1]=0xFF000000|(r1<<16)|(g1 << 8)|b1;
            }
        }
    }
}
void save_ppm()
{
    if(!buf[cur_index] || !cur_mode) return;

    FILE* fp = fopen("frame.ppm", "wb");
    if(!fp){perror("fopen"); return;}

    int out_w = lcd_w;    // 1280
    int out_h = lcd_h;    // 800
    int src_w = cur_mode->width;
    int src_h = cur_mode->high;

    fprintf(fp, "P6\n%d %d\n255\n", out_w, out_h);

    for(int y = 0; y < out_h; y++)
    {
        int src_y = y * src_h / out_h;
        __uint8_t* line = buf[cur_index] + src_y * src_w * 2;

        for(int x=0;x<out_w;x++)
        {
            int src_x=(x*src_w/out_w)&~1;
            int y0=line[src_x*2+0];
            int u=line[src_x*2+1]-128;
            int v=line[src_x*2+3]-128;

            int r=y0+((1436*v)>>10);
            int g=y0-((352*u+731*v)>>10);
            int b=y0+((1815*u)>>10);

            if(r<0)r=0;if(r>255)r=255;
            if(g<0)g=0;if(g > 255)g=255;
            if(b<0)b=0;if(b> 255)b=255;

            fputc(r,fp);
            fputc(g,fp);
            fputc(b,fp);
        }
    }
    fclose(fp);
    printf("saved frame.ppm\n");
}
int my_read(int fd)
{
    struct v4l2_buffer b={0};
    b.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int res=ioctl(fd,VIDIOC_DQBUF,&b);
    if(res<0){perror("read error");return -1;}
    // FILE* file=fopen("test.jpg","w+");
    // buf[b.index];
    // fwrite(buf[b.index],b.length,1,file);
    
    cur_index=b.index;
    yuyv_rgb();
    res=ioctl(fd,VIDIOC_QBUF,&b);
    if(res<0){perror("read error");return -1;}
}

void exe_end(int fd,int fd2)
{
    int type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd,VIDIOC_STREAMOFF, &type);
    for(int i=0;i<4;i++)
    {
        munmap(buf[i],size[i]);
    }
    close(fd);
    close(fd2);
}
int main(int argc, char* argv[])
{
    if(argc!=2){printf("use error:./... path\n");return 0;}
    int fd=open(argv[1],O_RDWR);
    int fd2=open("/dev/fb0",O_RDWR);
    if(fd==-1){perror("open");return 0;}
    if(fd2==-1){perror("open lcd");return 0;}
    write_init(fd2);
    mode_check(fd);
    set_camera_mode(fd);
    while(1)
    my_read(fd);

    exe_end(fd,fd2);
    
}
