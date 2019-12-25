#include "loginui.h"
#include "ui_loginui.h"

Loginui::Loginui(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Loginui)
{
    ui->setupUi(this);
    chatroom = new Chatroom(this);
    connect(this, &Loginui::loginInfo, chatroom, &Chatroom::recvLoginInfo);
    ui->le_sip->setText("127.0.0.1");
}

Loginui::~Loginui()
{
    delete ui;
}

void Loginui::on_pb_login_clicked()
{

    QString s_ip = ui->le_sip->displayText();
    QString s_port = ui->le_sport->displayText();
    QString user_name = "[" + ui->le_username->displayText() + "]";

    QVector<QString> login_info = {s_ip, s_port, user_name};

    emit loginInfo(login_info);

    this->hide();
    chatroom->exec();
}
