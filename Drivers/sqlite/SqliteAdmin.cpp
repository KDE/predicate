/* This file is part of the KDE project
   Copyright (C) 2006-2012 Jarosław Staniek <staniek@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#include <QDir>

#include "SqliteAdmin.h"
#include "SqliteVacuum.h"

#include <Predicate/DriverManager>
#include <Predicate/Private/Driver>

SQLiteAdminTools::SQLiteAdminTools()
        : Predicate::AdminTools()
{
}

SQLiteAdminTools::~SQLiteAdminTools()
{
}

#ifdef PREDICATE_SQLITE_VACUUM
bool SQLiteAdminTools::vacuum(const Predicate::ConnectionData& data, const QString& databaseName)
{
    clearResult();
    Predicate::DriverManager manager;
    Predicate::Driver *drv = manager.driver(data.driverName());
    QString title(QObject::tr("Could not compact database \"%1\".").arg(QDir::convertSeparators(databaseName)));
    if (!drv) {
        m_result = Predicate::Result(title);
        return false;
    }
    QFileInfo file(databaseName);
    SQLiteVacuum vacuum(QDir::convertSeparators(file.absoluteFilePath()));
    tristate result = vacuum.run();
    if (false == result) {
        m_result = Predicate::Result(title);
        return false;
    } else { //success or cancelled
        return true;
    }
}
#endif
