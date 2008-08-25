/* This file is part of the KDE project
   Copyright (C) 2005-2008 Jarosław Staniek <staniek@kde.org>

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

#include "SqlitePreparedStatement.h"

#include <QtDebug>
#include <assert.h>

using namespace Predicate;

SQLitePreparedStatement::SQLitePreparedStatement(ConnectionInternal& conn)
        : PreparedStatementInterface()
        , SQLiteConnectionInternal(conn.connection)
        , prepared_st_handle(0)
        , m_resetRequired(false)
{
    data_owned = false;
    data = dynamic_cast<Predicate::SQLiteConnectionInternal&>(conn).data; //copy
}

bool SQLitePreparedStatement::prepare(const QByteArray& statement)
{
    res = sqlite3_prepare(
              data, /* Database handle */
              temp_st, //const char *zSql,       /* SQL statement, UTF-8 encoded */
              temp_st.length(), //int nBytes,             /* Length of zSql in bytes. */
              &prepared_st_handle, //sqlite3_stmt **ppStmt,  /* OUT: Statement handle */
              0 //const char **pzTail     /* OUT: Pointer to unused portion of zSql */
          );
    if (SQLITE_OK == res)
        return true;

//! @todo copy error msg
    return false
}

SQLitePreparedStatement::~SQLitePreparedStatement()
{
    sqlite3_finalize(prepared_st_handle);
    prepared_st_handle = 0;
}

bool SQLitePreparedStatement::execute(const Arguments& args)
{
    if (!prepared_st_handle)
        return false;
    if (m_resetRequired) {
        res = sqlite3_reset(prepared_st_handle);
        if (SQLITE_OK != res) {
            //! @todo msg?
            return false;
        }
        m_resetRequired = false;
    }

    //for INSERT, we're iterating over inserting values
    //for SELECT, we're iterating over WHERE conditions
    const Field::List *fieldList = 0;
    if (d->type == SelectStatement)
        fieldList = &d->whereFields;
    else if (d->type == InsertStatement)
        fieldList = d->fields->fields();
    else
        assert(0); //impl. error

    int arg = 1; //arg index counted from 1
    Field::ListIterator itFields(fieldList->constBegin());
    for (QList<QVariant>::ConstIterator it = args.constBegin();
            itFields != fieldList->constEnd(); ++it, ++itFields, arg++) {
        Predicate::Field *field = *itFields;
        if (it == args.constEnd() || (*it).isNull()) {//no value to bind or the value is null: bind NULL
            res = sqlite3_bind_null(prepared_st_handle, arg);
            if (SQLITE_OK != res) {
                //! @todo msg?
                return false;
            }
            continue;
        }
        if (field->isTextType()) {
            //! @todo optimize: make a static copy so SQLITE_STATIC can be used
            QByteArray utf8String((*it).toString().toUtf8());
            res = sqlite3_bind_text(prepared_st_handle, arg,
                                    (const char*)utf8String, utf8String.length(), SQLITE_TRANSIENT /*??*/);
            if (SQLITE_OK != res) {
                //! @todo msg?
                return false;
            }
        } else {
            switch (field->type()) {
            case Predicate::Field::Byte:
            case Predicate::Field::ShortInteger:
            case Predicate::Field::Integer: {
                //! @todo what about unsigned > INT_MAX ?
                bool ok;
                const int value = (*it).toInt(&ok);
                if (ok) {
                    res = sqlite3_bind_int(prepared_st_handle, arg, value);
                    if (SQLITE_OK != res) {
                        //! @todo msg?
                        return false;
                    }
                } else {
                    res = sqlite3_bind_null(prepared_st_handle, arg);
                    if (SQLITE_OK != res) {
                        //! @todo msg?
                        return false;
                    }
                }
                break;
            }
            case Predicate::Field::Float:
            case Predicate::Field::Double:
                res = sqlite3_bind_double(prepared_st_handle, arg, (*it).toDouble());
                if (SQLITE_OK != res) {
                    //! @todo msg?
                    return false;
                }
                break;
            case Predicate::Field::BigInteger: {
                //! @todo what about unsigned > LLONG_MAX ?
                bool ok;
                qint64 value = (*it).toLongLong(&ok);
                if (ok) {
                    res = sqlite3_bind_int64(prepared_st_handle, arg, value);
                    if (SQLITE_OK != res) {
                        //! @todo msg?
                        return false;
                    }
                } else {
                    res = sqlite3_bind_null(prepared_st_handle, arg);
                    if (SQLITE_OK != res) {
                        //! @todo msg?
                        return false;
                    }
                }
                break;
            }
            case Predicate::Field::Boolean:
                res = sqlite3_bind_text(prepared_st_handle, arg,
                                        QString::number((*it).toBool() ? 1 : 0).toLatin1(),
                                        1, SQLITE_TRANSIENT /*??*/);
                if (SQLITE_OK != res) {
                    //! @todo msg?
                    return false;
                }
                break;
            case Predicate::Field::Time:
                res = sqlite3_bind_text(prepared_st_handle, arg,
                                        (*it).toTime().toString(Qt::ISODate).toLatin1(),
                                        sizeof("HH:MM:SS"), SQLITE_TRANSIENT /*??*/);
                if (SQLITE_OK != res) {
                    //! @todo msg?
                    return false;
                }
                break;
            case Predicate::Field::Date:
                res = sqlite3_bind_text(prepared_st_handle, arg,
                                        (*it).toDate().toString(Qt::ISODate).toLatin1(),
                                        sizeof("YYYY-MM-DD"), SQLITE_TRANSIENT /*??*/);
                if (SQLITE_OK != res) {
                    //! @todo msg?
                    return false;
                }
                break;
            case Predicate::Field::DateTime:
                res = sqlite3_bind_text(prepared_st_handle, arg,
                                        (*it).toDateTime().toString(Qt::ISODate).toLatin1(),
                                        sizeof("YYYY-MM-DDTHH:MM:SS"), SQLITE_TRANSIENT /*??*/);
                if (SQLITE_OK != res) {
                    //! @todo msg?
                    return false;
                }
                break;
            case Predicate::Field::BLOB: {
                const QByteArray byteArray((*it).toByteArray());
                res = sqlite3_bind_blob(prepared_st_handle, arg,
                                        (const char*)byteArray, byteArray.size(), SQLITE_TRANSIENT /*??*/);
                if (SQLITE_OK != res) {
                    //! @todo msg?
                    return false;
                }
                break;
            }
            default:
                PreWarn << "PreparedStatement::execute(): unsupported field type: "
                << field->type() << " - NULL value bound to column #" << arg << endl;
                res = sqlite3_bind_null(prepared_st_handle, arg);
                if (SQLITE_OK != res) {
                    //! @todo msg?
                    return false;
                }
            } //switch
        }//else
    }//for

    //real execution
    res = sqlite3_step(prepared_st_handle);
    m_resetRequired = true;
    if (d->type == InsertStatement && res == SQLITE_DONE) {
        return true;
    }
    if (d->type == SelectStatement) {
        //fetch result

        //todo
    }
    return false;
}
