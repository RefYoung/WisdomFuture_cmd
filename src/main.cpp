#include <QCoreApplication>
#include <MainClass.h>
#include <signal.h>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    MainClass m;

    return a.exec();
}
