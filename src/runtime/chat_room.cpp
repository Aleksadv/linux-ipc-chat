#include "sessions.hpp"
#include "client_reader.hpp"
#include "ui/chat_window.hpp"

#include <QApplication>
#include <QByteArray>
#include <QDeadlineTimer>
#include <QTimer>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <deque>
#include <string>
#include <system_error>

namespace {

// Оставляем место для номера сообщения и имени отправителя в кадре pipe.
constexpr std::size_t maxUserBytes = 4000;

QString sizeError()
{
    return QStringLiteral("Сообщение длиннее 4000 байт UTF-8");
}

struct Pending {
    std::string text;
    QDeadlineTimer deadline{5000};
};

struct Delivery {
    std::unique_ptr<ClientReader> reader;
    std::deque<Pending> queue;
};

} // namespace

int runClient(int& argc, char** argv, const QString& name, Connection& channel)
{
    QApplication app(argc, argv);
    ChatWindow window(name);
    channel.listen(&window,
        [&](const std::string& line) { window.appendLine(QString::fromStdString(line)); },
        [&](const std::string& reason) {
            if (!reason.empty()) std::fprintf(stderr, "client: %s\n", reason.c_str());
            app.quit();
        });

    QObject::connect(&window, &ChatWindow::sendRequested, &window,
        [&](const QString& text) {
            const auto bytes = text.toStdString();
            if (bytes.size() > maxUserBytes) {
                window.appendLine(sizeError());
                return;
            }
            try {
                channel.send(bytes);
                window.clearInput(); // Строку в историю доставит хост.
            } catch (const std::exception& e) {
                window.appendLine(QStringLiteral("Не отправлено: ") + QString::fromUtf8(e.what()));
            }
        });
    channel.send("HELLO");
    window.show();
    return app.exec();
}

int runHost(int& argc, char** argv, Sessions& sessions)
{
    QApplication app(argc, argv);
    ChatWindow window(QStringLiteral("Хост"));
    std::vector<Delivery> deliveries(sessions.size());
    quint64 sequence = 0;

    auto disconnect = [&](std::size_t i, const QString& reason) {
        auto& s = *sessions[i];
        if (!s.online) return;
        s.online = false;
        deliveries[i].reader.reset(); // Дождаться выхода потока перед close().
        deliveries[i].queue.clear();
        s.channel->close();
        window.appendLine(s.name + QStringLiteral(": ") + reason);
        for (const auto& other : sessions) {
            if (other->online) return;
        }
        window.stopSending();
    };

    auto flush = [&](std::size_t i) {
        auto& queue = deliveries[i].queue;
        while (!queue.empty()) {
            if (queue.front().deadline.hasExpired()) {
                disconnect(i, QStringLiteral("истёк срок доставки сообщения"));
                return;
            }
            try {
                sessions[i]->channel->send(queue.front().text);
                queue.pop_front();
            } catch (const std::system_error& e) {
                if (e.code().value() == EAGAIN || e.code().value() == EWOULDBLOCK) return;
                disconnect(i, QString::fromUtf8(e.what()));
                return;
            } catch (const std::exception& e) {
                disconnect(i, QString::fromUtf8(e.what()));
                return;
            }
        }
    };

    auto broadcast = [&](const QString& sender, const QString& text) {
        const auto line = QStringLiteral("#%1 %2: %3").arg(++sequence).arg(sender, text);
        const auto bytes = line.toStdString();
        window.appendLine(line);
        for (std::size_t i = 0; i < sessions.size(); ++i) {
            if (!sessions[i]->online) continue;
            auto& queue = deliveries[i].queue;
            if (queue.size() >= 128) {
                disconnect(i, QStringLiteral("переполнена очередь доставки"));
                continue;
            }
            queue.push_back(Pending{bytes});
            flush(i);
        }
    };

    for (std::size_t i = 0; i < sessions.size(); ++i) {
        auto& d = deliveries[i];
        d.reader = std::make_unique<ClientReader>(*sessions[i]->channel);
        d.reader->setObjectName(QStringLiteral("chat-reader-%1").arg(i + 1));
        QObject::connect(d.reader.get(), &ClientReader::received, &window,
            [&, i](const QString& message) {
                auto& s = *sessions[i];
                if (!s.online) return;
                if (!s.greeted) {
                    if (message != QStringLiteral("HELLO")) {
                        disconnect(i, QStringLiteral("ошибка первого сообщения"));
                        return;
                    }
                    s.greeted = true;
                    window.appendLine(s.name + QStringLiteral(" подключился"));
                    return;
                }
                if (message.toUtf8().size() > static_cast<qsizetype>(maxUserBytes)) {
                    disconnect(i, sizeError());
                    return;
                }
                s.idle.restart();
                broadcast(s.name, message);
            }, Qt::QueuedConnection);
        QObject::connect(d.reader.get(), &ClientReader::disconnected, &window,
            [&, i](const QString& reason) {
                disconnect(i, reason.isEmpty() ? QStringLiteral("отключился") : reason);
            }, Qt::QueuedConnection);
        d.reader->start();
    }

    QObject::connect(&window, &ChatWindow::sendRequested, &window,
        [&](const QString& text) {
            if (text.toUtf8().size() > static_cast<qsizetype>(maxUserBytes)) {
                window.appendLine(sizeError());
                return;
            }
            broadcast(QStringLiteral("Хост"), text);
            window.clearInput();
        });

    QTimer watchdog;
    QObject::connect(&watchdog, &QTimer::timeout, &window, [&] {
        for (std::size_t i = 0; i < sessions.size(); ++i) {
            auto& s = *sessions[i];
            const bool alive = stillRunning(s); // Забираем статус и у отключённых.
            if (!s.online) continue;
            if (alive && s.idle.elapsed() > 60000) {
                if (kill(s.pid, SIGKILL) == 0) {
                    disconnect(i, QStringLiteral("отключён за минуту без сообщений"));
                } else if (errno != ESRCH) {
                    std::perror("kill");
                }
            } else {
                flush(i);
            }
        }
    });
    watchdog.start(200);
    window.show();
    return app.exec();
}
