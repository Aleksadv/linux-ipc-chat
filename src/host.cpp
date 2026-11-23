#include <QApplication>
#include <QWidget>

int main(int argc, char* argv[])
{

    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle("Chat IPC");
    window.resize(600, 500);
    window.show();

    return app.exec();
}

