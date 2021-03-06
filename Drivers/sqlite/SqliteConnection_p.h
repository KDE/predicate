/* This file is part of the KDE project
   Copyright (C) 2003-2012 Jarosław Staniek <staniek@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#ifndef PREDICATE_SQLITECONN_P_H
#define PREDICATE_SQLITECONN_P_H

#include <Predicate/Private/Connection>

#include <sqlite3.h>

namespace Predicate
{

/*! Internal SQLite connection data. Also used by SqliteCursor. */
class SQLiteConnectionInternal : public ConnectionInternal
{
public:
    explicit SQLiteConnectionInternal(Connection* connection);
    virtual ~SQLiteConnectionInternal();

    virtual void storeResult();

    //! @return true is loading extensions is enabled
    bool extensionsLoadingEnabled() const;

    //! Sets loading extensions flag to @a set
    void setExtensionsLoadingEnabled(bool set);

    static QString serverResultName(int serverResultCode);

    sqlite3 *data;
    bool data_owned; //!< true if data pointer should be freed on destruction

private:
    bool m_extensionsLoadingEnabled;
};

}

#endif
