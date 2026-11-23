#include "ipc/conn_pipe.hpp"
#include "ui/chat_window.hpp"

#include <QApplication>
#include <csignal>
#include <cstdio>
#include <exception>
#include <unistd.h>

namespace {

int runWindow(int& argc, char** argv, bool isHost,
              PipeChannel& incoming, PipeChannel& outgoing)
{
    incoming.closeWriter();
    outgoing.closeReader();

    QApplication app(argc, argv);

    const QString name = isHost ? QStringLiteral("Хост")
                                : QStringLiteral("Клиент 1");
    const QString peer = isHost ? QStringLiteral("Клиент 1")
                                : QStringLiteral("Хост");

    ChatWindow window(name);

    incoming.listen(
        &window,
        [&](const std::string& bytes) {
            window.appendLine(peer + QStringLiteral(": ")
                              + QString::fromStdString(bytes));
        },
        [&](const std::string& error) {
            window.stopSending();

            if (!error.empty()) {
                window.appendLine(QString::fromStdString(error));
            }

            window.appendLine(peer + QStringLiteral(" отключился"));

            if (!isHost) {
                app.quit();
            }
        }
    );

    QObject::connect(
        &window, &ChatWindow::sendRequested, &window,
        [&](const QString& text) {
            try {
                outgoing.send(text.toStdString());
                window.confirmSend(text);
            } catch (const std::exception& e) {
                window.appendLine(QStringLiteral("Не отправлено: ")
                                  + QString::fromUtf8(e.what()));
            }
        }
    );

    if (!isHost) {
        outgoing.send("Клиент подключился");
    }

    window.show();
    return app.exec();
}

} // namespace

int main(int argc, char* argv[])
{
    if (std::signal(SIGCHLD, SIG_IGN) == SIG_ERR ||
        std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        std::perror("signal");
        return 1;
    }

    try {
        PipeChannel toHost;
        PipeChannel toClient;

        const pid_t child = fork();

        if (child == -1) {
            std::perror("fork");
            return 1;
        }

        if (child == 0) {
            try {
                const int code = runWindow(argc, argv, false, toClient, toHost);
                _exit(code);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "client: %s\n", e.what());
                _exit(1);
            }
        }

        return runWindow(argc, argv, true, toHost, toClient);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "host: %s\n", e.what());
        return 1;
    }
}