#include "widget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Widget w;

    w.setWindowTitle("Upgrade V1.2");
    w.show();

    return a.exec();
}
