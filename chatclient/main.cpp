#include "loginui.h"
#include <QApplication>
#include <iostream>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Loginui w;
    w.show();
    return a.exec();
}
