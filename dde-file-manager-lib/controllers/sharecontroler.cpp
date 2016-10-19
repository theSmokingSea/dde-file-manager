/**
 * Copyright (C) 2016 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include "sharecontroler.h"
#include "models/sharefileinfo.h"
#include "dfileinfo.h"

#include "usershare/shareinfo.h"
#include "usershare/usersharemanager.h"
#include "widgets/singleton.h"
#include "app/define.h"
#include "dfileservices.h"


ShareControler::ShareControler(QObject *parent) :
    DAbstractFileController(parent)
{
    connect(userShareManager, &UserShareManager::userShareAdded, this, [=](const QString& filePath){
        emit childrenAdded(DUrl::fromUserShareFile(filePath));
    });
    connect(userShareManager, &UserShareManager::userShareDeleted, this, [=](const QString& filePath){
        emit childrenRemoved(DUrl::fromUserShareFile(filePath));
    });

}

const DAbstractFileInfoPointer ShareControler::createFileInfo(const DUrl &fileUrl, bool &accepted) const
{
    accepted = true;

    return DAbstractFileInfoPointer(new ShareFileInfo(fileUrl));
}

const QList<DAbstractFileInfoPointer> ShareControler::getChildren(const DUrl &fileUrl, const QStringList &nameFilters, QDir::Filters filters, QDirIterator::IteratorFlags flags, bool &accepted) const
{
    Q_UNUSED(filters)
    Q_UNUSED(nameFilters)
    Q_UNUSED(flags)
    Q_UNUSED(fileUrl)

    accepted = true;

    QList<DAbstractFileInfoPointer> infolist;

    ShareInfoList sharelist = userShareManager->shareInfoList();
    foreach (ShareInfo shareInfo, sharelist) {
        DAbstractFileInfoPointer fileInfo = createFileInfo(DUrl::fromUserShareFile(shareInfo.path()), accepted);
        if(fileInfo->exists())
            infolist << fileInfo;
    }

    return infolist;
}

void ShareControler::onFileInfoChanged(const QString &filePath)
{
    const DUrl &url = DUrl::fromLocalFile(filePath);
    emit childrenUpdated(url);
}
