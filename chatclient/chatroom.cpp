#include "chatroom.h"
#include "ui_chatroom.h"


Chatroom::Chatroom(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Chatroom)
{
    ui->setupUi(this);
    connect(&chatclient, &Chatclient::recv_user_msg, this, &Chatroom::on_recv_msg_from_user);
}

Chatroom::~Chatroom()
{
    delete ui;
}

void Chatroom::recvLoginInfo(QVector<QString> login_info)
{

    this->s_ip = login_info[0];
    this->s_port = (login_info[1]).toInt();
    this->user_name = login_info[2];
    if (Utils::connectSock(this->sock,  this->s_ip, this->s_port) == -1){
        QString temp = "connect() error";
        chatclient.error_handling(temp.toLocal8Bit().data());
    }

    // todo: add thread to chatclient recv
    std::thread th(&Chatclient::recv_msg, &chatclient, this->sock);
    th.detach();
}

void Chatroom::on_le_mymsg_returnPressed()
{
    QString msg = ui->le_mymsg->displayText();
    qDebug() << "hello" << endl;
    chatclient.send_msg(this->sock, this->user_name, msg);

    ui->te_allmsg->setTextColor(Qt::red);
    ui->te_allmsg->append(msg.append("  ").append(this->user_name));
    ui->te_allmsg->setAlignment(Qt::AlignRight);
    ui->le_mymsg->clear();
}

void Chatroom::on_recv_msg_from_user(QString msg)
{
    msg[msg.size() - 1] = ' ';
    ui->te_allmsg->setTextColor(Qt::darkCyan);
    ui->te_allmsg->append(msg);
    ui->te_allmsg->setAlignment(Qt::AlignLeft);

}


