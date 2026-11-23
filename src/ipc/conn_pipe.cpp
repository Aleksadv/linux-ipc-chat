#include "conn_pipe.hpp"
#include "chat/frame.hpp"

#include <QSocketNotifier>
#include <cerrno>
#include <fcntl.h>
#include <limits.h>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace {

void closeEnd(int& fd)
{
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

void setNonblocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::system_error(errno, std::generic_category(), "fcntl");
    }
}

} // namespace

PipeChannel::PipeChannel()
{
    if (::pipe(ends_) == -1) {
        throw std::system_error(errno, std::generic_category(), "pipe");
    }
    try {
        setNonblocking(ends_[0]);
        setNonblocking(ends_[1]);
    } catch (...) {
        closeReader();
        closeWriter();
        throw;
    }
}

PipeChannel::~PipeChannel()
{
    closeReader();
    closeWriter();
}

void PipeChannel::closeReader() { closeEnd(ends_[0]); }
void PipeChannel::closeWriter() { closeEnd(ends_[1]); }

void PipeChannel::send(std::string_view text) const
{
    const auto frame = chat::encodeFrame(text);
    // В неблокирующей трубе запись до PIPE_BUF включительно либо целая,
    // либо завершается ошибкой. Поэтому повторная отправка не испортит поток.
    if (frame.size() > PIPE_BUF) {
        throw std::length_error("Сообщение слишком длинное для одной записи в pipe");
    }
    ssize_t n;
    do {
        n = ::write(ends_[1], frame.data(), frame.size());
    } while (n == -1 && errno == EINTR);

    if (n == -1) {
        throw std::system_error(errno, std::generic_category(), "write");
    }
    if (static_cast<std::size_t>(n) != frame.size()) {
        throw std::runtime_error("Неполная запись кадра");
    }
}

void PipeChannel::listen(QObject* context,
                         std::function<void(const std::string&)> onMessage,
                         std::function<void(const std::string&)> onEnd)
{
    auto* ready = new QSocketNotifier(ends_[0], QSocketNotifier::Read, context);
    QObject::connect(ready, &QSocketNotifier::activated, context,
        [this, ready, onMessage = std::move(onMessage), onEnd = std::move(onEnd)] {
            char chunk[1024];
            const ssize_t n = ::read(ends_[0], chunk, sizeof(chunk));

            if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
            }

            try {
                if (n < 0) {
                    throw std::system_error(errno, std::generic_category(), "read");
                }
                if (n == 0) {
                    ready->setEnabled(false);
                    onEnd(buffer_.empty() ? "" : "Соединение оборвалось внутри кадра");
                    return;
                }
                buffer_.append(chunk, static_cast<std::size_t>(n));
                while (auto message = chat::takeFrame(buffer_)) {
                    onMessage(*message);
                }
            } catch (const std::exception& e) {
                ready->setEnabled(false);
                onEnd(e.what());
            }
        });
}
