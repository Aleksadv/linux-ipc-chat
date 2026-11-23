#include "chat_window.hpp"

#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ChatWindow::ChatWindow(const QString& participantName, QWidget* parent)
    : QWidget(parent),
      participantName_(participantName),
      history_(new QListWidget(this)),
      input_(new QLineEdit(this)),
      sendButton_(new QPushButton(QStringLiteral("Отправить"), this))
{
    setWindowTitle(QStringLiteral("Linux IPC Chat — %1").arg(participantName_));
    resize(520, 360);

    auto* layout = new QVBoxLayout(this);
    input_->setPlaceholderText(QStringLiteral("Введите сообщение"));

    layout->addWidget(history_);
    layout->addWidget(input_);
    layout->addWidget(sendButton_);

    connect(input_, &QLineEdit::returnPressed,
            this, &ChatWindow::sendMessage);

    connect(sendButton_, &QPushButton::clicked,
            this, &ChatWindow::sendMessage);

    input_->setFocus();
}

void ChatWindow::appendLine(const QString& line)
{
    history_->addItem(line);
    history_->scrollToBottom();
}

void ChatWindow::sendMessage()
{
    const auto text = input_->text().trimmed();

    if (!text.isEmpty()) {
        emit sendRequested(text);
    }
}

void ChatWindow::confirmSend(const QString& text)
{
    appendLine(participantName_ + QStringLiteral(": ") + text);
    input_->clear();
    input_->setFocus();
}

void ChatWindow::stopSending()
{
    input_->setEnabled(false);
    sendButton_->setEnabled(false);
}