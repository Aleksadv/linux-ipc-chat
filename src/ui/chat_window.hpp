#pragma once

#include <QString>
#include <QWidget>

class QLineEdit;
class QListWidget;

class ChatWindow : public QWidget
{
public:
    explicit ChatWindow(const QString& participantName,
                        QWidget* parent = nullptr);

private:
    void sendMessage();

    QString participantName_;
    QListWidget* history_;
    QLineEdit* input_;
};