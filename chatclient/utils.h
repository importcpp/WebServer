#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread> // 使用c++11线程库支持
#include <mutex>

#include <QDebug>
#include <QString>

namespace Utils {
    void closeSock(int fd);

    int connectSock(int &sock, QString s_ip, int s_port);
}



#endif // UTILS_H
