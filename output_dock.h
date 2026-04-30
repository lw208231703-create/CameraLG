#ifndef OUTPUT_DOCK_H
#define OUTPUT_DOCK_H

#include <QDockWidget>

class QTextEdit;

class OutputDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit OutputDock(QWidget *parent = nullptr);

    QTextEdit *textEdit() const;
    void clearMessages();
    void appendMessage(const QString &message);

    /**
     * @brief Append an unfiltered diagnostic message for logging purposes
     */
    void appendDiagnostic(const QString &message);

private:
    QTextEdit *textEdit_ = nullptr;
};

#endif // OUTPUT_DOCK_H
