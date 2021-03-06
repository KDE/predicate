/* This file is part of the KDE project
   Copyright (C) 2003 Adam Pigg <adam@piggz.co.uk>
   Copyright (C) 2010 Jarosław Staniek <staniek@kde.org>

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

#ifndef PREDICATE_DRIVER_POSTGRESQLCURSOR_H
#define PREDICATE_DRIVER_POSTGRESQLCURSOR_H

#include <Predicate/Cursor>
#include <Predicate/Connection>
#include <Predicate/Utils>

namespace Predicate
{

class PostgresqlCursorData;

class PostgresqlCursor: public Cursor
{
public:
    explicit PostgresqlCursor(Connection* conn, const EscapedString& statement,
                              uint options = NoOptions);
    PostgresqlCursor(Connection* conn, QuerySchema* query, uint options = NoOptions);
    virtual ~PostgresqlCursor();

    virtual QVariant value(uint pos);
    virtual const char** recordData() const;
    virtual bool drv_storeCurrentRecord(RecordData* data) const;
    virtual bool drv_open(const EscapedString& sql);
    virtual bool drv_close();
    virtual void drv_getNextRecord();
    virtual void drv_appendCurrentRecordToBuffer();
    virtual void drv_bufferMovePointerNext();
    virtual void drv_bufferMovePointerPrev();
    virtual void drv_bufferMovePointerTo(qint64 to);

private:
    QVariant pValue(uint pos)const;

    unsigned long m_numRows;
    QVector<QVariant::Type> m_realTypes;

    PostgresqlCursorData *d;
};

}

#endif // PREDICATE_DRIVER_POSTGRESQLCURSOR_H
