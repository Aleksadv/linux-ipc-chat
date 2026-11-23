#include <QApplication>
#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

int main(int argc, char* argv[])
{

    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle("Chat IPC");
    window.resize(600, 500);

    auto* layout = new QVBoxLayout(&window);
    auto* history = new QListWidget(&window);
    auto* input = new QLineEdit(&window);
    auto* sendButton = new QPushButton("Отправить", &window);

    input->setPlaceholderText("Введите сообщение");

    layout->addWidget(history);
    layout->addWidget(input);
    layout->addWidget(sendButton);

    const auto sendMessage = [input, history]() {
        const QString text = input->text().trimmed();

        if (text.isEmpty()) {
            return;
        }

        history->addItem(QStringLiteral("Хост: ") + text);
        input->clear();
        input->setFocus();
    };

    QObject::connect(input, &QLineEdit::returnPressed,
                 &window, sendMessage);

    QObject::connect(sendButton, &QPushButton::clicked,
                 &window, sendMessage);

    window.show();
    input->setFocus();
    
    return app.exec();
}

