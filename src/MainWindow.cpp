#include "MainWindow.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QTabBar>
#include <QToolBar>
#include <QStyle>
#include "SharedMenuBar.h"
#include "TabWidget.h"

namespace UJ
{

namespace Qelly
{

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    buildToolBar();
    _tabs = new TabWidget(this);
    _tabs->setTabPosition(QTabWidget::North);
    int cellWidth = 12;     // NOTE: Use global preferences
    int cellHeight = 25;    // NOTE: Use global preferences
    int row = 24;           // NOTE: Use global preferences
    int column = 80;        // NOTE: Use global preferences
    _tabs->resize(cellWidth * column,
                  cellHeight * row + _tabs->getTabBar()->height());
    setCentralWidget(_tabs);
    setMenuBar(SharedMenuBar::sharedInstance());
    resize(_tabs->width(), _tabs->height() + _toolbar->height());
}

MainWindow::~MainWindow()
{
    delete _stretch;
    delete _inputFrame;
}

void MainWindow::buildToolBar()
{
    _stretch = new QWidget();
    _stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QLineEdit *input = new QLineEdit();
    input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QLabel *inputLabel = new QLabel(QString("<small>") +
                                    tr("Address") +
                                    QString("</small>"));
    inputLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    inputLabel->setAlignment(Qt::AlignCenter);
    inputLabel->setFocusPolicy(Qt::NoFocus);
    _inputFrame = new QWidget();
    _inputFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout *inputLayout = new QVBoxLayout(_inputFrame);
    inputLayout->addWidget(input);
    inputLayout->addWidget(inputLabel);
    _inputFrame->setLayout(inputLayout);

    QStyle *style = qApp->style();
    _toolbar = addToolBar(tr("General"));
    _toolbar->addAction(style->standardIcon(QStyle::SP_DriveNetIcon),
                        tr("Sites"));
    _toolbar->addAction(style->standardIcon(QStyle::SP_BrowserReload),
                        tr("Reconnect"));
    _toolbar->addAction(style->standardIcon(QStyle::SP_DialogSaveButton),
                        tr("Sites"));
    _toolbar->addWidget(_inputFrame);
    _toolbar->addWidget(_stretch);
    _toolbar->addAction(style->standardIcon(QStyle::SP_DirIcon),
                        tr("Emicons"));
    _toolbar->addAction(style->standardIcon(QStyle::SP_DirIcon),
                        tr("Anti-Idle"));
    _toolbar->addAction(style->standardIcon(QStyle::SP_DirIcon),
                        tr("Peek"));
    _toolbar->addAction(style->standardIcon(QStyle::SP_DirIcon),
                        tr("Double Byte"));
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setUnifiedTitleAndToolBarOnMac(true);
}

}   // namespace Qelly

}   // namespace UJ
