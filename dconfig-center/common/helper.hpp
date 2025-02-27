// SPDX-FileCopyrightText: 2021 - 2023 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QString>
#include <QSet>
#include <QList>
#include <QDir>
#include <QDirIterator>
#include <QDBusArgument>
#include <QJsonDocument>
#include <DConfigFile>

using ResourceId = QString;
using AppId = QString;
using SubpathKey = QString;
using ResourceList = QList<ResourceId>;
using AppList = QList<AppId>;
using SubpathList = QList<SubpathKey>;

static const QString &SUFFIX = QString(".json");
constexpr int ConfigUserRole = Qt::UserRole + 10;
constexpr int AppidRole = Qt::UserRole + 11;
constexpr int ResourceRole = Qt::UserRole + 12;
constexpr int SubpathRole = Qt::UserRole + 13;
constexpr int KeyRole = Qt::UserRole + 14;
constexpr int ValueRole = Qt::UserRole + 15;
constexpr int DescriptionRole = Qt::UserRole + 16;
static const QString NoAppId;
static const QString VirtualAppName("virtual-generic-applicaiton");

enum ConfigType {
    InvalidType = 0x00,
    AppType = 0x10,
    ResourceType = 0x20,
    AppResourceType = ResourceType | 0x01,
    CommonResourceType = ResourceType | 0x02,
    SubpathType = 0x30,
    KeyType = 0x40,
};

static AppList applications(const QString &localPrefix = QString())
{
    AppList result;
    result << NoAppId;

    result.reserve(50);

    // TODO calling service interface to get app list,
    // and now we can't distingush between `subpath` or `appid` for common configuration.
    using namespace Dtk::Core;
    QStringList appDirs = DConfigMeta::genericMetaDirs(localPrefix);
    const QStringList filterDirs {"overrides"};
    for (auto item : appDirs)
    {
        QDir appsDir(item);

        for (auto appid : appsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (filterDirs.contains(appid))
                continue;

            result.append(appid);
        }
    }
    return result;
}

static QSet<ResourceId> resourcesForDirectory(const QString &dir)
{
    QSet<ResourceId> result;
    QDirIterator iterator(dir, QDir::Files);
    while(iterator.hasNext()) {
        iterator.next();
        const QFileInfo &file(iterator.fileInfo());

        if (!file.fileName().endsWith(SUFFIX))
            continue;

        ResourceId resourceName = file.fileName().chopped(SUFFIX.size());
        result.insert(resourceName);
    }
    return result;
}

static ResourceList resourcesForApp(const QString &appid, const QString &localPrefix = QString())
{
    QSet<ResourceId> result;
    result.reserve(50);
    using namespace Dtk::Core;
    for (auto item : DConfigMeta::applicationMetaDirs(localPrefix, appid)) {
        result += resourcesForDirectory(item);
    }
    return result.values();
}

static ResourceList resourcesForAllApp(const QString &localPrefix = QString())
{
    QSet<ResourceId> result;
    result.reserve(50);
    using namespace Dtk::Core;
    for (auto item : DConfigMeta::genericMetaDirs(localPrefix)) {
        result += resourcesForDirectory(item);
    }
    return result.values();
}

static ResourceList subpathsForResource(const AppId &appid, const ResourceId &resourceId, const QString &localPrefix = QString())
{
    SubpathList result;
    for (auto item : resourcesForApp(appid, localPrefix)) {
        QDir resourceDir(item);
        auto filters = QDir::Dirs | QDir::NoDotAndDotDot;
        resourceDir.setFilter(filters);
        QDirIterator iterator(resourceDir, QDirIterator::Subdirectories);

        while(iterator.hasNext()) {
            iterator.next();
            const QFileInfo &file(iterator.fileInfo());
            if (QDir(file.absoluteFilePath()).exists(resourceId + SUFFIX)) {
                auto subpath = file.absoluteFilePath().replace(resourceDir.absolutePath(), "");
                result.append(subpath);
            }
        }
    }
    return result;
}

static bool existAppid(const QString &appid, const QString &localPrefix = QString())
{
    return !resourcesForApp(appid, localPrefix).isEmpty();
}

static bool existResource(const AppId &appid, const ResourceId &resourceId, const QString &localPrefix = QString())
{
    const ResourceList &apps = resourcesForApp(appid, localPrefix);
    if (apps.contains(resourceId))
        return true;

    const ResourceList &commons = resourcesForAllApp(localPrefix);
    if (commons.contains(resourceId)) {
        return true;
    }

    return false;
}

static QVariant decodeQDBusArgument(const QVariant &v)
{
    if (v.canConvert<QDBusArgument>()) {
        // we use QJsonValue to resolve all data type in DConfigInfo class, so it's type is equal QJsonValue::Type,
        // now we parse Map and Array type to QVariant explicitly.
        const QDBusArgument &complexType = v.value<QDBusArgument>();
        switch (complexType.currentType()) {
        case QDBusArgument::MapType: {
            QVariantMap list;
            complexType >> list;
            QVariantMap res;
            for (auto iter = list.begin(); iter != list.end(); iter++) {
                res[iter.key()] = decodeQDBusArgument(iter.value());
            }
            return res;
        }
        case QDBusArgument::ArrayType: {
            QVariantList list;
            complexType >> list;
            QVariantList res;
            res.reserve(list.size());
            for (auto iter = list.begin(); iter != list.end(); iter++) {
                res << decodeQDBusArgument(*iter);
            }
            return res;
        }
        default:
            qWarning("Can't parse the type, it maybe need user to do it, "
                     "QDBusArgument::ElementType: %d.", complexType.currentType());
        }
    }
    return v;
}

static QString qvariantToString(const QVariant &v)
{
    const auto &doc = QJsonDocument::fromVariant(v);
    return doc.isNull() ? v.toString() : doc.toJson();
}

static QVariant stringToQVariant(const QString &s)
{
    QJsonParseError error;
    const auto &doc = QJsonDocument::fromJson(s.toUtf8(), &error);
    if (error.error == QJsonParseError::NoError)
        return doc.toVariant();
    return s;
}
