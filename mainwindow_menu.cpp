#include "mainwindow_refactored.h"
#include "build_config.h"
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QSettings>

void MainWindowRefactored::setupMenuAndToolBar()
{
}

void MainWindowRefactored::updateThemeMenuActions()
{
}

void MainWindowRefactored::onActionNew()
{
    showTransientMessage(tr("New action triggered"));
}

void MainWindowRefactored::onActionOpen()
{
    showTransientMessage(tr("Open action triggered"));
}

void MainWindowRefactored::onActionSave()
{
    showTransientMessage(tr("Save action triggered"));
}

void MainWindowRefactored::onActionExit()
{
    close();
}
