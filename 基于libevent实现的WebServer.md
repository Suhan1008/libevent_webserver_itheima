基于libevent实现的简易WebServer

## 一、总体设计

### 1、web服务器的简易流程

web服务器<=>本地浏览器

（1）开发web服务器已经明确要使用http协议传送html文件

（2）开发协议选择是TCP+HTTP

（3）编写一个TCP并发服务器

（4）收发消息的格式采用的是HTTP协议

### 2、利用libevent_IO实现的流程

（1）创建根节点：event_base_new();

（2）创建套接字：socketaddr_in

（3）连接监听器：evconnlistener_new_bind();

（4）循环监听：event_base_dispatch();

（5）释放根节点：event_base_free();

（6）释放监听器：evconnlistener_free();

### 3、处理客户端请求流程

（1）获取请求行

（2）读缓冲区消息

（3）解析请求行信息

（4）判断文件/资源是否存在

​	存在：发送普通文件/目录？

​	不存在：404 NOT FOUND

## 二、具体实现

### 1、主函数编写

**（1）工作目录更改**

在C语言中，`getenv` 函数用于获取指定环境变量的值。该函数位于 `stdlib.h` 头文件中。

在C语言中，`chdir` 函数用于改变当前工作目录（Change Directory）。该函数位于 `unistd.h` 头文件中。

```c
char workdir[256] = {0};
strcpy(workdir,getenv("PWD"));
printf("%s\n",workdir);
chdir(workdir);
```

**（2）创建根节点**

创建结构体-创建套接字-IPv4 only-接受所有信息-绑定端口

`event_base` 是 Libevent 库中的一个结构体，用于管理事件和事件源。在 Libevent 中，`event_base` 结构体表示事件处理的上下文，包含了事件循环和与之关联的事件源。`event_base` 负责监视多个事件，当这些事件中的任何一个发生时，它将调用相应的回调函数。

```c
struct event_base* base=event_base_new();
struct sockaddr_in serv;//set sockaddr
serv.sin_family=AF_INET;//ipv4
serv.sin_addr.s_addr=htonl(INADDR_ANY);/*Address to accept any incoming messages.*/
serv.sin_port=htons(PORT);
```

**（3）创建监听器**

```c
// 创建一个 evconnlistener，用于监听连接事件
struct evconnlistener* listener = evconnlistener_new_bind
(
    base,                     // 事件基础（event_base）结构
    listen_cb,                // 当接受新连接时调用的回调函数
    base,                     // 传递给回调函数的上下文参数
    LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,  // 监听器的选项，包括在释放时关闭和地址重用
    -1,                       // 要监听的文件描述符（-1 表示自动分配）
    (struct sockaddr*)&serv,  // 表示要绑定的地址和端口的 sockaddr 结构体指针
    sizeof(serv)              // sockaddr 结构体的大小
);
```

解释：

- `base`：事件基础（`event_base`）结构，将管理这个监听器的事件。
- `listen_cb`：当接受新连接时将调用的回调函数。假定回调函数有以下原型：`void listen_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx)`。
- `base`：将传递给回调函数的参数。
- `LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE`：监听器的选项。`LEV_OPT_CLOSE_ON_FREE` 表示监听器在释放时关闭底层的套接字，`LEV_OPT_REUSEABLE` 允许地址重用。
- `-1`：要监听的文件描述符。在这里，使用 `-1` 表示自动分配一个文件描述符。
- `(struct sockaddr*)&serv`：指向表示要绑定的地址和端口的 `struct sockaddr` 结构体的指针。确保在使用之前正确初始化 `serv` 结构体，包含所需的地址和端口信息。
- `sizeof(serv)`：`sockaddr` 结构体的大小。

```c
// 循环监听基础事件
event_base_dispatch(base);

// 释放基础事件
event_base_free(base);

// 释放监听器
evconnlistener_free(listener);
```

### 2、回调函数实现

#### （1）listen_cb

```c
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
```

这段代码是 Libevent 库中用于处理监听器事件的回调函数 `listen_cb`。以下是对代码的中文总结：

1. **功能概述：**
   - 当有新的客户端连接时，此回调函数会被调用。
   - 在该回调函数中，创建了一个 `bufferevent` 对象，用于与客户端进行事件驱动的通信。

2. **代码解释：**
   - `listen_cb` 函数是一个回调函数，用于处理监听器事件。在这里，监听器负责接受新的客户端连接。
   - 通过参数 `evutil_socket_t fd` 获取到新连接的文件描述符。
   - 创建了一个 `bufferevent` 对象 `bev`，用于对该连接进行事件驱动的处理。
   - `bufferevent_setcb` 设置了事件回调函数，其中 `read_cb` 负责处理读事件，`bevent_cb` 负责处理其他事件，但没有设置写事件处理函数。
   - `bufferevent_enable` 启用了读和写事件，使得 `bufferevent` 对象可以监听和处理这两类事件。

3. **详细解释：**
   - `bufferevent_socket_new` 用于创建一个与套接字关联的 `bufferevent` 对象。这里传入的参数包括 `base`（事件循环基），`fd`（文件描述符），以及一些选项，其中 `BEV_OPT_CLOSE_ON_FREE` 表示在释放 `bufferevent` 时关闭底层的文件描述符。
   - `bufferevent_setcb` 用于设置事件回调函数。这里设置了读事件回调函数为 `read_cb`，写事件回调函数为 `NULL`（未设置），其他事件回调函数为 `bevent_cb`。
   - `bufferevent_enable` 启用了 `bufferevent` 对象的读和写事件，使其能够响应这两类事件。

4. **总结：**
   - `listen_cb` 函数是在接受到新的客户端连接时调用的，用于为每个连接创建并配置 `bufferevent` 对象，以进行后续的事件驱动通信。
   - 通过 Libevent 库提供的高层抽象，简化了与客户端的异步通信处理，同时具备高度的灵活性和效率。

请注意，此代码片段依赖于 Libevent 库，因此需要确保在项目中正确引入并初始化 Libevent。

#### （2）read_cb

```c
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
```

这段代码是 Libevent 库中用于处理读取事件的回调函数 `read_cb`。以下是对代码的中文总结：

1. **功能概述：**
   - 当 `bufferevent` 对象关联的套接字有数据可读时，该回调函数会被调用。
   - 在这里，函数负责读取客户端发来的数据，并处理客户端的 HTTP 请求。

2. **代码解释：**
   - `read_cb` 函数是一个回调函数，用于处理读取事件。当有数据可读时，该函数会被调用。
   - 使用 `bufferevent_read` 读取 `bufferevent` 中的数据，将其存储在 `buf` 中。
   - 使用 `sscanf` 从读取到的数据中解析出 HTTP 请求的方法、路径和协议版本。
   - 如果请求方法是 "GET"，则处理客户端的请求，调用 `http_request` 处理该请求。

3. **详细解释：**
   - `bufferevent_read` 用于从 `bufferevent` 中读取数据，将数据存储在 `buf` 中，返回读取的字节数。
   - 使用 `sscanf` 从读取到的数据中解析出 HTTP 请求的方法、路径和协议版本，存储在相应的变量中。
   - 如果请求方法是 "GET"，则继续处理客户端的请求。在这里，通过 `write(STDOUT_FILENO, buf, ret)` 将请求的第一行输出到标准输出。
   - 使用循环确保数据完全读取，继续读取数据并输出到标准输出。

4. **总结：**
   - `read_cb` 函数负责处理客户端发来的数据，特别是解析 HTTP 请求的方法、路径和协议版本。
   - 在该函数中调用了 `http_request` 函数，该函数用于处理客户端的 HTTP 请求。

请注意，此处的代码片段依赖于 Libevent 库，并且在处理 HTTP 请求时调用了 `http_request` 函数，但具体的 `http_request` 函数实现在提供的代码中并未提供。

#### （3）write_cb

```c
//write_cb do nothing
void write_cb(struct bufferevent *bev, void *user_data) 
{
    // Write operation completed
    printf("Write operation completed.\n");
}
```

这段代码是 Libevent 库中用于处理写入事件的回调函数 `write_cb`。以下是对代码的中文总结：

1. **功能概述：**
   - 当 `bufferevent` 对象关联的套接字可写时，该回调函数会被调用。
   - 在这里，函数仅仅输出一条消息表示写操作已完成。

2. **代码解释：**
   - `write_cb` 函数是一个回调函数，用于处理写入事件。当套接字可写时，该函数会被调用。
   - 在该函数中，输出一条消息到标准输出，表示写操作已经完成。

3. **详细解释：**
   - `write_cb` 函数用于处理写入事件，这里的实现非常简单，只是输出一条消息表示写操作已完成。
   - 通常情况下，该函数可以用于处理写入事件完成后的一些清理工作或其他逻辑。

4. **总结：**
   - `write_cb` 函数是在写入操作完成时被调用的回调函数。
   - 在该函数中，输出一条消息到标准输出，表示写入操作已完成。如果有其他需要在写入完成后执行的操作，可以在该函数中添加相应的逻辑。

请注意，具体应用中可能会根据需求在 `write_cb` 函数中添加更多的逻辑。

#### （4）bevent_cb

```c
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
```

这段代码是 Libevent 库中用于处理 `bufferevent` 事件的回调函数 `bevent_cb`。以下是对代码的中文总结：

1. **功能概述：**
   - 当与 `bufferevent` 关联的套接字发生特定事件时，该回调函数会被调用。
   - 在这里，函数处理了三种事件：客户端关闭连接、发生错误事件、连接成功事件。

2. **代码解释：**
   - `bevent_cb` 函数是一个回调函数，用于处理 `bufferevent` 的事件。当与 `bufferevent` 关联的套接字上发生特定事件时，该函数会被调用。
   - 使用了 `events` 参数来表示发生的事件，其中 `BEV_EVENT_EOF` 表示客户端关闭连接，`BEV_EVENT_ERROR` 表示发生错误事件，`BEV_EVENT_CONNECTED` 表示连接成功事件。
   - 根据不同的事件类型执行相应的操作：
     - 如果是客户端关闭事件，输出一条消息表示客户端已关闭，并释放 `bufferevent`。
     - 如果是错误事件，输出一条消息表示发生错误，并释放 `bufferevent`。
     - 如果是连接成功事件，输出一条消息表示客户端连接成功。

3. **详细解释：**
   - `bevent_cb` 函数用于处理 `bufferevent` 的事件回调，根据不同的事件类型执行相应的操作。
   - 当客户端关闭连接时，输出一条消息并释放 `bufferevent`。
   - 当发生错误事件时，输出一条消息并释放 `bufferevent`。
   - 当连接成功时，输出一条消息表示客户端连接成功。

4. **总结：**
   - `bevent_cb` 函数是 `bufferevent` 的事件回调函数，用于处理不同类型的事件。
   - 通过检查 `events` 参数，可以识别发生的事件类型，并在每种情况下执行相应的操作。

请注意，具体的应用中可能需要根据需求扩展该函数，添加其他逻辑以处理更复杂的场景。

### 3、处理客户端请求函数实现

#### （1）http_request

```c
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
```

这段代码是一个处理 HTTP 请求的函数 `http_request`，以下是对代码的中文总结：

1. **功能概述：**
   - `http_request` 函数用于处理 HTTP 请求，根据请求的路径信息判断是请求文件还是目录，并进行相应的处理。
   - 该函数会判断文件是否存在，如果存在，则根据文件类型发送相应的 HTTP 头部信息和内容。

2. **代码解释：**
   - `http_request` 函数接收两个参数，一个是指向 `bufferevent` 结构的指针 `bev`，另一个是表示请求路径的字符串 `path`。
   - 函数首先对路径进行解码，将中文字符转码成 UTF-8 格式的字符串。
   - 接着判断路径是否为根路径（"/"或"/."），如果是，则将路径设置为当前目录（"./"）。
   - 然后，判断请求的文件是否存在，如果不存在，发送 404 NOT FOUND 的响应，并返回错误码 -1。
   - 如果文件存在，通过 `stat` 函数获取文件的状态信息，包括文件类型等。
   - 如果是目录，发送相应的 HTTP 头部信息，然后调用 `send_dir` 函数发送目录内容。
   - 如果是普通文件，发送相应的 HTTP 头部信息，然后同样调用 `send_dir` 函数发送文件内容。

3. **详细解释：**
   - `http_request` 函数首先解码请求路径，然后判断路径是否为根路径，再通过 `stat` 函数获取文件信息。
   - 如果文件不存在，发送 404 NOT FOUND 的响应，如果存在，则根据文件类型发送相应的 HTTP 头部信息，并调用相应的函数发送内容。
   - 对于目录，调用 `send_dir` 函数发送目录内容；对于普通文件，同样调用 `send_dir` 函数发送文件内容。

4. **总结：**
   - `http_request` 函数是一个用于处理 HTTP 请求的函数，根据请求的路径和文件信息发送相应的 HTTP 响应。
   - 通过调用 `copy_header` 函数发送 HTTP 头部信息，然后根据文件类型调用相应的函数发送内容。

请注意，在实际应用中，可能需要根据具体需求对该函数进行扩展，添加更多的处理逻辑。

#### （2）copy_header

```c
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
```

这段代码是一个用于发送 HTTP 头部信息的函数 `copy_header`，以下是对代码的中文总结：

1. **功能概述：**
   - `copy_header` 函数用于构建并发送 HTTP 响应的头部信息。
   - 该函数根据传入的参数，包括响应状态码 (`op`)、状态消息 (`msg`)、文件类型 (`filetype`) 以及文件大小 (`filesize`)，生成相应的 HTTP 头部信息并发送。

2. **代码解释：**
   - `copy_header` 函数接收五个参数，一个是指向 `bufferevent` 结构的指针 `bev`，其余四个是整型 `op`、字符指针 `msg`、`filetype` 和 `filesize`。
   - 使用 `sprintf` 函数将状态行、Content-Type 和 Content-Length 信息格式化拼接到 `buf` 缓冲区中。
   - 如果 `filesize` 大于等于 0，也会将 Content-Length 加入到头部信息中。
   - 最后，通过 `strcat` 将空行添加到头部信息中，并使用 `bufferevent_write` 函数将头部信息发送给客户端。

3. **详细解释：**
   - `copy_header` 函数根据传入的参数构建 HTTP 头部信息，其中状态行包括协议版本和状态码，Content-Type 表示传输的文件类型，Content-Length 表示传输的文件大小。
   - 如果文件大小有效（大于等于 0），则将 Content-Length 加入到头部信息中。
   - 最终，通过 `bufferevent_write` 函数将构建好的头部信息发送给客户端。

4. **总结：**
   - `copy_header` 函数是一个用于发送 HTTP 头部信息的工具函数，简化了构建和发送头部信息的过程。
   - 通过传入不同的参数，可以灵活地生成不同状态码、消息、文件类型和文件大小的 HTTP 头部信息。

请注意，在实际应用中，可能需要根据具体需求对该函数进行扩展，以满足特定的 HTTP 响应头部的格式要求。

#### （3）send_dir

```c
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
```

这段代码是一个用于发送目录信息的函数 `send_dir`，以下是对代码的中文总结：

1. **功能概述：**
   - `send_dir` 函数用于组织一个 HTML 页面，包含指定目录的内容，并将该页面发送给客户端。
   - 在 HTML 页面中，目录的内容以列表形式显示，包括子目录和文件。

2. **代码解释：**
   - `send_dir` 函数接收两个参数，一个是指向 `bufferevent` 结构的指针 `bev`，另一个是指向表示目录路径的字符指针 `strPath`。
   - 使用 `copy_file` 函数将预定义的 HTML 页面的前缀内容发送给客户端。
   - 打开目录，遍历目录下的文件和子目录，对每个项目获取文件信息，包括文件名、文件类型、文件大小等。
   - 根据文件类型，生成 HTML 列表项的内容，并使用 `bufferevent_write` 函数将每个列表项发送给客户端。
   - 最后，使用 `copy_file` 函数将 HTML 页面的后缀内容发送给客户端。

3. **详细解释：**
   - `send_dir` 函数首先通过 `copy_file` 发送 HTML 页面的前缀，该页面可能包含样式和其他必要的头部信息。
   - 使用 `opendir` 打开指定的目录，如果打开失败则返回错误。
   - 遍历目录中的每个文件和子目录，使用 `readdir` 获取目录项信息。
   - 对于每个目录项，通过 `stat` 获取其详细信息，包括文件类型和大小。
   - 根据文件类型，生成 HTML 列表项的内容，包括超链接、文件名、文件大小等。
   - 使用 `bufferevent_write` 将每个列表项发送给客户端。
   - 遍历完所有目录项后，关闭目录，并使用 `copy_file` 发送 HTML 页面的后缀。

4. **总结：**
   - `send_dir` 函数是一个用于发送目录信息的工具函数，用于在客户端浏览器中显示指定目录的内容。
   - 通过组织 HTML 页面，以列表形式显示目录项，使用户能够直观地浏览目录结构。

请注意，在实际应用中，可能需要根据具体需求对该函数进行扩展，以满足特定的目录列表显示格式和样式要求。

#### （4）copy_file

```c
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
```

这段代码定义了一个 `copy_file` 函数，以下是对代码的中文总结：

1. **功能概述：**
   - `copy_file` 函数用于从指定文件中读取内容，并通过 `bufferevent_write` 将内容写入到与 `bufferevent` 相关联的缓冲事件中，从而发送给客户端。

2. **代码解释：**
   - `copy_file` 函数接收两个参数，一个是指向 `bufferevent` 结构的指针 `bev`，另一个是指向表示文件路径的字符指针 `strFile`。
   - 打开指定路径的文件，如果打开失败则返回错误。
   - 循环读取文件内容，每次读取一定大小的数据（1024 字节），然后使用 `bufferevent_write` 将读取的数据写入到缓冲事件中。
   - 循环直到文件全部读取完毕。
   - 关闭文件。

3. **详细解释：**
   - `copy_file` 函数首先通过 `open` 打开指定路径的文件，如果打开失败则输出错误信息。
   - 循环读取文件内容，每次读取一块数据（1024 字节），并通过 `bufferevent_write` 将读取的数据写入到缓冲事件中，从而发送给客户端。
   - 循环结束的条件是读取文件时遇到文件结束（`read` 返回 0）或者发生错误（`read` 返回 -1）。
   - 关闭文件，释放相关资源。

4. **总结：**
   - `copy_file` 函数是一个用于将文件内容发送给客户端的工具函数。
   - 通过逐块读取文件内容，并使用 `bufferevent_write` 将内容写入到缓冲事件中，实现了在事件驱动的网络编程模型中发送文件的功能。

请注意，在实际应用中，可能需要对文件读取的错误进行更详细的处理，并根据具体情况进行调整，以确保函数的稳健性和安全性。

### 4、自建库实现

#### pub.c

这段代码实现了一些用于处理HTTP请求的辅助函数，以下是对这些函数的中文总结：

##### 1. `get_mime_type` 函数：
- **功能概述：**
  - 根据文件名获取文件类型（MIME 类型）。

- **详细解释：**
  - 接受一个文件名字符串作为参数，通过查找文件名中的最后一个 '.' 来确定文件类型。
  - 根据文件类型返回相应的 MIME 类型字符串。
  - 如果无法识别文件类型，默认返回 "text/plain; charset=utf-8"。

##### 2. `get_line` 函数：
- **功能概述：**
  - 从 socket 中读取一行数据，每行以 `\r\n` 作为结束标记。

- **详细解释：**
  - 接受三个参数：socket 描述符、保存数据的缓冲区指针、缓冲区大小。
  - 通过 `recv` 函数逐字符读取数据，直到遇到 `\r\n` 为止，或者缓冲区已满，或者读取到错误。
  - 读取的数据存储在缓冲区中，并以 `\0` 结尾。
  - 返回实际存储的字节数（不包括 `\0`）。

##### 3. `strdecode` 函数：
- **功能概述：**
  - 对 URL 编码中的特殊字符进行解码。

- **详细解释：**
  - 接受两个参数：目标字符串指针 `to` 和源字符串指针 `from`。
  - 对 `from` 中的 URL 编码进行解码，将 `%20` 转换为对应的字符。
  - 解码后的内容存储在 `to` 中，并以 `\0` 结尾。

##### 4. `hexit` 函数：
- **功能概述：**
  - 将 16 进制字符转换为对应的 10 进制数字。

- **详细解释：**
  - 接受一个字符参数 `c`，判断该字符是否为 16 进制字符，如果是则将其转换为对应的 10 进制数字返回。

##### 5. `strencode` 函数：
- **功能概述：**
  - 对字符串进行编码，用于在回写浏览器时将非字母数字及一些特殊字符进行转义。

- **详细解释：**
  - 接受三个参数：目标字符串指针 `to`、目标字符串大小 `tosize` 和源字符串指针 `from`。
  - 将 `from` 中的非字母数字及一些特殊字符进行 URL 编码，结果存储在 `to` 中。
  - 返回编码后的字符串。

这些函数主要用于处理 HTTP 请求中的一些编解码操作，以及获取文件类型。在一个基于 C 的 HTTP 服务器中，这些函数可能会在处理请求、解析 URL 等方面发挥关键作用。

#### wrap.c

这段代码是一个简化版的网络编程封装，提供了一些常见的网络操作函数，用于创建、绑定、监听、接受连接等操作。以下是对其中一些函数的中文总结：

##### 1. `perr_exit` 函数：
- **功能概述：**
  - 打印错误信息并退出程序。

- **详细解释：**
  - 接受一个字符串 `s` 作为参数，使用 `perror` 打印错误信息，然后调用 `exit(-1)` 退出程序。

##### 2. 套接字操作函数：
- `Socket`: 创建套接字。
- `Bind`: 绑定套接字。
- `Connect`: 连接套接字。
- `Listen`: 监听套接字。
- `Accept`: 接受连接请求。

   - **详细解释：**
     - 这些函数是对相应的系统调用的封装，用于处理套接字的创建、绑定、连接和监听等操作。如果发生错误，这些函数会调用 `perr_exit` 函数打印错误信息并退出程序。

##### 3. I/O 操作函数：
- `Read`: 从文件描述符中读取数据。
- `Write`: 向文件描述符中写入数据。
- `Readn`: 从文件描述符中读取固定字节数的数据。
- `Writen`: 向文件描述符中写入固定字节数的数据。
- `Readline`: 从文件描述符中读取一行数据。

   - **详细解释：**
     - 这些函数用于进行 I/O 操作，包括读取和写入数据。`Readn` 和 `Writen` 函数用于读取和写入固定字节数的数据，`Readline` 用于读取一行数据。

##### 4. `my_read` 函数：
- **功能概述：**
  - 从文件描述符中读取一个字符。

- **详细解释：**
  - 该函数维护了一个静态变量 `read_buf` 作为缓冲区，用于存储从文件描述符中读取的数据。
  - 如果缓冲区中没有数据，调用系统调用 `read` 从文件描述符中读取数据到缓冲区。
  - 每次调用该函数，返回缓冲区中的一个字符，直到缓冲区中的数据被读取完。

##### 5. `tcp4bind` 函数：
- **功能概述：**
  - 创建并绑定一个 IPv4 TCP 套接字。

- **详细解释：**
  - 接受两个参数：端口号 `port` 和 IP 地址字符串 `IP`。
  - 创建一个 IPv4 TCP 套接字，并设置相关参数，如 IP 地址和端口号。
  - 如果 `IP` 为 `NULL`，则允许任意 IP 地址连接。
  - 返回创建的套接字文件描述符。

这些函数提供了对套接字编程中常见操作的封装，使网络编程更加方便。

## 三、总结

这段代码是一个使用 Libevent 库编写的简单的 Web 服务器。以下是对代码的主要功能和结构的总结：

1. **功能概述：**
   - 服务器监听端口 8080，并等待客户端连接。
   - 当有客户端连接时，创建一个 `bufferevent` 来处理与客户端的通信。
   - 服务器通过 HTTP 协议响应客户端请求，支持 GET 方法。
   - 可以处理文件请求和目录请求，发送相应的文件内容或目录列表。

2. **代码结构：**
   - `listen_cb` 函数是连接事件的回调函数，负责创建 `bufferevent` 并设置相应的回调函数。
   - `read_cb` 函数是读事件的回调函数，处理客户端发送的 HTTP 请求。
   - `write_cb` 函数是写事件的回调函数，目前为空，因为写操作在这里没有被使用。
   - `bevent_cb` 函数是 `bufferevent` 事件的回调函数，处理连接关闭等事件。

3. **HTTP 请求处理：**
   - 当收到 GET 请求时，通过 `http_request` 函数处理请求，判断请求的路径是文件还是目录，并相应地发送文件内容或目录列表。
   - 支持中文路径，并通过 `strdecode` 函数将中文路径转码成 UTF-8 格式的字符串。

4. **文件发送：**
   - `copy_header` 函数用于发送 HTTP 响应头部信息，包括状态码、文件类型、文件大小等。
   - `copy_file` 函数用于发送文件内容。

5. **事件循环：**
   - 使用 `event_base_dispatch` 进行事件循环，监听客户端连接和处理事件。

6. **资源释放：**
   - 在程序结束时，释放使用的 Libevent 相关资源，包括 `event_base` 和 `evconnlistener`。

请注意，代码中可能存在一些问题，比如路径拼接和错误处理可能需要更加健壮的实现。此外，对于实际生产环境，可能需要考虑更多的安全性和性能方面的问题。