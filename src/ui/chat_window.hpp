#pragma once

#include <QString>
#include <QWidget>

class QLineEdit;
class QListWidget;
class QPushButton;

class ChatWindow : public QWidget {
    Q_OBJECT

public:
    explicit ChatWindow(const QString& participantName, QWidget* parent = nullptr);

    void appendLine(const QString& line);
    void confirmSend(const QString& text);
    void stopSending();

signals:
    void sendRequested(const QString& text);

private:
    void sendMessage();

    QString participantName_;
    QListWidget* history_;
    QLineEdit* input_;
    QPushButton* sendButton_;
};