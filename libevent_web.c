//通过libevent编写的web服务器
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "pub.h"
#include <event.h>
#include <event2/listener.h>
#include <dirent.h>
#define PORT 8080
#define _WORK_DIR_ "%s/webpath"
#define _DIR_PREFIX_FILE_ "html/dir_header.html"
#define _DIR_TAIL_FILE_ "html/dir_tail.html"
int copy_header(struct bufferevent *bev,int op,char *msg,char *filetype,long filesize)
{
    //发送报文 文件类型 文件大小 空行 文件
    char buf[4096]="";
    sprintf(buf,"HTTP/1.1 %d %s\r\n",op,msg);
    sprintf(buf,"%sContent-Type: %s\r\n",buf,filetype);
    if(filesize>=0)
    {
        sprintf(buf,"%sContent-Length:%ld\r\n",buf,filesize);
    }
    strcat(buf,"\r\n");
    bufferevent_write(bev,buf,strlen(buf));
    return 0;
}
//发送目录，实际上组织一个html页面发给客户端，目录的内容作为列表显示
int send_dir(struct bufferevent *bev,const char *strPath)
{
    //需要拼出来一个html页面发送给客户端
    copy_file(bev,_DIR_PREFIX_FILE_);
    //send dir info 
    DIR *dir = opendir(strPath);
    if(dir == NULL)
    {
        perror("opendir err");
        return -1;
    }
    char bufline[1024]={0};
    struct dirent* dent=NULL;
    while(dent=readdir(dir))
    {
        struct stat sb;
        //int stat(const char *pathname, struct stat *statbuf);
        //pathname 是文件的路径名。
        //statbuf 是一个 struct stat 结构，用于存储文件信息。
        //用于获取文件的信息，如文件大小、创建时间、修改时间等。
        stat(dent->d_name,&sb);
        if(dent->d_name==DT_DIR)
        {
            //目录文件 特殊处理
            //格式 <a href="dirname/">dirname</a><p>size</p><p>time</p></br>
            memset(bufline,0x00,sizeof(bufline));
            sprintf(bufline,"<li><a href='%s/'>%32s</a> %8ld</li>",dent->d_name,dent->d_name,sb.st_size);
            bufferevent_write(bev,bufline,strlen(bufline));
        }
        else if(dent->d_name==DT_REG)
        {
            //普通文件 直接显示列表即可
            memset(bufline,0x00,sizeof(bufline));
            sprintf(bufline,"<li><a href='%s'>%32s</a>     %8ld</li>",dent->d_name,dent->d_name,sb.st_size);
            bufferevent_write(bev,bufline,strlen(bufline));
        }
    }
    //close dictionary
    closedir(dir);
    copy_file(bev,_DIR_TAIL_FILE_);
    return 0;
}
int copy_file(struct bufferevent *bev,const char *strFile)
{
    int fd=open(strFile,O_RDONLY);
    char buf[1024]={0};
    int ret;
    while((ret=read(fd,buf,sizeof(buf)))>0)
    {
        bufferevent_write(bev,buf,ret);
    }
    close(fd);
    return 0;
}
int http_request(struct bufferevent* bev,char* path)
{
    //将中文问题转码成utf-8格式的字符串
    strdecode(path,path);//from path to path
    //将传来的path存储在另一个地址数组中
    char* strPath=path;
    if(strcmp(strPath,"/")==0||strcmp(strPath,"/.") == 0)
    {
        strPath="./";
    }
    else
    {
        //[GET] [/%E8%8B%A6%E7%93%9C.txt] [HTTP/1.1]
        strPath=path+1;
    }
    struct stat sb;
    //判断请求的文件在不在
    if(stat(strPath,&sb)<0)
    {
        //not found 404
        copy_header(bev,404,"NOT FOUND",get_mime_type("error.html"),-1);
        copy_file(bev,"error.html");
        return -1;
    }
    if(S_ISDIR(sb.st_mode))
    {
        //dictionary
        copy_header(bev,200,"OK",get_mime_type("ww.html"),sb.st_size);
        send_dir(bev,strPath);
    }
    if(S_ISREG(sb.st_mode))
    {
        //regular file
        copy_header(bev,200,"OK",get_mime_time(strPath),sb.st_size);
        send_dir(bev,strPath);
    }
    return 0;
}
//listener_callback
void listen_cb(struct evconnlistener *listener, evutil_socket_t fd,struct sockaddr *sa, int socklen, void *arg) 
{
    printf("Accepted a connection.\n");
    //define bufferevent to connect with client
    struct event_base* base=(struct event_base*)arg;
    /** If set, we close the underlying file
	 * descriptor/bufferevent/whatever when this bufferevent is freed. */
    struct bufferevent* bev=bufferevent_socket_new(base,fd,BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev,read_cb,NULL,bevent_cb,base);//not set write_cb -> read only
    bufferevent_enable(bev,EV_READ|EV_WRITE);//启用读和写
}
//read_callback
void read_cb(struct bufferevent *bev, void *ctx)
{
    char buf[256]={0};
    //报文组合
    char method[10],path[256],protocol[10];
    //read buf
    int ret=bufferevent_read(bev,buf,sizeof(buf));
    if(ret<=0)
    {
        perror("");
        return;
    }
    else
    {
        //scanf buf in a certain way method path protocol
        sscanf(buf,"%[^ ] %[^ ] %[^ \r\n]",method,path,protocol);
        /*strcasecmp 返回一个整数，
        表示比较结果。返回值为0表示两个字符串相等，返回值小于0表示 s1 小于 s2，
        返回值大于0表示 s1 大于 s2。*/
        if(strcasecmp(method,"get")==0)//method[]="get"
        {
            //处理客户端的请求
            char bufLine[256];
            write(STDOUT_FILENO,buf,ret);
            //make sure data reading over
            while(ret = bufferevent_read(bev, bufLine, sizeof(bufLine))>0)
            {
                write(STDOUT_FILENO,bufLine,ret);
            }
            http_request(bev,path);
        }
    }
}
//write_cb do nothing
void write_cb(struct bufferevent *bev, void *user_data) 
{
    // Write operation completed
    printf("Write operation completed.\n");
}
//bufferevent_callback
void bevent_cb(struct bufferevent *bev, short events, void *ctx)
{
    if(events & BEV_EVENT_EOF)
    {//客户端关闭
        printf("client closed\n");
        bufferevent_free(bev);
    }
    else if(events & BEV_EVENT_ERROR)
    {//error event
        printf("err to client closed\n");
        bufferevent_free(bev);
    }
    else if(events & BEV_EVENT_CONNECTED)
    {//连接成功
        printf("client connect ok\n");
    }
}
int main(int argc,char *argv[])
{
    //change work dictionary
    char workdir[256]="";
    /*在类Unix/Linux系统中，PWD 是一个环境变量，
    代表当前工作目录（Present Working Directory）。你可以使用 getenv 函数来获取它的值。*/
    strcpy(workdir,getenv("PWD"));
    printf("%s\n",workdir);
    chdir(workdir);//change dictionary
    //set root addr and details
    struct event_base* base=event_base_new();
    struct sockaddr_in serv;//set sockaddr
    serv.sin_family=AF_INET;//ipv4
    serv.sin_addr.s_addr=htonl(INADDR_ANY);/*Address to accept any incoming messages.*/
    serv.sin_port=htons(PORT);
    //create listener
    /*base：event_base 对象，表示事件的事件循环基。
    cb：连接事件的回调函数。
    ptr：传递给回调函数的指针。
    flags：一些标志位，用于控制监听器的行为。
    backlog：传递给 listen 函数的连接队列的大小。
    sa：struct sockaddr 结构体，表示要绑定的地址信息。
    socklen：sa 结构体的长度。*/
    struct evconnlistener* listener=
        evconnlistener_new_bind(base,listen_cb,base,
            LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,
                (struct sockaddr*)&serv,sizeof(serv));
    //loop listening base_root
    event_base_dispatch(base);
    //release root
    event_base_free(base);
    //release listener
    evconnlistener_free(listener);
    return 0;
}