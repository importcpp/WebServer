#include "utils.h"


void Utils::closeSock(int fd)
{
    close(fd);
}


int Utils::connectSock(int &sock, QString s_ip, int s_port)
{
    sock = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(s_ip.toLocal8Bit().data());
    serv_addr.sin_port = htons(s_port);

    int ret = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    qDebug() << ret << endl;
    return ret;
}

