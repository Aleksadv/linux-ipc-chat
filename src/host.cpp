#include "ipc/conn_pipe.hpp"
#include "ui/chat_window.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <exception>
#include <sys/wait.h>
#include <unistd.h>

namespace {

// Только родитель проверяет и собирает статус своего ребёнка.
bool stillRunning(pid_t& child)
{
    if (child <= 0) {
        return false;
    }

    pid_t result;
    do {
        result = waitpid(child, nullptr, WNOHANG);
    } while (result == -1 && errno == EINTR);

    if (result == 0) {
        return true;
    }

    if (result == -1 && errno != ECHILD) {
        std::perror("waitpid");
    }

    child = -1;
    return false;
}

int runWindow(int& argc, char** argv, pid_t& child,
              PipeChannel& incoming, PipeChannel& outgoing)
{
    const bool isHost = child > 0;

    incoming.closeWriter();
    outgoing.closeReader();

    QApplication app(argc, argv);

    const QString name = isHost ? QStringLiteral("Хост")
                                : QStringLiteral("Клиент 1");
    const QString peer = isHost ? QStringLiteral("Клиент 1")
                                : QStringLiteral("Хост");

    ChatWindow window(name);

    QElapsedTimer idle;
    QTimer watchdog;
    bool awaitingHello = isHost;
    bool killSent = false;

    if (isHost) {
        idle.start();

        QObject::connect(&watchdog, &QTimer::timeout, &window, [&] {
            if (!stillRunning(child)) {
                watchdog.stop();
                return;
            }

            if (!killSent && idle.elapsed() > 60000) {
                if (kill(child, SIGKILL) == 0) {
                    killSent = true;
                    window.stopSending();
                    window.appendLine(
                        QStringLiteral("Клиент отключён за минуту без сообщений")
                    );
                } else if (errno != ESRCH) {
                    std::perror("kill");
                }
            }
        });

        watchdog.start(250);
    }

    incoming.listen(
        &window,
        [&](const std::string& bytes) {
            if (awaitingHello) {
                // Первым кадром клиент всегда присылает приветствие.
                awaitingHello = false;
                window.appendLine(QString::fromStdString(bytes));
                return;
            }

            if (isHost) {
                idle.restart();
            }

            window.appendLine(peer + QStringLiteral(": ")
                              + QString::fromStdString(bytes));
        },
        [&](const std::string& error) {
            window.stopSending();

            if (!error.empty()) {
                window.appendLine(QString::fromStdString(error));
            }

            if (!killSent) {
                window.appendLine(peer + QStringLiteral(" отключился"));
            }

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
    if (std::signal(SIGCHLD, SIG_DFL) == SIG_ERR ||
        std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        std::perror("signal");
        return 1;
    }

    try {
        PipeChannel toHost;
        PipeChannel toClient;

        pid_t child = fork();

        if (child == -1) {
            std::perror("fork");
            return 1;
        }

        if (child == 0) {
            try {
                const int code = runWindow(argc, argv, child, toClient, toHost);
                _exit(code);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "client: %s\n", e.what());
                _exit(1);
            }
        }

        const int code = runWindow(argc, argv, child, toHost, toClient);

        // Если пользователь закрыл хоста, клиент получит EOF.
        toClient.closeWriter();
        toHost.closeReader();

        if (child > 0) {
            while (waitpid(child, nullptr, 0) == -1) {
                if (errno == EINTR) {
                    continue;
                }

                std::perror("waitpid");
                return 1;
            }
        }

        return code;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "host: %s\n", e.what());
        return 1;
    }
}