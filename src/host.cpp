#include "ui/chat_window.hpp"
#include <QApplication>

int main(int argc, char* argv[])
{

    QApplication app(argc, argv);

    ChatWindow window(QStringLiteral("Хост"));
    window.show();

    return app.exec();
}

