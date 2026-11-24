#include "ipc/connection.hpp"
#include "runtime/client_reader.hpp"
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

// Только родитель вызывает эту функцию. После сбора статуса PID забываем.
bool stillRunning(pid_t& child)
{
    if (child <= 0) return false;
    pid_t result;
    do {
        result = waitpid(child, nullptr, WNOHANG);
    } while (result == -1 && errno == EINTR);
    if (result == 0) return true;
    if (result == -1 && errno != ECHILD) std::perror("waitpid");
    child = -1;
    return false;
}

int runWindow(int& argc, char** argv, pid_t& child,
              Connection& connection)
{
    const bool isHost = child > 0;
    connection.selectSide(isHost);
    QApplication app(argc, argv);
    const QString name = isHost ? QStringLiteral("Хост") : QStringLiteral("Клиент 1");
    const QString peer = isHost ? QStringLiteral("Клиент 1") : QStringLiteral("Хост");
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
                    window.appendLine(QStringLiteral("Клиент отключён за минуту без сообщений"));
                } else if (errno != ESRCH) {
                    std::perror("kill");
                }
            }
        });
        watchdog.start(250);
    }

    auto onText = [&](const QString& text) {
        if (awaitingHello) {
            awaitingHello = false;
            window.appendLine(text);
            return;
        }
        if (isHost) idle.restart();
        window.appendLine(peer + QStringLiteral(": ") + text);
    };
    auto onEnd = [&](const QString& reason) {
        window.stopSending();
        if (!reason.isEmpty()) window.appendLine(reason);
        if (!killSent) window.appendLine(peer + QStringLiteral(" отключился"));
        if (!isHost) app.quit();
    };

    // Деструктор reader останавливает поток до уничтожения окна и соединения.
    ClientReader reader(connection);
    if (isHost) {
        reader.setObjectName(QStringLiteral("chat-reader-1"));
        QObject::connect(&reader, &ClientReader::received, &window,
                         onText, Qt::QueuedConnection);
        QObject::connect(&reader, &ClientReader::disconnected, &window,
                         onEnd, Qt::QueuedConnection);
        reader.start();
    } else {
        connection.listen(&window,
            [&](const std::string& text) { onText(QString::fromStdString(text)); },
            [&](const std::string& reason) { onEnd(QString::fromStdString(reason)); });
    }

    QObject::connect(&window, &ChatWindow::sendRequested, &window,
        [&](const QString& text) {
            try {
                connection.send(text.toStdString());
                window.confirmSend(text);
            } catch (const std::exception& e) {
                window.appendLine(QStringLiteral("Не отправлено: ") + QString::fromUtf8(e.what()));
            }
        });

    if (!isHost) connection.send("Клиент подключился");
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
        auto connection = openConnection();
        pid_t child = fork();
        if (child == -1) {
            std::perror("fork");
            return 1;
        }
        if (child == 0) {
            try {
                const int code = runWindow(argc, argv, child, *connection);
                _exit(code);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "client: %s\n", e.what());
                _exit(1);
            }
        }

        int code = 1;
        try {
            code = runWindow(argc, argv, child, *connection);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "host: %s\n", e.what());
            // При ошибке запуска ребёнок может ещё ждать готовности хоста.
            if (child > 0 && kill(child, SIGKILL) == -1 && errno != ESRCH) {
                std::perror("kill");
            }
        }
        connection->close(); // Клиент получит EOF и закроет своё окно.
        if (child > 0) {
            while (waitpid(child, nullptr, 0) == -1) {
                if (errno == EINTR) continue;
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
