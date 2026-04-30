#include "output_dock.h"

#include <QTextEdit>
#include <QDateTime>

OutputDock::OutputDock(QWidget *parent)
    : QDockWidget(tr("Output Messages"), parent)
    , textEdit_(new QTextEdit(this))
{
    setObjectName(QStringLiteral("outputDock"));
    textEdit_->setReadOnly(true);
    textEdit_->clear();
    setWidget(textEdit_);
    setAllowedAreas(Qt::AllDockWidgetAreas);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
}

QTextEdit *OutputDock::textEdit() const
{
    return textEdit_;
}

void OutputDock::clearMessages()
{
    textEdit_->clear();
}

void OutputDock::appendMessage(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("[HH:mm:ss]");
    textEdit_->append(timestamp + " " + message.trimmed());

    if (textEdit_->document()->blockCount() > 1000) {
        QTextCursor cursor(textEdit_->document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }
}

void OutputDock::appendDiagnostic(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("[HH:mm:ss]");
    textEdit_->append(timestamp + " " + message.trimmed());

    if (textEdit_->document()->blockCount() > 2000) {
        QTextCursor cursor(textEdit_->document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }
}
