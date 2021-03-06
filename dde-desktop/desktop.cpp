/**
 * Copyright (C) 2016 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include "desktop.h"

#include <QDebug>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QDBusConnection>
#include <QScreen>

#include "presenter/apppresenter.h"
#include "../dde-wallpaper-chooser/frame.h"
#include "../util/dde/desktopinfo.h"
#include "backgroundmanager.h"
#include "canvasviewmanager.h"
#include "screen/screenhelper.h"
#include "presenter/gridmanager.h"

#ifndef DISABLE_ZONE
#include "../dde-zone/mainwindow.h"
#endif
#include <malloc.h>

using WallpaperSettings = Frame;

#ifndef DISABLE_ZONE
using ZoneSettings = ZoneMainWindow;
#endif

class DesktopPrivate
{
public:
    ~DesktopPrivate(){
        if (m_background){
            delete m_background;
            m_background = nullptr;
        }

        if (m_canvas){
            delete m_canvas;
            m_canvas = nullptr;
        }

        if (wallpaperSettings){
            delete wallpaperSettings;
            wallpaperSettings = nullptr;
        }
#ifndef DISABLE_ZONE
        if (zoneSettings){
            delete zoneSettings;
            zoneSettings = nullptr;
        }
#endif
    }
    BackgroundManager *m_background = nullptr;
    CanvasViewManager *m_canvas = nullptr;
    WallpaperSettings *wallpaperSettings{ nullptr };

#ifndef DISABLE_ZONE
    ZoneSettings *zoneSettings { nullptr };
#endif
};

Desktop::Desktop()
    : d(new DesktopPrivate)
{

}

Desktop::~Desktop()
{

}

void Desktop::preInit()
{
    d->m_background = new BackgroundManager;
    //    //5s归还一次内存
    //    QTimer *releaseMem = new QTimer;
    //    connect(releaseMem,&QTimer::timeout,this,[](){malloc_trim(0);});
    //    releaseMem->start(5000);
}

void Desktop::loadData()
{
    Presenter::instance()->init();
}

void Desktop::loadView()
{
    d->m_canvas = new CanvasViewManager(d->m_background);
}

void Desktop::showWallpaperSettings(QString name, int mode)
{
    if(name.isNull() || name.isEmpty()){
        if (ScreenHelper::screenManager()->primaryScreen() == nullptr){
            qCritical() << "get primary screen failed! stop show wallpaper";
            return;
        }

        name = ScreenHelper::screenManager()->primaryScreen()->name();
    }

    if (d->wallpaperSettings) {
        //防止暴力操作，高频调用接口
        if (d->wallpaperSettings->isVisible())
            return;
        d->wallpaperSettings->deleteLater();
        d->wallpaperSettings = nullptr;
    }

    d->wallpaperSettings = new WallpaperSettings(name, Frame::Mode(mode));
    connect(d->wallpaperSettings, &Frame::done, this, [ = ] {
        d->wallpaperSettings->deleteLater();
        d->wallpaperSettings = nullptr;
    });


    connect(d->wallpaperSettings, &Frame::aboutHide, this, [this] {
        WallpaperSettings *setting = dynamic_cast<WallpaperSettings *>(sender());
        if (setting){
            QPair<QString, QString> screenImage = setting->desktopBackground();
            d->m_background->setBackgroundImage(screenImage.first, screenImage.second);
        }
    }, Qt::DirectConnection);


    d->wallpaperSettings->show();

    //监控窗口状态
    QWindow *window = d->wallpaperSettings->windowHandle();
    connect(window, &QWindow::activeChanged, d->wallpaperSettings, [=]() {
        if(d->wallpaperSettings == nullptr || d->wallpaperSettings->isActiveWindow())
            return;
        //激活窗口
        d->wallpaperSettings->activateWindow();
        //10毫秒后再次检测
        QTimer::singleShot(10,d->wallpaperSettings,[=]()
        {
            if (d->wallpaperSettings && !d->wallpaperSettings->isActiveWindow())
                d->wallpaperSettings->windowHandle()->activeChanged();
        });
    });
}


void Desktop::showZoneSettings()
{
#ifndef DISABLE_ZONE
    if (d->zoneSettings) {
        d->zoneSettings->deleteLater();
        d->zoneSettings = nullptr;
    }

    d->zoneSettings = new ZoneSettings;
    connect(d->zoneSettings, &ZoneMainWindow::finished, this, [ = ] {
        d->zoneSettings->deleteLater();
        d->zoneSettings = nullptr;
    });

    d->zoneSettings->show();
    d->zoneSettings->grabKeyboard();
#else
    qWarning() << "Zone is disabled";
#endif
}


void Desktop::EnableUIDebug(bool enable)
{
    for (CanvasViewPointer view: d->m_canvas->canvas().values()){
        view->EnableUIDebug(enable);
        view->update();
    }
}

void Desktop::SetVisible(int screenNum, bool v)
{
    --screenNum;
    QVector<ScreenPointer> screens = ScreenMrg->logicScreens();
    if (screens.size() > screenNum && screenNum >= 0){
        ScreenPointer sp = screens[screenNum];
        BackgroundWidgetPointer bw = d->m_background->allbackgroundWidgets().value(sp);
        if (bw)
            bw->setVisible(v);

        CanvasViewPointer view = d->m_canvas->canvas().value(sp);
        if (view)
            view->setVisible(v);
    }
}

void Desktop::FixGeometry(int screenNum)
{
    --screenNum;
    QVector<ScreenPointer> screens = ScreenMrg->logicScreens();
    if (screens.size() > screenNum && screenNum >= 0){
        ScreenPointer sp = screens[screenNum];
        emit ScreenMrg->sigScreenGeometryChanged();
    }
}

void Desktop::Reset()
{
    ScreenMrg->reset();
    if (d->m_background->isEnabled()){
        d->m_background->onBackgroundBuild();
    }
    else {
        d->m_background->onSkipBackgroundBuild();
    }
}

void Desktop::PrintInfo()
{
    ScreenPointer primary = ScreenMrg->primaryScreen();
    qInfo() << "**************Desktop Info" << qApp->applicationVersion()
            << "*****************";
    if (primary)
        qInfo() << "primary screen :" << primary->name()
                << "available geometry" << primary->availableGeometry()
                << "handle geometry"   << primary->handleGeometry()
                << "devicePixelRatio" << ScreenMrg->devicePixelRatio()
                << "screen count" << ScreenMrg->screens().count();
    else
        qCritical() << "primary screen error! not found";

    qInfo() << "*****************Screens  Mode " << ScreenMrg->displayMode()
            << "********************";
    int num = 1;
    for (ScreenPointer screen : ScreenMrg->logicScreens()){
        if (screen){
            qInfo() << screen.get() << "screen name " << screen->name()
                    << "num" << num << "geometry" << screen->geometry()
                    << "handle geometry"   << screen->handleGeometry();
            ++num;
        }else {
            qCritical() << "error! empty screen pointer!";
        }
    }

    qInfo() << "*****************Background Eable" << d->m_background->isEnabled()
            << "**********************";
    auto backgronds = d->m_background->allbackgroundWidgets();
    for (auto iter = backgronds.begin(); iter != backgronds.end(); ++iter) {
        qInfo() << "Background" << iter.value().get() << "on screen" << iter.key()->name() << iter.key().get()
                << "geometry" << iter.value()->geometry() << "visable" << iter.value()->isVisible()
                << "rect" << iter.value()->rect() << "background image"
                << d->m_background->backgroundImages().value(iter.key()->name())
                << "pixmap" << iter.value()->pixmap();

        if (iter.value()->windowHandle()){
            qInfo() << "window geometry" << iter.value()->windowHandle()->geometry()
                << iter.value()->windowHandle()->screen()->geometry();
        }
    }

    qInfo() << "*****************Canvas Grid" << "**********************";
    if (d->m_canvas){
        auto canvas = d->m_canvas->canvas();
        GridCore *core = GridManager::instance()->core();
        for (auto iter = canvas.begin(); iter != canvas.end(); ++iter){
            int num = iter.value()->screenNum();
            qInfo() << "canvas" << iter.value().get() << "on screen" << iter.value()->canvansScreenName()
                    << "num" << num << "geometry" << iter.value()->geometry()
                    << "background" << iter.value()->parentWidget() << "screen" << iter.key().get();
            if (core->screensCoordInfo.contains(num)){
                auto coord = core->screensCoordInfo.value(num);
                qInfo() << "coord " << coord.first << "*" << coord.second
                        << "display items count" << core->itemGrids.value(num).size();
            }
            else {
                qCritical() << "Grid" << iter.value()->screenNum() << "not find coordinfo";
            }

        }

        qInfo() << "overlap items count" << core->overlapItems.size();
        delete core;
    }
    else {
        qWarning() << "not load canvasgridview";
    }
    qInfo() << "************Desktop Infomation End **************";
}

void Desktop::Refresh()
{
    if (d->m_canvas)
        for (CanvasViewPointer view: d->m_canvas->canvas().values()){
            view->Refresh();
        }
}

void Desktop::ShowWallpaperChooser(const QString &screen)
{
    showWallpaperSettings(screen,Frame::WallpaperMode);
}

void Desktop::ShowScreensaverChooser(const QString &screen)
{
    showWallpaperSettings(screen,Frame::ScreenSaverMode);
}

QList<int> Desktop::GetIconSize()
{
    QSize iconSize{0,0};
    if (d->m_canvas && !d->m_canvas->canvas().isEmpty())
        iconSize = d->m_canvas->canvas().first()->iconSize();
    QList<int> size{iconSize.width(),iconSize.height()};
    return  size;
}
