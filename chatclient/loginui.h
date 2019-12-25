#ifndef LOGINUI_H
#define LOGINUI_H

#include <QDialog>
#include "chatroom.h"
#include <QString>
#include <tuple>
#include <QVector>

namespace Ui {
class Loginui;
}

class Loginui : public QDialog
{
    Q_OBJECT

public:
    explicit Loginui(QWidget *parent = 0);
    ~Loginui();

private slots:
    void on_pb_login_clicked();

signals:
    void loginInfo(QVector<QString> login_info);

private:
    Ui::Loginui *ui;
    Chatroom *chatroom;

};

#endif // LOGINUI_H
