#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <QObject>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread> // 使用c++11线程库支持

#define BUF_SIZE 100
#define NAME_SIZE 20


#include <QString>


class Chatclient : public QObject
{
    Q_OBJECT

signals:
    void recv_user_msg(QString name_msg);

public:
    explicit Chatclient(QObject *parent = nullptr);

    void send_msg(int sock, QString name, QString msg);
    void recv_msg(int sock);

    void error_handling(char *msg);

public slots:
};

#endif // CHATCLIENT_H
