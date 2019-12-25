#ifndef CHATROOM_H
#define CHATROOM_H

#include <QDialog>
#include <string>
#include <tuple>
#include <QVector>
#include <thread>
#include "chatclient.h"

#include "utils.h"
#include <mutex>


#define BUF_SIZE 100
#define NAME_SIZE 20



namespace Ui {
class Chatroom;
}

class Chatroom : public QDialog
{
    Q_OBJECT

public:
    explicit Chatroom(QWidget *parent = 0);
    ~Chatroom();

public slots:
    void recvLoginInfo(QVector<QString> login_info);

private slots:
    void on_le_mymsg_returnPressed();

    void on_recv_msg_from_user(QString msg);

private:
    Chatclient chatclient;


private:
    Ui::Chatroom *ui;
    QString s_ip = "192.168.1.134";
    int s_port = 9090;
    QString user_name = "[Anonymous]";
    int sock;
};

#endif // CHATROOM_H
