#include "ipc/conn_pipe.hpp"
#include "ui/chat_window.hpp"

#include <QApplication>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <exception>
#include <unistd.h>

int main(int argc, char* argv[])
{
    // Уведомление о завершении ребёнка для этого шага не требуется.
    // Linux сам освобождает его запись, SIGPIPE превращается в ошибку write().
    if (std::signal(SIGCHLD, SIG_IGN) == SIG_ERR ||
        std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        std::perror("signal");
        return 1;
    }

    try {
        PipeChannel toHost;
        const pid_t child = fork();
        if (child == -1) {
            std::perror("fork");
            return 1;
        }
        if (child == 0) {
            toHost.closeReader();
            try {
                toHost.send("Клиент подключился");
                _exit(0);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "client: %s\n", e.what());
                _exit(1);
            }
        }

        toHost.closeWriter();
        QApplication app(argc, argv);
        ChatWindow window(QStringLiteral("Хост"));
        toHost.listen(&window,
            [&](const std::string& text) { window.appendLine(QString::fromStdString(text)); },
            [&](const std::string& error) {
                if (!error.empty()) {
                    window.appendLine(QString::fromStdString(error));
                }
            });
        window.show();
        return app.exec();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "host: %s\n", e.what());
        return 1;
    }
}
