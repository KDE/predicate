/* This file is part of the KDE project
   Copyright (C) 2003 Adam Pigg <adam@piggz.co.uk>

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

#ifndef PQXXCONNECTION_H
#define PQXXCONNECTION_H

#include <qstringlist.h>

#include <Predicate/Connection.h>
#include "PqxxCursor.h"


namespace Predicate
{

class pqxxSqlConnectionInternal;

//! @internal
class pqxxTransactionData : public TransactionData
{
public:
    pqxxTransactionData(Connection *conn, bool nontransaction);
    ~pqxxTransactionData();
    pqxx::transaction_base *data;
};

/**
@author Adam Pigg
*/
class pqxxSqlConnection : public Connection
{
public:
    virtual ~pqxxSqlConnection();

    virtual Cursor* prepareQuery(const EscapedString& statement, uint cursor_options = 0);
    virtual Cursor* prepareQuery(QuerySchema* query, uint cursor_options = 0);

    virtual PreparedStatementInterface* prepareStatementInternal();

protected:
    /*! Used by driver */
    pqxxSqlConnection(Driver *driver, const ConnectionData& connData);

    virtual bool drv_isDatabaseUsed() const;
    virtual bool drv_connect(Predicate::ServerVersionInfo* version);
    virtual bool drv_disconnect();
    virtual bool drv_getDatabasesList(QStringList* list);
    virtual bool drv_createDatabase(const QString &dbName = QString());
    virtual bool drv_useDatabase(const QString &dbName = QString(), bool *cancelled = 0,
                                 MessageHandler* msgHandler = 0);
    virtual bool drv_closeDatabase();
    virtual bool drv_dropDatabase(const QString &dbName = QString());
    virtual bool drv_executeSQL(const EscapedString& statement);
    virtual quint64 drv_lastInsertRecordId();

//TODO: move this somewhere to low level class (MIGRATION?)
    virtual bool drv_getTablesList(QStringList* list);
//TODO: move this somewhere to low level class (MIGRATION?)
    virtual bool drv_containsTable(const QString &tableName);

    virtual TransactionData* drv_beginTransaction();
    virtual bool drv_commitTransaction(TransactionData *);
    virtual bool drv_rollbackTransaction(TransactionData *);

    //! Implemented for Resultable
    virtual QString serverResultName() const;
//    virtual void drv_clearServerResult();

    pqxxSqlConnectionInternal *d;
private:
    QString escapeName(const QString &tn) const;
    //   pqxx::transaction_base* m_trans;
    //! temporary solution for executeSQL()...
    pqxxTransactionData *m_trans;



    friend class PqxxSqlDriver;
    friend class PqxxSqlCursor;
    friend class PqxxTransactionData;
};
}
#endif
