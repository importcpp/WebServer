#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h> // contains the system call(alarm)
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <vector>
#include <vector>
#include "hpserver/ThreadPool.h"
#include "hpserver/lst_timer.h"
#include <iostream>


#define FD_LIMIT 65535 // 文件描述符数量限制
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5
#define NUMBER_TIME 8
#define USER_LIMIT // 最大用户数量

static int pipefd[2];
static sort_time_lst timer_lst;
static int epollfd = 0;

static std::vector<int> active_user;

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 边缘触发方式一定要采用非阻塞io
int addfd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//  
void addsig(int sig){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void timer_handler(){
    // 定时处理任务，实际上就是调用tick函数
    timer_lst.tick();
    // 因为一次alarm调用只会引起一次SIGALRM信号，所以我们需要重新定时，以不断触发SIGALRM信号
    alarm(TIMESLOT);
}

// 定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之
void cb_func(client_data* user_data){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d\n", user_data->sockfd);
}

ThreadPool pool(10);
std::vector< std::future<int> > results;

int main(int argc, char * argv[])
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    
    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    ret = listen( listenfd, 5 );
    assert( ret != -1 );

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    ret = socketpair( PF_UNIX, SOCK_STREAM, 0, pipefd );
    assert( ret != -1 );
    setnonblocking( pipefd[1] );
    addfd( epollfd, pipefd[0] );

    // add all the interesting signals here
    addsig( SIGALRM );
    addsig( SIGTERM );
    bool stop_server = false;

    client_data* users = new client_data[FD_LIMIT]; 
    bool timeout = false;
    alarm( TIMESLOT );

    while( !stop_server )
    {
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        if ( ( number < 0 ) && ( errno != EINTR ) )
        {
            printf( "epoll failure\n" );
            break;
        }
    
        for ( int i = 0; i < number; i++ )
        {
            int sockfd = events[i].data.fd;
            // 如果就绪的fd是listenfd, 则处理新的连接
            if( sockfd == listenfd )
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );

                if(connfd < 0){
                    printf("errno is: %d\n", errno);
                    continue;
                }

                // 记录当前处理的连接
                active_user.push_back(connfd);

                // 如果请求太多，则关闭新到的连接

                addfd( epollfd, connfd );
                users[connfd].address = client_address;
                users[connfd].sockfd = connfd;
                util_timer* timer = new util_timer;
                timer->user_data = &users[connfd];
                timer->cb_func = cb_func;
                time_t cur = time( NULL );
                timer->expire = cur + NUMBER_TIME * TIMESLOT;
                users[connfd].timer = timer;
                timer_lst.add_timer( timer );
            }
            else if( ( sockfd == pipefd[0] ) && ( events[i].events & EPOLLIN ) )
            {
                int sig;
                char signals[1024];
                ret = recv( pipefd[0], signals, sizeof( signals ), 0 );
                if( ret == -1 )
                {
                    // handle the error
                    continue;
                }
                else if( ret == 0 )
                {
                    continue;
                }
                else
                {
                    for( int i = 0; i < ret; ++i )
                    {
                        switch( signals[i] )
                        {
                            case SIGALRM:
                            {
                                timeout = true;
                                break;
                            }
                            case SIGTERM:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            // 处理客户连接上接收到的数据
            else if(  events[i].events & EPOLLIN )
            {
                memset( users[sockfd].buf, '\0', BUFFER_SIZE );
                ret = recv( sockfd, users[sockfd].buf, BUFFER_SIZE-1, 0 );
                printf( "get %d bytes of client data %s from %d\n", ret, users[sockfd].buf, sockfd );
                util_timer* timer = users[sockfd].timer;
                if( ret < 0 )
                {
                    // 如果发生读错误，则关闭连接，并移除其对应的的定时器
                    if( errno != EAGAIN )
                    {
                        cb_func( &users[sockfd] );
                        for(int j = 0; j < active_user.size(); j++){
                            if(active_user[j] == sockfd) active_user[j] = -1;
                        }
                        if( timer )
                        {
                            timer_lst.del_timer( timer );
                        }
                    }
                }
                else if( ret == 0 )
                {
                    // 如果对方已经关闭连接，则我们也关闭连接，并移除对应的定时器
                    cb_func( &users[sockfd] );
                    for(int j = 0; j < active_user.size(); j++){
                        if(active_user[j] == sockfd) active_user[j] = -1;
                    }
                    if( timer )
                    {
                        timer_lst.del_timer( timer );
                    }
                }
                else
                {
                    // send( sockfd, users[sockfd].buf, BUFFER_SIZE-1, 0 );
                    // 拷贝当前信息作为临时变量，避免中途存在user加入，也会收到消息
                    char* tempbuf = users[sockfd].buf;
                    std::vector<int> temp_active_user = active_user;
                    results.emplace_back(
                        pool.enqueue([sockfd, tempbuf, temp_active_user]{
                            for(int j = 0; j < temp_active_user.size(); j++){
                                if(temp_active_user[j] == sockfd || temp_active_user[j] == -1) continue;
                                write(temp_active_user[j], tempbuf, BUFFER_SIZE-1);
                            }
                            return sockfd;
                        })
                    );

                    // 如果某个客户连接上有数据可读，则我们需要调整该连接对应的定时器，以延迟该连接被关闭的时间
                    if( timer )
                    {
                        time_t cur = time( NULL );
                        timer->expire = cur + NUMBER_TIME * TIMESLOT;
                        printf( "adjust timer once\n" );
                        timer_lst.adjust_timer( timer );
                    }
                    
                }
            }
            else
            {
                // others
            }
        }

        if( timeout )
        {
            timer_handler();
            timeout = false;
        }
    }

    for (auto && result : results) {
		std::cout << result.get() << ' ' << std::endl;
	}

    close( listenfd );
    close( pipefd[1] );
    close( pipefd[0] );
    delete [] users;
    return 0;

}