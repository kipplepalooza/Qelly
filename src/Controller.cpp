/*****************************************************************************
 * Controller.cpp
 *
 * Created: 09/10 2011 by uranusjr
 *
 * Copyright 2011 uranusjr. All rights reserved.
 *
 * This file may be distributed under the terms of GNU Public License version
 * 3 (GPL v3) as defined by the Free Software Foundation (FSF). A copy of the
 * license should have been included with this file, or the project in which
 * this file belongs to. You may also find the details of GPL v3 at:
 * http://www.gnu.org/licenses/gpl-3.0.txt
 *
 * If you have any questions regarding the use of this file, feel free to
 * contact the author of this file, or the owner of the project in which
 * this file belongs to.
 *****************************************************************************/

#include "Controller.h"
#include <QApplication>
#include <QDesktopServices>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QUrl>
#include "Globals.h"
#include "EmoticonViewer.h"
#include "MainWindow.h"
#include "PreferencesWindow.h"
#include "SharedMenuBar.h"
#include "SharedPreferences.h"
#include "Site.h"
#include "SiteManagerDialog.h"
#include "Ssh.h"
#include "TabWidget.h"
#include "Telnet.h"
#include "Terminal.h"
#include "View.h"
#include "Tab.h"

namespace UJ
{

namespace Qelly
{

Controller::Controller(QObject *parent) :
    QObject(parent), _antiIdleTimer(0), _preferencesWindow(0)
{
    _window = new MainWindow();
    SharedMenuBar *menu = SharedMenuBar::sharedInstance();
    setAntiIdleTimer(SharedPreferences::sharedInstance()->isAntiIdleActive());

    connect(menu, SIGNAL(preferences()), SLOT(showPreferencesWindow()));
    connect(menu, SIGNAL(fileNewTab()), SLOT(addTab()));
    connect(menu, SIGNAL(fileOpenLocation()), SLOT(focusAddressField()));
    connect(menu, SIGNAL(fileReconnect()), SLOT(reconnect()));
    connect(menu, SIGNAL(fileCloseTab()), SLOT(closeTab()));
    connect(menu, SIGNAL(fileQuit()), SLOT(closeWindow()));
    connect(menu, SIGNAL(editCopy()), SLOT(copy()));
    connect(menu, SIGNAL(editEmoticons()), SLOT(showEmoticonViewer()));
    connect(menu, SIGNAL(editPaste()), SLOT(paste()));
    connect(menu, SIGNAL(editPasteColor()), SLOT(pasteColor()));
    connect(menu, SIGNAL(viewAntiIdle(bool)), SLOT(toggleAntiIdle(bool)));
    connect(menu, SIGNAL(viewShowHiddenText(bool)),
            SLOT(toggleShowHiddenText(bool)));
    _window->connect(menu, SIGNAL(viewToggleToolbar(bool)),
                     SLOT(setToolbarVisible(bool)));
    connect(menu, SIGNAL(sitesEditSites()), SLOT(showSiteManager()));
    connect(menu, SIGNAL(siteAddThisSite()), SLOT(addCurrentSite()));
    connect(menu, SIGNAL(windowMinimize()), SLOT(minimize()));
    connect(menu, SIGNAL(windowSelectNextTab()), SLOT(tabNext()));
    connect(menu, SIGNAL(windowSelectPreviousTab()), SLOT(tabPrevious()));
    connect(menu, SIGNAL(about()), SLOT(showAbout()));
    connect(menu, SIGNAL(windowAcknowledgement()), SLOT(showAcknowledgement()));
    connect(menu, SIGNAL(helpVisitProjectHome()), SLOT(visitProject()));
    connect(_window, SIGNAL(siteManageShouldOpen()), SLOT(showSiteManager()));
    connect(_window, SIGNAL(reconnect()), SLOT(reconnect()));
    connect(_window, SIGNAL(windowShouldClose()), SLOT(closeWindow()));
    connect(_window, SIGNAL(addCurrentSite()), SLOT(addCurrentSite()));
    connect(_window, SIGNAL(antiIdleTriggered(bool)),
            SLOT(toggleAntiIdle(bool)));
    connect(_window, SIGNAL(showHiddenTextTriggered(bool)),
            SLOT(toggleShowHiddenText(bool)));
    connect(_window, SIGNAL(emoticonViewerShouldOpen()),
            SLOT(showEmoticonViewer()));
    connect(_window->address(), SIGNAL(returnPressed()),
            SLOT(onAddressReturnPressed()));
    connect(_window->tabs(), SIGNAL(tabCloseRequested(int)),
            SLOT(closeTab(int)));

    SharedPreferences *prefs = SharedPreferences::sharedInstance();
    if (prefs->isMaximized())
        _window->showMaximized();
    else
        _window->show();
    if (prefs->restoreConnectionsOnStartup())
    {
        foreach (Connection::Site *site, prefs->storedConnections())
        {
            addTab(false);
            connectWith(site);
        }
    }
    if (!_window->tabs()->count())
        addTab();
}

Controller::~Controller()
{
    SharedPreferences *prefs = SharedPreferences::sharedInstance();
    if (prefs->restoreConnectionsOnStartup())
    {
        QList<Connection::Site *> sites;
        for (int i = 0; i < _window->tabs()->count(); i++)
        {
            View *view = viewInTab(i);
            if (!view->terminal() || !view->terminal()->connection())
                continue;
            Connection::Site *site = view->terminal()->connection()->site();
            if (site)
                sites << site;
        }
        prefs->storeConnections(sites);
    }
    prefs->sync();  // Force sync because we might not have another chance!
    delete _window;
}

void Controller::connectWith(const QString &address)
{
    connectWith(new Connection::Site(address, address));
}

void Controller::connectWith(Connection::Site *site)
{
    SharedPreferences *prefs = SharedPreferences::sharedInstance();
    Connection::AbstractConnection *connection = 0;
    switch (site->type())
    {
    case Connection::TypeSsh:
        if (!prefs->isSshEnabled())
        {
            QString title = tr("SSH Not Available");
            QString msg = tr("To enable SSH connections, you need to enable "
                             "SSH, and set a correct path to an external SSH "
                             "client executable.");
            QMessageBox box(QMessageBox::Question, title, msg,
                            QMessageBox::Cancel);
            QPushButton *open = box.addButton(tr("Configure"),
                                              QMessageBox::YesRole);
            box.exec();
            if (reinterpret_cast<QPushButton *>(box.clickedButton()) == open)
                showPreferencesWindow();
            else
                focusAddressField();
            break;
        }
        connection = new Connection::Ssh(prefs->sshClientPath());
        break;
    default:
        connection = new Connection::Telnet();
        break;
    }

    if (!connection)
        return;
    if (currentView()->isConnected())
        addTab();
    _window->tabs()->setTabText(_window->tabs()->currentIndex(), site->name());
    View *view = currentView();
    Connection::Terminal *terminal = new Connection::Terminal(view);
    terminal->setConnection(connection);
    view->setTerminal(terminal);
    view->setAddress(site->fullForm());
    view->setFocus(Qt::OtherFocusReason);
    connect(_window->tabs(), SIGNAL(currentChanged(int)),
            SLOT(onTabChanged(int)));
    connection->connectToSite(site);
    changeAddressField(site->fullForm());
}

void Controller::focusAddressField()
{
    SharedPreferences *prefs = SharedPreferences::sharedInstance();
    if (prefs->isToolbarVisible())
    {
        QLineEdit *address = _window->address();
        address->setFocus(Qt::ShortcutFocusReason);
        address->selectAll();
    }
    else
    {
        QString text = QInputDialog::getText(
                    _window, tr("Connect to..."), tr("Address:"));
        if (!text.isEmpty())
        {
            _window->address()->setText(text);
            onAddressReturnPressed();
        }
    }
}

void Controller::addTab(bool focus)
{
    _window->tabs()->addTab(new Tab(new View()), "");
    _window->address()->setText(QString());
    if (focus)
        focusAddressField();
}

void Controller::closeTab()
{
    closeTab(_window->tabs()->currentIndex());
}

void Controller::closeTab(int index)
{
    TabWidget *tabs = _window->tabs();
    View *view = viewInTab(index);
    SharedPreferences *prefs = SharedPreferences::sharedInstance();
    if (view)
    {
        if (view->isConnected() && prefs->warnOnClose())
        {
            QMessageBox sure(_window);
            sure.setIcon(QMessageBox::Warning);
            sure.setText(tr("Are you sure you want to close this tab?"));
            sure.setInformativeText(
                tr("The connection is still alive. If you close this tab, the "
                   "connection will be lost. Do you want to close this tab "
                   "anyway?"));
            sure.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            sure.setWindowModality(Qt::WindowModal);
            sure.setFocus(Qt::PopupFocusReason);
            switch (sure.exec())
            {
            case QMessageBox::Ok:
                tabs->closeTab(index);
                break;
            case QMessageBox::Cancel:
                break;
            default:
                break;
            }
        }
        else
        {
            tabs->closeTab(index);
        }
    }

    // Finish up after the tab is closed
    view = viewInTab(_window->tabs()->currentIndex());
    if (view)
        view->setFocus(Qt::TabFocusReason);
    else
        addTab();
}

void Controller::closeWindow()
{
    SharedPreferences *prefs = SharedPreferences::sharedInstance();
    if (!_window->tabs()->count() || !prefs->warnOnClose())
    {
        qApp->quit();
        return;
    }

    int count = 0;
    for (int i = 0; i < _window->tabs()->count(); i++)
    {
        if (viewInTab(i)->isConnected())
            count++;
    }

    if (count)
    {
        QMessageBox *sure = new QMessageBox(_window);
        sure->setIcon(QMessageBox::Warning);
        sure->setText(tr("Are you sure you want to quit Qelly?"));
        sure->setInformativeText(
            tr("There are %n tab(s) open in Qelly. Do you want to quit anyway?",
               "", count));
        sure->setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        sure->setWindowModality(Qt::WindowModal);
        sure->setFocus(Qt::PopupFocusReason);

        switch (sure->exec())
        {
        case QMessageBox::Ok:
            qApp->quit();
            break;
        case QMessageBox::Cancel:
            break;
        default:
            break;
        }
        sure->deleteLater();
    }
    else
    {
        qApp->quit();
    }
}

void Controller::copy()
{
    View *view = currentView();
    if (!view || !view->isConnected())
        return;

    view->copy();
}

void Controller::paste()
{
    View *view = currentView();
    if (!view || !view->isConnected())
        return;

    view->paste();
}

void Controller::pasteColor()
{
    View *view = currentView();
    if (!view || !view->isConnected())
        return;

    view->pasteColor();
}

void Controller::reconnect()
{
    View *view = currentView();
    if (!view)
        return;
    Connection::Terminal *terminal = view->terminal();
    if (!terminal)
        return;
    Connection::AbstractConnection *connection = terminal->connection();
    if (!connection)
        return;
    connection->reconnect();
}

void Controller::insertText(const QString &text)
{
    View *view = currentView();
    if (!view || !view->isConnected())
        return;
    view->insertText(text);
}

void Controller::onAddressReturnPressed()
{
    QString address = _window->address()->text();
    if (!address.size())
        return;

    if (!_window->tabs()->count() || currentView()->terminal())
    {
        int newTab = _window->tabs()->addTab(new Tab(new View()), "");
        _window->tabs()->setCurrentIndex(newTab);
    }
    connectWith(address);
}

void Controller::changeAddressField(const QString &address)
{
    _window->address()->setText(address);
}

void Controller::addCurrentSite()
{
    View *view = currentView();
    if (!view || !view->isConnected())
        return;
    Connection::Site *site = view->terminal()->connection()->site();
    if (!site)
        return;
    showSiteManager();
    _siteManager->addSite(site);
}

void Controller::showPreferencesWindow()
{
    if (!_preferencesWindow)
    {
        _preferencesWindow = new PreferencesWindow(_window);
        _preferencesWindow->setAttribute(Qt::WA_DeleteOnClose);
        connect(_preferencesWindow, SIGNAL(displayPreferenceChanged()),
                SLOT(updateAll()));
    }
    _preferencesWindow->setAttribute(Qt::WA_ShowModal);
    _preferencesWindow->show();
}

void Controller::showAbout()
{
    static const QString text =
            QString::fromUtf8(fromFile(":/data/about.html"));
    QMessageBox::about(_window, "About", text);
}

void Controller::showAcknowledgement()
{
    static const QString text =
            QString::fromUtf8(fromFile(":/data/acknowledgement.html"));
    QMessageBox::about(_window, "About", text);
}

void Controller::visitProject()
{
    QDesktopServices::openUrl(QUrl("https://github.com/uranusjr/Qelly"));
}

void Controller::toggleAntiIdle(bool enabled)
{
    SharedPreferences::sharedInstance()->setAntiIdleActive(enabled);
    setAntiIdleTimer(enabled);
}

void Controller::toggleShowHiddenText(bool enabled)
{
    SharedPreferences *prefs = SharedPreferences::sharedInstance();
    if (prefs->showHiddenText() != enabled)
    {
        prefs->setShowHiddenText(enabled);
        updateAll();
    }
}

void Controller::showEmoticonViewer()
{
    if (!_emoticonViewer)
    {
        _emoticonViewer = new EmoticonViewer(_window);
        _emoticonViewer->setAttribute(Qt::WA_DeleteOnClose);
        connect(_emoticonViewer, SIGNAL(hasTextToInsert(QString)),
                SLOT(insertText(QString)));
    }
    _emoticonViewer->setAttribute(Qt::WA_ShowModal);
    _emoticonViewer->show();
}

void Controller::showSiteManager()
{
    if (!_siteManager)
    {
        _siteManager = new SiteManagerDialog(_window);
        _siteManager->setAttribute(Qt::WA_DeleteOnClose);
        connect(_siteManager, SIGNAL(connectRequested(Connection::Site*)),
                SLOT(connectWith(Connection::Site*)));
    }
    _siteManager->setAttribute(Qt::WA_ShowModal);
    _siteManager->show();
}

void Controller::tabNext()
{
    QTabWidget *tabs = _window->tabs();
    tabs->setCurrentIndex((tabs->currentIndex() + 1) % tabs->count());
}

void Controller::tabPrevious()
{
    QTabWidget *tabs = _window->tabs();
    int next = tabs->currentIndex() - 1;
    if (next < 0)
        next += tabs->count();
    tabs->setCurrentIndex(next % tabs->count());
}

void Controller::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == _antiIdleTimer)
    {
        for (int i = 0; i < _window->tabs()->count(); i++)
        {
            Connection::Terminal *terminal = viewInTab(i)->terminal();
            if (!terminal || !terminal->isConnected())
                continue;

            QDateTime now = QDateTime::currentDateTime();
            Connection::AbstractConnection *conn = terminal->connection();
            if (conn->lastTouch().secsTo(now) >= 119)
                conn->sendBytes(QByteArray("\0\0\0\0\0\0", 6));
        }
    }
}

void Controller::updateAll()
{
    _window->tabs()->updateBackground();
    for (int i = 0; i < _window->tabs()->count(); i++)
    {
        View *view = viewInTab(i);
        if (!view)
            continue;
        view->updateCellSize();
        Connection::Terminal *terminal = view->terminal();
        if(!terminal || !terminal->isConnected())
            continue;
        terminal->setDirtyAll();
        view->updateBackImage();
        view->update();
    }
}

void Controller::minimize()
{
    _window->setWindowState(Qt::WindowMinimized);
}

void Controller::onTabChanged(int to)
{
    View *view = viewInTab(to);
    if (view)
        changeAddressField(view->address());
}

View *Controller::currentView() const
{
    return static_cast<Tab *>(_window->tabs()->currentWidget())->view();
}

View *Controller::viewInTab(int index) const
{
    QWidget *w = _window->tabs()->widget(index);
    if (!w)
        return 0;
    return static_cast<Tab *>(w)->view();
}

void Controller::setAntiIdleTimer(bool enabled)
{
    if (enabled)
    {
        _antiIdleTimer = startTimer(2 * 60 * 1000);
    }
    else
    {
        killTimer(_antiIdleTimer);
        _antiIdleTimer = 0;
    }
}

}   // namespace Qelly

}   // namespace UJ
