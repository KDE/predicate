/* This file is part of the KDE project
Copyright (C) 2007   Sharan Rao <sharanrao@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public License
along with this program; see the file COPYING. If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#ifndef SYBASEDB_H
#define SYBASEDB_H

#include <Predicate/Driver.h>

namespace Predicate
{

//! Sybase database driver.
class SybaseDriver : public Driver
{
    Q_OBJECT
    PREDICATE_DRIVER

public:
    /*!
     * Constructor sets database features and
     * maps the types in KexiDB::Field::Type to the Sybase types.
     */
    SybaseDriver();

    virtual ~SybaseDriver();

    virtual bool isSystemDatabaseName(const QString &n) const;

    //! Escape a string for use as a value
    virtual EscapedString escapeString(const QString& str) const;
    virtual EscapedString escapeString(const QByteArray& str) const;

    //! Escape BLOB value \a array
    virtual EscapedString escapeBLOB(const QByteArray& array) const;

protected:
    virtual QByteArray drv_escapeIdentifier(const QString& str) const;
    virtual QByteArray drv_escapeIdentifier(const QByteArray& str) const;
    virtual Connection *drv_createConnection(const ConnectionData& connData);
    virtual bool drv_isSystemFieldName(const QString& n) const;
    virtual EscapedString addLimitTo1(const QString& sql, bool add);

private:
    static const char *keywords[];
};
}

#endif
