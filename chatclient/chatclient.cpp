#include "chatclient.h"

Chatclient::Chatclient(QObject *parent) : QObject(parent)
{

}

void Chatclient::send_msg(int sock, QString name, QString msg)
{
    //char name_msg[NAME_SIZE + BUF_SIZE];

    if(!strcmp(msg.toLocal8Bit().data(), "q\n") || !strcmp(msg.toLocal8Bit().data(), "Q\n")){
        close(sock);
        exit(0);
    }
    // sprintf(name_msg, "%s %s", name.toLocal8Bit().data(), msg.toLocal8Bit().data());
    name = name.append("  ");
    char* name_msg = (name.append(msg).append('\n')).toLocal8Bit().data();
    //name_msg[str_len] = 0;
    write(sock, name_msg, strlen(name_msg));

    return;
}

void Chatclient::error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

void Chatclient::recv_msg(int sock){
    char name_msg[NAME_SIZE + BUF_SIZE];
    int str_len;
    while(1){
        str_len = read(sock, name_msg, NAME_SIZE + BUF_SIZE - 1);
        if(str_len == -1){
            return;
        }
        name_msg[str_len] = 0;

        // fputs(name_msg, stdout);
        emit recv_user_msg(QString::fromUtf8(name_msg));
    }
    return;
}

