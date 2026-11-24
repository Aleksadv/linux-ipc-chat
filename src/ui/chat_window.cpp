#include "chat_window.hpp"

#include <QByteArray>
#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVariant>
#include <QVBoxLayout>

ChatWindow::ChatWindow(int self, int clientCount, QWidget* parent)
    : QWidget(parent), self_(self), history_(new QListWidget(this)),
      recipient_(new QComboBox(this)), input_(new QLineEdit(this)),
      sendButton_(new QPushButton(QStringLiteral("Отправить"), this))
{
    setWindowTitle(QStringLiteral("Linux IPC Chat — %1").arg(chat::participantName(self)));
    resize(520, 360);
    recipient_->addItem(QStringLiteral("Общий чат"), chat::everyone);
    if (self != chat::host) recipient_->addItem(chat::participantName(chat::host), chat::host);
    for (int i = 0; i < clientCount; ++i) {
        const int id = i + 1;
        if (id != self) recipient_->addItem(chat::participantName(id), id);
    }
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(history_);
    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Кому"), recipient_);
    layout->addLayout(form);
    input_->setPlaceholderText(QStringLiteral("Введите сообщение"));
    layout->addWidget(input_);
    layout->addWidget(sendButton_);
    connect(input_, &QLineEdit::returnPressed, this, &ChatWindow::sendMessage);
    connect(sendButton_, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    input_->setFocus();
}

void ChatWindow::showMessage(const chat::Message& message)
{
    appendLine(chat::formatMessage(message));
}

void ChatWindow::appendLine(const QString& line)
{
    history_->addItem(line);
    history_->scrollToBottom();
}

void ChatWindow::sendMessage()
{
    const auto text = input_->text().trimmed();
    if (text.isEmpty()) return;
    if (text.toUtf8().size() > chat::maxTextBytes) {
        appendLine(QStringLiteral("Сообщение длиннее %1 байт UTF-8").arg(chat::maxTextBytes));
        return;
    }
    emit sendRequested(chat::Message{self_, recipient_->currentData().toInt(), 0, text});
}

void ChatWindow::clearInput()
{
    input_->clear();
    input_->setFocus();
}

void ChatWindow::stopSending()
{
    recipient_->setEnabled(false);
    input_->setEnabled(false);
    sendButton_->setEnabled(false);
}
