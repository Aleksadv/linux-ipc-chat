#include "chat_window.hpp"

#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ChatWindow::ChatWindow(const QString& participantName, QWidget* parent)
    : QWidget(parent),
      participantName_(participantName),
      history_(new QListWidget(this)),
      input_(new QLineEdit(this))
{
    setWindowTitle(QStringLiteral("Linux IPC Chat — %1").arg(participantName_));
    resize(520, 360);

    auto* layout = new QVBoxLayout(this);
    auto* sendButton = new QPushButton(QStringLiteral("Отправить"), this);

    input_->setPlaceholderText(QStringLiteral("Введите сообщение"));

    layout->addWidget(history_);
    layout->addWidget(input_);
    layout->addWidget(sendButton);

    connect(input_, &QLineEdit::returnPressed,
            this, &ChatWindow::sendMessage);
    connect(sendButton, &QPushButton::clicked,
            this, &ChatWindow::sendMessage);

    input_->setFocus();
}

void ChatWindow::sendMessage()
{
    const QString text = input_->text().trimmed();

    if (text.isEmpty()) {
        return;
    }

    history_->addItem(participantName_ + QStringLiteral(": ") + text);
    input_->clear();
    input_->setFocus();
}