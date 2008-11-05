/* This file is part of the KDE project
   Copyright (C) 2003-2007 Jarosław Staniek <staniek@kde.org>

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

#include "Connection.h"

#include "Error.h"
#include "Connection_p.h"
#include "ConnectionData.h"
#include "Driver.h"
#include "Driver_p.h"
#include "SchemaData.h"
#include "TableSchema.h"
#include "Relationship.h"
#include "Transaction.h"
#include "Cursor.h"
#include <Predicate/Global.h>
#include "RowEditBuffer.h"
#include "Utils.h"
#include "DbProperties.h"
#include "LookupFieldSchema.h"
#include "parser/Parser.h"

#include "tools/Utils.h"
#include "tools/identifier.h"
#include "Interfaces/PreparedStatementInterface.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QtXML>
#include <QtDebug>

#define PREDICATE_EXTENDED_TABLE_SCHEMA_VERSION 1

//#define PREDICATE_LOOKUP_FIELD_TEST

namespace Predicate
{

Connection::SelectStatementOptions::SelectStatementOptions()
        : identifierEscaping(Driver::EscapeDriver | Driver::EscapeAsNecessary)
        , alsoRetrieveROWID(false)
        , addVisibleLookupColumns(true)
{
}

Connection::SelectStatementOptions::~SelectStatementOptions()
{
}

//================================================

ConnectionInternal::ConnectionInternal(Connection *conn)
        : connection(conn)
{
}

ConnectionInternal::~ConnectionInternal()
{
}

//================================================
//! @internal
class ConnectionPrivate
{
public:
    ConnectionPrivate(Connection* const conn, ConnectionData &conn_data)
            : conn(conn)
            , conn_data(&conn_data)
            , m_parser(0)
            , dont_remove_transactions(false)
            , skip_databaseExists_check_in_useDatabase(false)
            , default_trans_started_inside(false)
            , isConnected(false)
            , autoCommit(true)
            , takeTableEnabled(true) {
//Qt 4   tableSchemaChangeListeners.setAutoDelete(true);
//Qt 4   obsoleteQueries.setAutoDelete(true);

//Qt 4   tables.setAutoDelete(true);
//Qt 4   predicateSystemTables.setAutoDelete(true);//only system tables
//Qt 4   queries.setAutoDelete(true);
//Qt 4   queries_byname.setAutoDelete(false);//queries is owner, not me

        //reasonable sizes: TODO
        //Qt 4 tables.resize(101);
        //Qt 4 queries.resize(101);
    }
    ~ConnectionPrivate() {
        qDeleteAll(cursors);
        delete m_parser;
    }

    void errorInvalidDBContents(const QString& details) {
        conn->setError(ERR_INVALID_DATABASE_CONTENTS, QObject::tr("Invalid database contents. %1")
            .arg(details));
    }

    QString strItIsASystemObject() const {
        return QObject::tr("It is a system object.");
    }

    inline Parser *parser() {
        return m_parser ? m_parser : (m_parser = new Parser(conn));
    }

    inline TableSchema* table(const QString& name) const {
        return tables_byname.value(name);
    }

    inline TableSchema* table(int id) const {
        return tables.value(id);
    }

    //! used just for removing system TableSchema objects on db close.
    inline const QSet<TableSchema*>& predicateSystemTables() const {
        return _predicateSystemTables;
    }

    inline void insertTable(TableSchema& tableSchema) {
        tables.insert(tableSchema.id(), &tableSchema);
        tables_byname.insert(tableSchema.name().toLower(), &tableSchema);
    }

    /*! @internal. Inserts internal table to Connection's structures, so it can be found by name.
     Used by Connection::insertInternalTable(TableSchema&) */
    inline void insertInternalTable(TableSchema& tableSchema) {
        tableSchema.setKexiDBSystem(true);
        _predicateSystemTables.insert(&tableSchema);
        tables_byname.insert(tableSchema.name().toLower(), &tableSchema);
    }

    /*! @internal Removes table schema pointed by tableSchema.id() and tableSchema.name()
     from internal structures and destroys it. Does not make any change at the backend.
     Note that the table schema being removed may be not the same as @a tableSchema. */
    inline void removeTable(const TableSchema& tableSchema) {
        tables_byname.remove(tableSchema.name());
        TableSchema *toDelete = tables.take(tableSchema.id());
        delete toDelete;
    }

    inline void takeTable(TableSchema& tableSchema) {
        if (!takeTableEnabled)
            return;
        tables.take(tableSchema.id());
        tables_byname.take(tableSchema.name());
    }

    inline void renameTable(TableSchema& tableSchema, const QString& newName) {
        tables_byname.take(tableSchema.name());
        tableSchema.setName(newName.toLower());
        tables_byname.insert(tableSchema.name(), &tableSchema);
    }

    inline void changeTableId(TableSchema& tableSchema, int newId) {
        tables.take(tableSchema.id());
        tables.insert(newId, &tableSchema);
    }

    inline void clearTables() {
        tables_byname.clear();
        qDeleteAll(_predicateSystemTables);
        _predicateSystemTables.clear();
        takeTableEnabled = false; //!< needed because otherwise 'tables' hash will
        //!< be touched by takeTable() what's not allowed during qDeleteAll()
        qDeleteAll(tables);
        takeTableEnabled = true;
        tables.clear();
    }

    inline QuerySchema* query(const QString& name) const {
        return queries_byname.value(name);
    }

    inline QuerySchema* query(int id) const {
        return queries.value(id);
    }

    inline void insertQuery(QuerySchema& query) {
        queries.insert(query.id(), &query);
        queries_byname.insert(query.name(), &query);
    }

    /*! @internal Removes \a querySchema from internal structures and
     destroys it. Does not make any change at the backend. */
    inline void removeQuery(QuerySchema &querySchema) {
        queries_byname.remove(querySchema.name());
        queries.remove(querySchema.id());
        delete &querySchema;
    }

    inline void setQueryObsolete(QuerySchema& query) {
        obsoleteQueries.insert(&query);
        queries_byname.take(query.name());
        queries.take(query.id());
    }

    inline void clearQueries() {
        qDeleteAll(queries);
        queries.clear();
    }

    Connection* const conn; //!< The \a Connection instance this \a ConnectionPrivate belongs to.
    QPointer<ConnectionData> conn_data; //!< the \a ConnectionData used within that connection.

    /*! Default Transaction.handle.
    If transactions are supported: Any operation on database (e.g. inserts)
    that is started without specifying transaction context, will be performed
    in the context of this transaction. */
    Transaction default_trans;
    QList<Transaction> transactions;

    QHash<TableSchema*, QSet<Connection::TableSchemaChangeListenerInterface*>* > tableSchemaChangeListeners;

    //! Used in Connection::setQuerySchemaObsolete( const QString& queryName )
    //! to collect obsolete queries. THese are deleted on connection deleting.
    QSet<QuerySchema*> obsoleteQueries;

    //! server version information for this connection.
    Predicate::ServerVersionInfo serverVersion;

    //! Database version information for this connection.
    Predicate::DatabaseVersionInfo databaseVersion;

    Parser *m_parser;

    //! cursors created for this connection
    QSet<Predicate::Cursor*> cursors;

    //! Database properties
    DatabaseProperties* dbProperties;

    QString availableDatabaseName; //!< used by anyAvailableDatabaseName()
    QString usedDatabase; //!< database name that is opened now (the currentDatabase() name)

    //! true if rollbackTransaction() and commitTransaction() shouldn't remove
    //! the transaction object from 'transactions' list; used by closeDatabase()
bool dont_remove_transactions : 1;

    //! used to avoid endless recursion between useDatabase() and databaseExists()
    //! when useTemporaryDatabaseIfNeeded() works
bool skip_databaseExists_check_in_useDatabase : 1;

    /*! Used when single transactions are only supported (Driver::SingleTransactions).
     True value means default Transaction.has been started inside connection object
     (by beginAutoCommitTransaction()), otherwise default Transaction.has been started outside
     of the object (e.g. before createTable()), so we shouldn't autocommit the transaction
     in commitAutoCommitTransaction(). Also, beginAutoCommitTransaction() doesn't restarts
     transaction if default_trans_started_inside is false. Such behaviour allows user to
     execute a sequence of actions like CREATE TABLE...; INSERT DATA...; within a single transaction
     and commit it or rollback by hand. */
bool default_trans_started_inside : 1;

bool isConnected : 1;

bool autoCommit : 1;

    /*! True for read only connection. Used especially for file-based drivers. */
bool readOnly : 1;
private:
    //! Table schemas retrieved on demand with tableSchema()
    QHash<int, TableSchema*> tables;
    QHash<QString, TableSchema*> tables_byname;
    //! used just for removing system TableSchema objects on db close.
    QSet<TableSchema*> _predicateSystemTables;
    //! Query schemas retrieved on demand with querySchema()
    QHash<int, QuerySchema*> queries;
    QHash<QString, QuerySchema*> queries_byname;
bool takeTableEnabled : 1; //!< used by takeTable() needed because otherwise 'tables' hash will
    //!< be touched by takeTable() what's not allowed during qDeleteAll()
};

}//namespace Predicate

//================================================
using namespace Predicate;

//! static: list of internal Predicate system table names
QStringList Predicate_predicateSystemTableNames;

Connection::Connection(Driver *driver, ConnectionData &conn_data)
        : QObject()
        , Predicate::Object()
        , d(new ConnectionPrivate(this, conn_data))
        , m_driver(driver)
        , m_destructor_started(false)
        , m_insideCloseDatabase(false)
{
    d->dbProperties = new DatabaseProperties(this);
//Qt3 m_cursors.setAutoDelete(true);
// d->transactions.setAutoDelete(true);
    //reasonable sizes: TODO
// d->transactions.resize(101);//woohoo! so many transactions?
    m_sql.reserve(0x4000);
}

void Connection::destroy()
{
    disconnect();
    //do not allow the driver to touch me: I will kill myself.
    m_driver->d->connections.remove(this);
}

Connection::~Connection()
{
    m_destructor_started = true;
// PreDbg << "Connection::~Connection()";
    delete d->dbProperties;
    delete d;
    d = 0;
    /* if (m_driver) {
        if (m_is_connected) {
          //delete own table schemas
          d->tables.clear();
          //delete own cursors:
          m_cursors.clear();
        }
        //do not allow the driver to touch me: I will kill myself.
        m_driver->m_connections.take( this );
      }*/
}

ConnectionData* Connection::data() const
{
    return d->conn_data;
}

bool Connection::connect()
{
    clearError();
    if (d->isConnected) {
        setError(ERR_ALREADY_CONNECTED, QObject::tr("Connection already established."));
        return false;
    }

    d->serverVersion.clear();
    if (!(d->isConnected = drv_connect(d->serverVersion))) {
        setError(m_driver->isFileBased() ?
                    tr("Could not open \"%1\" project file.")
                    .arg(QDir::convertSeparators(d->conn_data->fileName()))
                 :  tr("Could not connect to \"%1\" database server.").arg(d->conn_data->serverInfoString()));
    }
    return d->isConnected;
}

bool Connection::isDatabaseUsed() const
{
    return !d->usedDatabase.isEmpty() && d->isConnected && drv_isDatabaseUsed();
}

void Connection::clearError()
{
    Object::clearError();
    m_sql.clear();
}

bool Connection::disconnect()
{
    clearError();
    if (!d->isConnected)
        return true;

    if (!closeDatabase())
        return false;

    bool ok = drv_disconnect();
    if (ok)
        d->isConnected = false;
    return ok;
}

bool Connection::isConnected() const
{
    return d->isConnected;
}

bool Connection::checkConnected()
{
    if (d->isConnected) {
        clearError();
        return true;
    }
    setError(ERR_NO_CONNECTION, tr("Not connected to the database server."));
    return false;
}

bool Connection::checkIsDatabaseUsed()
{
    if (isDatabaseUsed()) {
        clearError();
        return true;
    }
    setError(ERR_NO_DB_USED, tr("Currently no database is used."));
    return false;
}

QStringList Connection::databaseNames(bool also_system_db)
{
    PreDbg << "Connection::databaseNames(" << also_system_db << ")";
    if (!checkConnected())
        return QStringList();

    QString tmpdbName;
    //some engines need to have opened any database before executing "create database"
    if (!useTemporaryDatabaseIfNeeded(tmpdbName))
        return QStringList();

    QStringList list, non_system_list;

    bool ret = drv_getDatabasesList(list);

    if (!tmpdbName.isEmpty()) {
        //whatever result is - now we have to close temporary opened database:
        if (!closeDatabase())
            return QStringList();
    }

    if (!ret)
        return QStringList();

    if (also_system_db)
        return list;
    //filter system databases:
    for (QStringList::ConstIterator it = list.constBegin(); it != list.constEnd(); ++it) {
        PreDbg << "Connection::databaseNames(): " << *it;
        if (!m_driver->isSystemDatabaseName(*it)) {
            PreDbg << "add " << *it;
            non_system_list << (*it);
        }
    }
    return non_system_list;
}

bool Connection::drv_getDatabasesList(QStringList &list)
{
    list.clear();
    return true;
}

bool Connection::drv_databaseExists(const QString &dbName, bool ignoreErrors)
{
    QStringList list = databaseNames(true);//also system
    if (error()) {
        return false;
    }

    if (list.indexOf(dbName) == -1) {
        if (!ignoreErrors)
            setError(ERR_OBJECT_NOT_FOUND, tr("The database \"%1\" does not exist.").arg(dbName));
        return false;
    }

    return true;
}

bool Connection::databaseExists(const QString &dbName, bool ignoreErrors)
{
// PreDbg << "Connection::databaseExists(" << dbName << "," << ignoreErrors << ")";
    if (!checkConnected())
        return false;
    clearError();

    if (m_driver->isFileBased()) {
        //for file-based db: file must exists and be accessible
//js: moved from useDatabase():
        QFileInfo file(d->conn_data->fileName());
        if (!file.exists() || (!file.isFile() && !file.isSymLink())) {
            if (!ignoreErrors)
                setError(ERR_OBJECT_NOT_FOUND, tr("Database file \"%1\" does not exist.")
                                               .arg(QDir::convertSeparators(d->conn_data->fileName())));
            return false;
        }
        if (!file.isReadable()) {
            if (!ignoreErrors)
                setError(ERR_ACCESS_RIGHTS, tr("Database file \"%1\" is not readable.")
                                            .arg(QDir::convertSeparators(d->conn_data->fileName())));
            return false;
        }
        if (!file.isWritable()) {
            if (!ignoreErrors)
                setError(ERR_ACCESS_RIGHTS, tr("Database file \"%1\" is not writable.")
                                            .arg(QDir::convertSeparators(d->conn_data->fileName())));
            return false;
        }
        return true;
    }

    QString tmpdbName;
    //some engines need to have opened any database before executing "create database"
    const bool orig_skip_databaseExists_check_in_useDatabase = d->skip_databaseExists_check_in_useDatabase;
    d->skip_databaseExists_check_in_useDatabase = true;
    bool ret = useTemporaryDatabaseIfNeeded(tmpdbName);
    d->skip_databaseExists_check_in_useDatabase = orig_skip_databaseExists_check_in_useDatabase;
    if (!ret)
        return false;

    ret = drv_databaseExists(dbName, ignoreErrors);

    if (!tmpdbName.isEmpty()) {
        //whatever result is - now we have to close temporary opened database:
        if (!closeDatabase())
            return false;
    }

    return ret;
}

#define createDatabase_CLOSE \
    { if (!closeDatabase()) { \
            setError(tr("Database \"%1\" created but could not be closed after creation.").arg(dbName) ); \
            return false; \
        } }

#define createDatabase_ERROR \
    { createDatabase_CLOSE; return false; }


bool Connection::createDatabase(const QString &dbName)
{
    if (!checkConnected())
        return false;

    if (databaseExists(dbName)) {
        setError(ERR_OBJECT_EXISTS, tr("Database \"%1\" already exists.").arg(dbName));
        return false;
    }
    if (m_driver->isSystemDatabaseName(dbName)) {
        setError(ERR_SYSTEM_NAME_RESERVED,
                 tr("Cannot create database \"%1\". This name is reserved for system database.").arg(dbName));
        return false;
    }
    if (m_driver->isFileBased()) {
        //update connection data if filename differs
        d->conn_data->setFileName(dbName);
    }

    QString tmpdbName;
    //some engines need to have opened any database before executing "create database"
    if (!useTemporaryDatabaseIfNeeded(tmpdbName))
        return false;

    //low-level create
    if (!drv_createDatabase(dbName)) {
        setError(tr("Error creating database \"%1\" on the server.").arg(dbName));
        closeDatabase();//sanity
        return false;
    }

    if (!tmpdbName.isEmpty()) {
        //whatever result is - now we have to close temporary opened database:
        if (!closeDatabase())
            return false;
    }

    if (!tmpdbName.isEmpty() || !m_driver->d->isDBOpenedAfterCreate) {
        //db need to be opened
        if (!useDatabase(dbName, false/*not yet kexi compatible!*/)) {
            setError(tr("Database \"%1\" created but could not be opened.").arg(dbName));
            return false;
        }
    } else {
        //just for the rule
        d->usedDatabase = dbName;
    }

    Transaction trans;
    if (m_driver->transactionsSupported()) {
        trans = beginTransaction();
        if (!trans.active())
            return false;
    }
//not needed since closeDatabase() rollbacks transaction: TransactionGuard trans_g(this);
// if (error())
//  return false;

    //-create system tables schema objects
    if (!setupKexiDBSystemSchema())
        return false;

    //-physically create system tables
    foreach(TableSchema* t, d->predicateSystemTables()) {
        if (!drv_createTable(t->name()))
            createDatabase_ERROR;
    }

    /* moved to KexiProject...

      //-create default part info
      TableSchema *ts;
      if (!(ts = tableSchema("kexi__parts")))
        createDatabase_ERROR;
      FieldList *fl = ts->subList("p_id", "p_name", "p_mime", "p_url");
      if (!fl)
        createDatabase_ERROR;
      if (!insertRecord(*fl, QVariant(1), QVariant("Tables"), QVariant("kexi/table"), QVariant("http://koffice.org/kexi/")))
        createDatabase_ERROR;
      if (!insertRecord(*fl, QVariant(2), QVariant("Queries"), QVariant("kexi/query"), QVariant("http://koffice.org/kexi/")))
        createDatabase_ERROR;
    */

    //-insert Predicate version info:
    TableSchema *t_db = d->table("kexi__db");
    if (!t_db)
        createDatabase_ERROR;
    if (!insertRecord(*t_db, "predicate_major_ver", Predicate::version().major)
            || !insertRecord(*t_db, "predicate_minor_ver", Predicate::version().minor))
        createDatabase_ERROR;

    if (trans.active() && !commitTransaction(trans))
        createDatabase_ERROR;

    createDatabase_CLOSE;
    return true;
}

#undef createDatabase_CLOSE
#undef createDatabase_ERROR

bool Connection::useDatabase(const QString &dbName, bool kexiCompatible, bool *cancelled, MessageHandler* msgHandler)
{
    if (cancelled)
        *cancelled = false;
    PreDbg << "Connection::useDatabase(" << dbName << "," << kexiCompatible << ")";
    if (!checkConnected())
        return false;

    if (dbName.isEmpty())
        return false;
    QString my_dbName = dbName;
// if (my_dbName.isEmpty()) {
//  const QStringList& db_lst = databaseNames();
//  if (!db_lst.isEmpty())
//   my_dbName = db_lst.first();
// }
    if (d->usedDatabase == my_dbName)
        return true; //already used

    if (!d->skip_databaseExists_check_in_useDatabase) {
        if (!databaseExists(my_dbName, false /*don't ignore errors*/))
            return false; //database must exist
    }

    if (!d->usedDatabase.isEmpty() && !closeDatabase()) //close db if already used
        return false;

    d->usedDatabase = "";

    if (!drv_useDatabase(my_dbName, cancelled, msgHandler)) {
        if (cancelled && *cancelled)
            return false;
        QString msg(tr("Opening database \"%1\" failed.").arg(my_dbName));
        if (error())
            setError(this, msg);
        else
            setError(msg);
        return false;
    }

    //-create system tables schema objects
    if (!setupKexiDBSystemSchema())
        return false;

    if (kexiCompatible && my_dbName.toLower() != anyAvailableDatabaseName().toLower()) {
        //-get global database information
        int num;
        bool ok;
//  static QString notfound_str = tr("\"%1\" database property not found");
        num = d->dbProperties->value("predicate_major_ver").toInt(&ok);
        if (!ok)
            return false;
        d->databaseVersion.major = num;
        /*  if (true!=querySingleNumber(
              "select db_value from kexi__db where db_property=" + m_driver->escapeString(QString("predicate_major_ver")), num)) {
              d->errorInvalidDBContents(notfound_str.arg("predicate_major_ver"));
              return false;
            }*/
        num = d->dbProperties->value("predicate_minor_ver").toInt(&ok);
        if (!ok)
            return false;
        d->databaseVersion.minor = num;
        /*  if (true!=querySingleNumber(
              "select db_value from kexi__db where db_property=" + m_driver->escapeString(QString("predicate_minor_ver")), num)) {
              d->errorInvalidDBContents(notfound_str.arg("predicate_minor_ver"));
              return false;
            }*/

#if 0 //this is already checked in DriverManagerInternal::lookupDrivers()
        //** error if major version does not match
        if (m_driver->versionMajor() != Predicate::versionMajor()) {
            setError(ERR_INCOMPAT_DATABASE_VERSION,
                     tr("Database version (%1) does not match Kexi application's version (%2)")
                     .arg( QString("%1.%2").arg(versionMajor(), versionMinor()),
                          QString("%1.%2").arg(Predicate::versionMajor(), Predicate::versionMinor()))
                    );
            return false;
        }
        if (m_driver->versionMinor() != Predicate::versionMinor()) {
            //js TODO: COMPATIBILITY CODE HERE!
            //js TODO: CONVERSION CODE HERE (or signal that conversion is needed)
        }
#endif
    }
    d->usedDatabase = my_dbName;
    return true;
}

bool Connection::closeDatabase()
{
    if (d->usedDatabase.isEmpty())
        return true; //no db used
    if (!checkConnected())
        return true;

    bool ret = true;

    /*! \todo (js) add CLEVER algorithm here for nested transactions */
    if (m_driver->transactionsSupported()) {
        //rollback all transactions
        d->dont_remove_transactions = true; //lock!
        foreach(const Transaction& tr, d->transactions) {
            if (!rollbackTransaction(tr)) {//rollback as much as you can, don't stop on prev. errors
                ret = false;
            } else {
                PreDbg << "Connection::closeDatabase(): transaction rolled back!";
                PreDbg << "Connection::closeDatabase(): trans.refcount==" <<
                (tr.m_data ? QString::number(tr.m_data->refcount) : "(null)");
            }
        }
        d->dont_remove_transactions = false; //unlock!
        d->transactions.clear(); //free trans. data
    }

    m_insideCloseDatabase = true;

    //delete own cursors:
    qDeleteAll(d->cursors);
    d->cursors.clear();
    //delete own schemas
    d->clearTables();
    d->clearQueries();

    m_insideCloseDatabase = false;

    if (!drv_closeDatabase())
        return false;

    d->usedDatabase = "";
// PreDbg << "Connection::closeDatabase(): " << ret;
    return ret;
}

QString Connection::currentDatabase() const
{
    return d->usedDatabase;
}

bool Connection::useTemporaryDatabaseIfNeeded(QString &tmpdbName)
{
    if (!m_driver->isFileBased() && m_driver->beh->USING_DATABASE_REQUIRED_TO_CONNECT
            && !isDatabaseUsed()) {
        //we have no db used, but it is required by engine to have used any!
        tmpdbName = anyAvailableDatabaseName();
        if (tmpdbName.isEmpty()) {
            setError(ERR_NO_DB_USED, tr("Cannot find any database for temporary connection."));
            return false;
        }
        const bool orig_skip_databaseExists_check_in_useDatabase = d->skip_databaseExists_check_in_useDatabase;
        d->skip_databaseExists_check_in_useDatabase = true;
        bool ret = useDatabase(tmpdbName, false);
        d->skip_databaseExists_check_in_useDatabase = orig_skip_databaseExists_check_in_useDatabase;
        if (!ret) {
            setError(errorNum(),
                     tr("Error during starting temporary connection using \"%1\" database name.")
                        .arg(tmpdbName));
            return false;
        }
    }
    return true;
}

bool Connection::dropDatabase(const QString &dbName)
{
    if (!checkConnected())
        return false;

    QString dbToDrop;
    if (dbName.isEmpty() && d->usedDatabase.isEmpty()) {
        if (!m_driver->isFileBased()
                || (m_driver->isFileBased() && d->conn_data->fileName().isEmpty())) {
            setError(ERR_NO_NAME_SPECIFIED, tr("Cannot drop database - name not specified."));
            return false;
        }
        //this is a file driver so reuse previously passed filename
        dbToDrop = d->conn_data->fileName();
    } else {
        if (dbName.isEmpty()) {
            dbToDrop = d->usedDatabase;
        } else {
            if (m_driver->isFileBased()) //lets get full path
                dbToDrop = QFileInfo(dbName).absoluteFilePath();
            else
                dbToDrop = dbName;
        }
    }

    if (dbToDrop.isEmpty()) {
        setError(ERR_NO_NAME_SPECIFIED, tr("Cannot delete database - name not specified."));
        return false;
    }

    if (m_driver->isSystemDatabaseName(dbToDrop)) {
        setError(ERR_SYSTEM_NAME_RESERVED, tr("Cannot delete system database \"%1\".").arg(dbToDrop));
        return false;
    }

    if (isDatabaseUsed() && d->usedDatabase == dbToDrop) {
        //we need to close database because cannot drop used this database
        if (!closeDatabase())
            return false;
    }

    QString tmpdbName;
    //some engines need to have opened any database before executing "drop database"
    if (!useTemporaryDatabaseIfNeeded(tmpdbName))
        return false;

    //ok, now we have access to dropping
    bool ret = drv_dropDatabase(dbToDrop);

    if (!tmpdbName.isEmpty()) {
        //whatever result is - now we have to close temporary opened database:
        if (!closeDatabase())
            return false;
    }
    return ret;
}

QStringList Connection::objectNames(int objType, bool* ok)
{
    QStringList list;

    if (!checkIsDatabaseUsed()) {
        if (ok)
            *ok = false;
        return list;
    }

    QString sql;
    if (objType == Predicate::AnyObjectType)
        sql = "SELECT o_name FROM kexi__objects";
    else
        sql = QString::fromLatin1("SELECT o_name FROM kexi__objects WHERE o_type=%1").arg(objType);

    Cursor *c = executeQuery(sql);
    if (!c) {
        if (ok)
            *ok = false;
        return list;
    }

    for (c->moveFirst(); !c->eof(); c->moveNext()) {
        QString name = c->value(0).toString();
        if (Utils::isIdentifier(name)) {
            list.append(name);
        }
    }

    if (!deleteCursor(c)) {
        if (ok)
            *ok = false;
        return list;
    }

    if (ok)
        *ok = true;
    return list;
}

QStringList Connection::tableNames(bool also_system_tables)
{
    bool ok = true;
    QStringList list = objectNames(TableObjectType, &ok);
    if (also_system_tables && ok) {
        list += Connection::predicateSystemTableNames();
    }
    return list;
}

//! \todo (js): this will depend on Predicate lib version
const QStringList& Connection::predicateSystemTableNames()
{
    if (Predicate_predicateSystemTableNames.isEmpty()) {
        Predicate_predicateSystemTableNames
        << "kexi__objects"
        << "kexi__objectdata"
        << "kexi__fields"
//  << "kexi__querydata"
//  << "kexi__queryfields"
//  << "kexi__querytables"
        << "kexi__db"
        ;
    }
    return Predicate_predicateSystemTableNames;
}

Predicate::ServerVersionInfo* Connection::serverVersion() const
{
    return isConnected() ? &d->serverVersion : 0;
}

Predicate::DatabaseVersionInfo* Connection::databaseVersion() const
{
    return isDatabaseUsed() ? &d->databaseVersion : 0;
}

DatabaseProperties& Connection::databaseProperties()
{
    return *d->dbProperties;
}

QList<int> Connection::tableIds()
{
    return objectIds(Predicate::TableObjectType);
}

QList<int> Connection::queryIds()
{
    return objectIds(Predicate::QueryObjectType);
}

QList<int> Connection::objectIds(int objType)
{
    QList<int> list;

    if (!checkIsDatabaseUsed())
        return list;

    Cursor *c = executeQuery(
                    QString::fromLatin1("SELECT o_id, o_name FROM kexi__objects WHERE o_type=%1")
                    .arg(objType));
    if (!c)
        return list;
    for (c->moveFirst(); !c->eof(); c->moveNext()) {
        QString tname = c->value(1).toString(); //kexi__objects.o_name
        if (Utils::isIdentifier(tname)) {
            list.append(c->value(0).toInt()); //kexi__objects.o_id
        }
    }
    deleteCursor(c);
    return list;
}

QString Connection::createTableStatement(const Predicate::TableSchema& tableSchema) const
{
// Each SQL identifier needs to be escaped in the generated query.
    QString sql;
    sql.reserve(4096);
    sql = "CREATE TABLE " + escapeIdentifier(tableSchema.name()) + " (";
    bool first = true;
    foreach(Field *field, tableSchema.m_fields) {
        if (first)
            first = false;
        else
            sql += ", ";
        QString v = escapeIdentifier(field->name()) + " ";
        const bool autoinc = field->isAutoIncrement();
        const bool pk = field->isPrimaryKey() || (autoinc && m_driver->beh->AUTO_INCREMENT_REQUIRES_PK);
//TODO: warning: ^^^^^ this allows only one autonumber per table when AUTO_INCREMENT_REQUIRES_PK==true!
        if (autoinc && m_driver->beh->SPECIAL_AUTO_INCREMENT_DEF) {
            if (pk)
                v += m_driver->beh->AUTO_INCREMENT_TYPE + " " + m_driver->beh->AUTO_INCREMENT_PK_FIELD_OPTION;
            else
                v += m_driver->beh->AUTO_INCREMENT_TYPE + " " + m_driver->beh->AUTO_INCREMENT_FIELD_OPTION;
        } else {
            if (autoinc && !m_driver->beh->AUTO_INCREMENT_TYPE.isEmpty())
                v += m_driver->beh->AUTO_INCREMENT_TYPE;
            else
                v += m_driver->sqlTypeName(field->type(), field->precision());

            if (field->isUnsigned())
                v += (" " + m_driver->beh->UNSIGNED_TYPE_KEYWORD);

            if (field->isFPNumericType() && field->precision() > 0) {
                if (field->scale() > 0)
                    v += QString::fromLatin1("(%1,%2)").arg(field->precision()).arg(field->scale());
                else
                    v += QString::fromLatin1("(%1)").arg(field->precision());
            } else if (field->type() == Field::Text && field->length() > 0)
                v += QString::fromLatin1("(%1)").arg(field->length());

            if (autoinc)
                v += (" " +
                      (pk ? m_driver->beh->AUTO_INCREMENT_PK_FIELD_OPTION : m_driver->beh->AUTO_INCREMENT_FIELD_OPTION));
            else
                //TODO: here is automatically a single-field key created
                if (pk)
                    v += " PRIMARY KEY";
            if (!pk && field->isUniqueKey())
                v += " UNIQUE";
///@todo IS this ok for all engines?: if (!autoinc && !field->isPrimaryKey() && field->isNotNull())
            if (!autoinc && !pk && field->isNotNull())
                v += " NOT NULL"; //only add not null option if no autocommit is set
            if (field->defaultValue().isValid()) {
                QString valToSQL(m_driver->valueToSQL(field, field->defaultValue()));
                if (!valToSQL.isEmpty()) //for sanity
                    v += QString::fromLatin1(" DEFAULT ") + valToSQL;
            }
        }
        sql += v;
    }
    sql += ")";
    return sql;
}

//yeah, it is very efficient:
#define C_A(a) , const QVariant& c ## a

#define V_A0 m_driver->valueToSQL( tableSchema.field(0), c0 )
#define V_A(a) +","+m_driver->valueToSQL( \
        tableSchema.field(a) ? tableSchema.field(a)->type() : Field::Text, c ## a )

//  PreDbg << "******** " << QString("INSERT INTO ") +
//   escapeIdentifier(tableSchema.name()) +
//   " VALUES (" + vals + ")";

#define C_INS_REC(args, vals) \
    bool Connection::insertRecord(Predicate::TableSchema &tableSchema args) {\
        if ( !drv_beforeInsert( tableSchema.name(), tableSchema ) )  \
            return false;                                      \
        \
        bool res = executeSQL(                                      \
                   QString("INSERT INTO ") + escapeIdentifier(tableSchema.name()) \
                   + " (" + tableSchema.sqlFieldsList(m_driver) + ") VALUES (" + vals + ")"      \
                             ); \
        \
        if ( !drv_afterInsert( tableSchema.name(),tableSchema ) ) \
            return false;                                      \
        \
        return res;                                             \
    }

#define C_INS_REC_ALL \
    C_INS_REC( C_A(0), V_A0 ) \
    C_INS_REC( C_A(0) C_A(1), V_A0 V_A(1) ) \
    C_INS_REC( C_A(0) C_A(1) C_A(2), V_A0 V_A(1) V_A(2) ) \
    C_INS_REC( C_A(0) C_A(1) C_A(2) C_A(3), V_A0 V_A(1) V_A(2) V_A(3) ) \
    C_INS_REC( C_A(0) C_A(1) C_A(2) C_A(3) C_A(4), V_A0 V_A(1) V_A(2) V_A(3) V_A(4) ) \
    C_INS_REC( C_A(0) C_A(1) C_A(2) C_A(3) C_A(4) C_A(5), V_A0 V_A(1) V_A(2) V_A(3) V_A(4) V_A(5) ) \
    C_INS_REC( C_A(0) C_A(1) C_A(2) C_A(3) C_A(4) C_A(5) C_A(6), V_A0 V_A(1) V_A(2) V_A(3) V_A(4) V_A(5) V_A(6) ) \
    C_INS_REC( C_A(0) C_A(1) C_A(2) C_A(3) C_A(4) C_A(5) C_A(6) C_A(7), V_A0 V_A(1) V_A(2) V_A(3) V_A(4) V_A(5) V_A(6) V_A(7) )

C_INS_REC_ALL

#undef V_A0
#undef V_A
#undef C_INS_REC

#define V_A0 value += m_driver->valueToSQL( it.next(), c0 );
#define V_A( a ) value += ("," + m_driver->valueToSQL( it.next(), c ## a ));

#define C_INS_REC(args, vals) \
    bool Connection::insertRecord(FieldList& fields args) \
    { \
        QString value; \
        const Field::List *flist = fields.fields(); \
        QListIterator<Field*> it(*flist); \
        vals \
        it.toFront(); \
        QString tableName( (it.hasNext() && it.peekNext()->table()) ? it.next()->table()->name() : "??" ); \
        if ( !drv_beforeInsert( tableName, fields ) )            \
            return false;                                       \
        bool res = executeSQL(                                  \
                   QString("INSERT INTO ") + escapeIdentifier(tableName) \
                   + "(" + fields.sqlFieldsList(m_driver) + ") VALUES (" + value + ")" \
                             ); \
        if ( !drv_afterInsert( tableName, fields ) )    \
            return false;                               \
        return res;                             \
    }

C_INS_REC_ALL

#undef C_A
#undef V_A
#undef V_ALAST
#undef C_INS_REC
#undef C_INS_REC_ALL

bool Connection::insertRecord(TableSchema &tableSchema, const QList<QVariant>& values)
{
// Each SQL identifier needs to be escaped in the generated query.
    const Field::List *fields = tableSchema.fields();
    Field::ListIterator fieldsIt(fields->constBegin());
    Field *f = *fieldsIt;
// QString s_val;
// s_val.reserve(4096);
    m_sql.clear();
    QList<QVariant>::ConstIterator it = values.constBegin();
// int i=0;
    while (f && (it != values.end())) {
        if (m_sql.isEmpty())
            m_sql = QString("INSERT INTO ") +
                    escapeIdentifier(tableSchema.name()) +
                    " VALUES (";
        else
            m_sql += ",";
        m_sql += m_driver->valueToSQL(f, *it);
//  PreDbg << "val" << i++ << ": " << m_driver->valueToSQL( f, *it );
        ++it;
        ++fieldsIt;
        f = *fieldsIt;
    }
    m_sql += ")";

// PreDbg<<"******** "<< m_sql;
    if (!drv_beforeInsert(tableSchema.name(), tableSchema))
        return false;
    bool res = executeSQL(m_sql);
    if (!drv_afterInsert(tableSchema.name(), tableSchema))
        return false;

    return res;
}

bool Connection::insertRecord(FieldList& fields, const QList<QVariant>& values)
{
// Each SQL identifier needs to be escaped in the generated query.
    const Field::List *flist = fields.fields();
    Field::ListIterator fieldsIt(flist->constBegin());
    Field *f = *fieldsIt;
    if (!f)
        return false;
// QString s_val;
// s_val.reserve(4096);
    m_sql.clear();
    QList<QVariant>::ConstIterator it = values.constBegin();
// int i=0;
    QString tableName = escapeIdentifier(flist->first()->table()->name());
    while (f && (it != values.constEnd())) {
        if (m_sql.isEmpty())
            m_sql = QString("INSERT INTO ") +
                    tableName + "(" +
                    fields.sqlFieldsList(m_driver) + ") VALUES (";
        else
            m_sql += ",";
        m_sql += m_driver->valueToSQL(f, *it);
//  PreDbg << "val" << i++ << ": " << m_driver->valueToSQL( f, *it );
        ++it;
        ++fieldsIt;
        f = *fieldsIt;
    }
    m_sql += ")";

    if (!drv_beforeInsert(tableName, fields))
        return false;
    bool res = executeSQL(m_sql);
    if (!drv_afterInsert(tableName, fields))
        return false;

    return res;
}

bool Connection::executeSQL(const QString& statement)
{
    m_sql = statement; //remember for Error.handling
    if (!drv_executeSQL(m_sql)) {
        m_errMsg.clear(); //clear as this could be most probably jsut "Unknown error" string.
        m_errorSql = statement;
        setError(this, ERR_SQL_EXECUTION_ERROR, tr("Error while executing SQL statement."));
        return false;
    }
    return true;
}

QString Connection::selectStatement(Predicate::QuerySchema& querySchema,
                                    const QList<QVariant>& params,
                                    const SelectStatementOptions& options) const
{
//"SELECT FROM ..." is theoretically allowed "
//if (querySchema.fieldCount()<1)
//  return QString();
// Each SQL identifier needs to be escaped in the generated query.

    if (!querySchema.statement().isEmpty())
        return querySchema.statement();

//! @todo looking at singleTable is visually nice but a field name can conflict
//!   with function or variable name...
    uint number = 0;
    bool singleTable = querySchema.tables()->count() <= 1;
    if (singleTable) {
        //make sure we will have single table:
        foreach(Field *f, *querySchema.fields()) {
            if (querySchema.isColumnVisible(number) && f->table() && f->table()->lookupFieldSchema(*f)) {
                //uups, no, there's at least one left join
                singleTable = false;
                break;
            }
            number++;
        }
    }

    QString sql; //final sql string
    sql.reserve(4096);
//unused QString s_from_additional; //additional tables list needed for lookup fields
    QString s_additional_joins; //additional joins needed for lookup fields
    QString s_additional_fields; //additional fields to append to the fields list
    uint internalUniqueTableAliasNumber = 0; //used to build internalUniqueTableAliases
    uint internalUniqueQueryAliasNumber = 0; //used to build internalUniqueQueryAliases
    number = 0;
    QList<QuerySchema*> subqueries_for_lookup_data; // subqueries will be added to FROM section
    QString predicate_subquery_prefix("__predicate_subquery_");
    foreach(Field *f, *querySchema.fields()) {
        if (querySchema.isColumnVisible(number)) {
            if (!sql.isEmpty())
                sql += QString::fromLatin1(", ");

            if (f->isQueryAsterisk()) {
                if (!singleTable && static_cast<QueryAsterisk*>(f)->isSingleTableAsterisk()) //single-table *
                    sql += escapeIdentifier(f->table()->name(), options.identifierEscaping) +
                           QString::fromLatin1(".*");
                else //all-tables * (or simplified table.* when there's only one table)
                    sql += QString::fromLatin1("*");
            } else {
                if (f->isExpression()) {
                    sql += f->expression()->toString();
                } else {
                    if (!f->table()) //sanity check
                        return QString();

                    QString tableName;
                    int tablePosition = querySchema.tableBoundToColumn(number);
                    if (tablePosition >= 0)
                        tableName = querySchema.tableAlias(tablePosition);
                    if (tableName.isEmpty())
                        tableName = f->table()->name();

                    if (!singleTable) {
                        sql += (escapeIdentifier(tableName, options.identifierEscaping) + ".");
                    }
                    sql += escapeIdentifier(f->name(), options.identifierEscaping);
                }
                QString aliasString = QString(querySchema.columnAlias(number));
                if (!aliasString.isEmpty())
                    sql += (QString::fromLatin1(" AS ") + aliasString);
//! @todo add option that allows to omit "AS" keyword
            }
            LookupFieldSchema *lookupFieldSchema = (options.addVisibleLookupColumns && f->table())
                                                   ? f->table()->lookupFieldSchema(*f) : 0;
            if (lookupFieldSchema && lookupFieldSchema->boundColumn() >= 0) {
                // Lookup field schema found
                // Now we also need to fetch "visible" value from the lookup table, not only the value of binding.
                // -> build LEFT OUTER JOIN clause for this purpose (LEFT, not INNER because the binding can be broken)
                // "LEFT OUTER JOIN lookupTable ON thisTable.thisField=lookupTable.boundField"
                LookupFieldSchema::RowSource& rowSource = lookupFieldSchema->rowSource();
                if (rowSource.type() == LookupFieldSchema::RowSource::Table) {
                    TableSchema *lookupTable = querySchema.connection()->tableSchema(rowSource.name());
                    FieldList* visibleColumns = 0;
                    Field *boundField = 0;
                    if (lookupTable
                            && (uint)lookupFieldSchema->boundColumn() < lookupTable->fieldCount()
                            && (visibleColumns = lookupTable->subList(lookupFieldSchema->visibleColumns()))
                            && (boundField = lookupTable->field(lookupFieldSchema->boundColumn()))) {
                        //add LEFT OUTER JOIN
                        if (!s_additional_joins.isEmpty())
                            s_additional_joins += QString::fromLatin1(" ");
                        QString internalUniqueTableAlias(QString("__predicate_") + lookupTable->name() + "_"
                                                         + QString::number(internalUniqueTableAliasNumber++));
                        s_additional_joins += QString("LEFT OUTER JOIN %1 AS %2 ON %3.%4=%5.%6")
                                              .arg(escapeIdentifier(lookupTable->name(), options.identifierEscaping),
                                                   internalUniqueTableAlias,
                                                   escapeIdentifier(f->table()->name(), options.identifierEscaping),
                                                   escapeIdentifier(f->name(), options.identifierEscaping),
                                                   internalUniqueTableAlias,
                                                   escapeIdentifier(boundField->name(), options.identifierEscaping));

                        //add visibleField to the list of SELECTed fields //if it is not yet present there
//not needed      if (!querySchema.findTableField( visibleField->table()->name()+"."+visibleField->name() )) {
#if 0
                        if (!querySchema.table(visibleField->table()->name())) {
                            /* not true
                                          //table should be added after FROM
                                          if (!s_from_additional.isEmpty())
                                            s_from_additional += QString::fromLatin1(", ");
                                          s_from_additional += escapeIdentifier(visibleField->table()->name(), options.identifierEscaping);
                                          */
                        }
#endif
                        if (!s_additional_fields.isEmpty())
                            s_additional_fields += QString::fromLatin1(", ");
//       s_additional_fields += (internalUniqueTableAlias + "." //escapeIdentifier(visibleField->table()->name(), options.identifierEscaping) + "."
//         escapeIdentifier(visibleField->name(), options.identifierEscaping));
//! @todo Add lookup schema option for separator other than ' ' or even option for placeholders like "Name ? ?"
//! @todo Add possibility for joining the values at client side.
                        s_additional_fields += visibleColumns->sqlFieldsList(
                                                   driver(), " || ' ' || ", internalUniqueTableAlias, options.identifierEscaping);
                    }
                    delete visibleColumns;
                } else if (rowSource.type() == LookupFieldSchema::RowSource::Query) {
                    QuerySchema *lookupQuery = querySchema.connection()->querySchema(rowSource.name());
                    if (!lookupQuery) {
                        PreWarn << "Connection::selectStatement(): !lookupQuery";
                        return QString();
                    }
                    const QueryColumnInfo::Vector fieldsExpanded(lookupQuery->fieldsExpanded());
                    if (lookupFieldSchema->boundColumn() >= fieldsExpanded.count()) {
                        PreWarn << "Connection::selectStatement(): (uint)lookupFieldSchema->boundColumn() >= fieldsExpanded.count()";
                        return QString();
                    }
                    QueryColumnInfo *boundColumnInfo = fieldsExpanded.at(lookupFieldSchema->boundColumn());
                    if (!boundColumnInfo) {
                        PreWarn << "Connection::selectStatement(): !boundColumnInfo";
                        return QString();
                    }
                    Field *boundField = boundColumnInfo->field;
                    if (!boundField) {
                        PreWarn << "Connection::selectStatement(): !boundField";
                        return QString();
                    }
                    //add LEFT OUTER JOIN
                    if (!s_additional_joins.isEmpty())
                        s_additional_joins += QString::fromLatin1(" ");
                    QString internalUniqueQueryAlias(
                        predicate_subquery_prefix + lookupQuery->name() + "_"
                        + QString::number(internalUniqueQueryAliasNumber++));
                    s_additional_joins += QString("LEFT OUTER JOIN (%1) AS %2 ON %3.%4=%5.%6")
                                          .arg(selectStatement(*lookupQuery, params, options),
                                               internalUniqueQueryAlias,
                                               escapeIdentifier(f->table()->name(), options.identifierEscaping),
                                               escapeIdentifier(f->name(), options.identifierEscaping),
                                               internalUniqueQueryAlias,
                                               escapeIdentifier(boundColumnInfo->aliasOrName(), options.identifierEscaping));

                    if (!s_additional_fields.isEmpty())
                        s_additional_fields += QString::fromLatin1(", ");
                    const QList<uint> visibleColumns(lookupFieldSchema->visibleColumns());
                    QString expression;
                    foreach(uint visibleColumnIndex, visibleColumns) {
//! @todo Add lookup schema option for separator other than ' ' or even option for placeholders like "Name ? ?"
//! @todo Add possibility for joining the values at client side.
                        if ((uint)fieldsExpanded.count() <= visibleColumnIndex) {
                            PreWarn << "Connection::selectStatement(): fieldsExpanded.count() <= (*visibleColumnsIt) : "
                            << fieldsExpanded.count() << " <= " << visibleColumnIndex;
                            return QString();
                        }
                        if (!expression.isEmpty())
                            expression += " || ' ' || ";
                        expression += (internalUniqueQueryAlias + "." +
                                       escapeIdentifier(fieldsExpanded.value(visibleColumnIndex)->aliasOrName(),
                                                        options.identifierEscaping));
                    }
                    s_additional_fields += expression;
//subqueries_for_lookup_data.append(lookupQuery);
                } else {
                    PreWarn << "Connection::selectStatement(): unsupported row source type "
                    << rowSource.typeName();
                    return QString();
                }
            }
        }
        number++;
    }

    //add lookup fields
    if (!s_additional_fields.isEmpty())
        sql += (QString::fromLatin1(", ") + s_additional_fields);

    if (options.alsoRetrieveROWID) { //append rowid column
        QString s;
        if (!sql.isEmpty())
            s = QString::fromLatin1(", ");
        if (querySchema.masterTable())
            s += (escapeIdentifier(querySchema.masterTable()->name()) + ".");
        s += m_driver->beh->ROW_ID_FIELD_NAME;
        sql += s;
    }

    sql.prepend("SELECT ");
    TableSchema::List* tables = querySchema.tables();
    if ((tables && !tables->isEmpty()) || !subqueries_for_lookup_data.isEmpty()) {
        sql += QString::fromLatin1(" FROM ");
        QString s_from;
        if (tables) {
            number = 0;
            foreach(TableSchema *table, *tables) {
                if (!s_from.isEmpty())
                    s_from += QString::fromLatin1(", ");
                s_from += escapeIdentifier(table->name(), options.identifierEscaping);
                QString aliasString = QString(querySchema.tableAlias(number));
                if (!aliasString.isEmpty())
                    s_from += (QString::fromLatin1(" AS ") + aliasString);
                number++;
            }
            /*unused if (!s_from_additional.isEmpty()) {//additional tables list needed for lookup fields
                  if (!s_from.isEmpty())
                    s_from += QString::fromLatin1(", ");
                  s_from += s_from_additional;
                }*/
        }
        // add subqueries for lookup data
        uint subqueries_for_lookup_data_counter = 0;
        foreach(QuerySchema* subQuery, subqueries_for_lookup_data) {
            if (!s_from.isEmpty())
                s_from += QString::fromLatin1(", ");
            s_from += QString::fromLatin1("(");
            s_from += selectStatement(*subQuery, params, options);
            s_from += QString::fromLatin1(") AS %1%2")
                      .arg(predicate_subquery_prefix, subqueries_for_lookup_data_counter++);
        }
        sql += s_from;
    }
    QString s_where;
    s_where.reserve(4096);

    //JOINS
    if (!s_additional_joins.isEmpty()) {
        sql += QString::fromLatin1(" ") + s_additional_joins + QString::fromLatin1(" ");
    }

//@todo: we're using WHERE for joins now; use INNER/LEFT/RIGHT JOIN later

    //WHERE
    bool wasWhere = false; //for later use
    foreach(Relationship *rel, *querySchema.relationships()) {
        if (s_where.isEmpty()) {
            wasWhere = true;
        } else
            s_where += QString::fromLatin1(" AND ");
        QString s_where_sub;
        foreach(Field::Pair pair, *rel->fieldPairs()) {
            if (!s_where_sub.isEmpty())
                s_where_sub += QString::fromLatin1(" AND ");
            s_where_sub += (
                               escapeIdentifier(pair.first->table()->name(), options.identifierEscaping) +
                               QString::fromLatin1(".") +
                               escapeIdentifier(pair.first->name(), options.identifierEscaping) +
                               QString::fromLatin1(" = ")  +
                               escapeIdentifier(pair.second->table()->name(), options.identifierEscaping) +
                               QString::fromLatin1(".") +
                               escapeIdentifier(pair.second->name(), options.identifierEscaping));
        }
        if (rel->fieldPairs()->count() > 1) {
            s_where_sub.prepend("(");
            s_where_sub += QString::fromLatin1(")");
        }
        s_where += s_where_sub;
    }
    //EXPLICITLY SPECIFIED WHERE EXPRESSION
    if (querySchema.whereExpression()) {
        QuerySchemaParameterValueListIterator paramValuesIt(*m_driver, params);
        QuerySchemaParameterValueListIterator *paramValuesItPtr = params.isEmpty() ? 0 : &paramValuesIt;
        if (wasWhere) {
//TODO: () are not always needed
            s_where = "(" + s_where + ") AND (" + querySchema.whereExpression()->toString(paramValuesItPtr) + ")";
        } else {
            s_where = querySchema.whereExpression()->toString(paramValuesItPtr);
        }
    }
    if (!s_where.isEmpty())
        sql += QString::fromLatin1(" WHERE ") + s_where;
//! \todo (js) add other sql parts
    //(use wasWhere here)

    // ORDER BY
    QString orderByString(
        querySchema.orderByColumnList().toSQLString(!singleTable/*includeTableName*/,
                driver(), options.identifierEscaping));
    const QVector<int> pkeyFieldsOrder(querySchema.pkeyFieldsOrder());
    if (orderByString.isEmpty() && !pkeyFieldsOrder.isEmpty()) {
        //add automatic ORDER BY if there is no explicitly defined (especially helps when there are complex JOINs)
        OrderByColumnList automaticPKOrderBy;
        const QueryColumnInfo::Vector fieldsExpanded(querySchema.fieldsExpanded());
        foreach(int pkeyFieldsIndex, pkeyFieldsOrder) {
            if (pkeyFieldsIndex < 0) // no field mentioned in this query
                continue;
            if (pkeyFieldsIndex >= (int)fieldsExpanded.count()) {
                PreWarn << "Connection::selectStatement(): ORDER BY: (*it) >= fieldsExpanded.count() - "
                << pkeyFieldsIndex << " >= " << fieldsExpanded.count();
                continue;
            }
            QueryColumnInfo *ci = fieldsExpanded[ pkeyFieldsIndex ];
            automaticPKOrderBy.appendColumn(*ci);
        }
        orderByString = automaticPKOrderBy.toSQLString(!singleTable/*includeTableName*/,
                        driver(), options.identifierEscaping);
    }
    if (!orderByString.isEmpty())
        sql += (" ORDER BY " + orderByString);

    //PreDbg << sql;
    return sql;
}

QString Connection::selectStatement(Predicate::TableSchema& tableSchema,
                                    const SelectStatementOptions& options) const
{
    return selectStatement(*tableSchema.query(), options);
}

Field* Connection::findSystemFieldName(const Predicate::FieldList& fieldlist)
{
    for (Field::ListIterator it(fieldlist.fieldsIterator()); it != fieldlist.fieldsIteratorConstEnd(); ++it) {
        if (m_driver->isSystemFieldName((*it)->name()))
            return *it;
    }
    return 0;
}

quint64 Connection::lastInsertedAutoIncValue(const QString& aiFieldName, const QString& tableName,
        quint64* ROWID)
{
    quint64 row_id = drv_lastInsertRowID();
    if (ROWID)
        *ROWID = row_id;
    if (m_driver->beh->ROW_ID_FIELD_RETURNS_LAST_AUTOINCREMENTED_VALUE) {
        return row_id;
    }
    RecordData rdata;
    if (row_id <= 0 || true != querySingleRecord(
                QString::fromLatin1("SELECT ") + tableName + QString::fromLatin1(".") + aiFieldName + QString::fromLatin1(" FROM ") + tableName
                + QString::fromLatin1(" WHERE ") + m_driver->beh->ROW_ID_FIELD_NAME + QString::fromLatin1("=") + QString::number(row_id), rdata)) {
//  PreDbg << "Connection::lastInsertedAutoIncValue(): row_id<=0 || true!=querySingleRecord()";
        return (quint64) - 1; //ULL;
    }
    return rdata[0].toULongLong();
}

quint64 Connection::lastInsertedAutoIncValue(const QString& aiFieldName,
        const Predicate::TableSchema& table, quint64* ROWID)
{
    return lastInsertedAutoIncValue(aiFieldName, table.name(), ROWID);
}

//! Creates a Field list for kexi__fields, for sanity. Used by createTable()
static FieldList* createFieldListForKexi__Fields(TableSchema *kexi__fieldsSchema)
{
    if (!kexi__fieldsSchema)
        return 0;
    return kexi__fieldsSchema->subList(
               "t_id",
               "f_type",
               "f_name",
               "f_length",
               "f_precision",
               "f_constraints",
               "f_options",
               "f_default",
               "f_order",
               "f_caption",
               "f_help"
           );
}

//! builds a list of values for field's \a f properties. Used by createTable().
void buildValuesForKexi__Fields(QList<QVariant>& vals, Field* f)
{
    vals.clear();
    vals
    << QVariant(f->table()->id())
    << QVariant(f->type())
    << QVariant(f->name())
    << QVariant(f->isFPNumericType() ? f->scale() : f->length())
    << QVariant(f->isFPNumericType() ? f->precision() : 0)
    << QVariant(f->constraints())
    << QVariant(f->options())
    // Predicate::variantToString() is needed here because the value can be of any QVariant type,
    // depending on f->type()
    << (f->defaultValue().isNull()
        ? QVariant() : QVariant(Predicate::variantToString(f->defaultValue())))
    << QVariant(f->order())
    << QVariant(f->caption())
    << QVariant(f->description());
}

bool Connection::storeMainFieldSchema(Field *field)
{
    if (!field || !field->table())
        return false;
    FieldList *fl = createFieldListForKexi__Fields(d->table("kexi__fields"));
    if (!fl)
        return false;

    QList<QVariant> vals;
    buildValuesForKexi__Fields(vals, field);
    QList<QVariant>::ConstIterator valsIt = vals.constBegin();
    bool first = true;
    QString sql = "UPDATE kexi__fields SET ";
    foreach(Field *f, *fl->fields()) {
        sql.append((first ? QString() : QString(", ")) +
                   f->name() + "=" + m_driver->valueToSQL(f, *valsIt));
        if (first)
            first = false;
        ++valsIt;
    }
    delete fl;

    sql.append(QString(" WHERE t_id=") + QString::number(field->table()->id())
               + " AND f_name=" + m_driver->valueToSQL(Field::Text, field->name()));
    return executeSQL(sql);
}

#define createTable_ERR \
    { PreDbg << "Connection::createTable(): ERROR!"; \
        setError(this, tr("Creating table failed.")); \
        rollbackAutoCommitTransaction(tg.transaction()); \
        return false; }
//setError( errorNum(), tr("Creating table failed.") + " " + errorMsg());

//! Creates a table according to the given schema
/*! Creates a table according to the given TableSchema, adding the table and
    column definitions to kexi__* tables.  Checks that a database is in use,
    that the table name is not that of a system table, and that the schema
    defines at least one column.
    If the table exists, and replaceExisting is true, the table is replaced.
    Otherwise, the table is not replaced.
*/
bool Connection::createTable(Predicate::TableSchema* tableSchema, bool replaceExisting)
{
    if (!tableSchema || !checkIsDatabaseUsed())
        return false;

    //check if there are any fields
    if (tableSchema->fieldCount() < 1) {
        clearError();
        setError(ERR_CANNOT_CREATE_EMPTY_OBJECT, tr("Cannot create table without fields."));
        return false;
    }
    const bool internalTable = dynamic_cast<InternalTableSchema*>(tableSchema);

    const QString &tableName = tableSchema->name().toLower();

    if (!internalTable) {
        if (m_driver->isSystemObjectName(tableName)) {
            clearError();
            setError(ERR_SYSTEM_NAME_RESERVED, tr("System name \"%1\" cannot be used as table name.")
                                                .arg(tableSchema->name()));
            return false;
        }

        Field *sys_field = findSystemFieldName(*tableSchema);
        if (sys_field) {
            clearError();
            setError(ERR_SYSTEM_NAME_RESERVED,
                     tr("System name \"%1\" cannot be used as one of fields in \"%2\" table.")
                     .arg(sys_field->name(), tableName));
            return false;
        }
    }

    bool previousSchemaStillKept = false;

    Predicate::TableSchema *existingTable = 0;
    if (replaceExisting) {
        //get previous table (do not retrieve, though)
        existingTable = d->table(tableName);
        if (existingTable) {
            if (existingTable == tableSchema) {
                clearError();
                setError(ERR_OBJECT_EXISTS,
                         tr("Could not create the same table \"%1\" twice.").arg(tableSchema->name()));
                return false;
            }
//TODO(js): update any structure (e.g. queries) that depend on this table!
            if (existingTable->id() > 0)
                tableSchema->m_id = existingTable->id(); //copy id from existing table
            previousSchemaStillKept = true;
            if (!dropTable(existingTable, false /*alsoRemoveSchema*/))
                return false;
        }
    } else {
        if (this->tableSchema(tableSchema->name()) != 0) {
            clearError();
            setError(ERR_OBJECT_EXISTS, tr("Table \"%1\" already exists.").arg(tableSchema->name()));
            return false;
        }
    }

    /* if (replaceExisting) {
      //get previous table (do not retrieve, though)
      Predicate::TableSchema *existingTable = d->tables_byname.take(name);
      if (oldTable) {
      }*/

    TransactionGuard tg;
    if (!beginAutoCommitTransaction(tg))
        return false;

    if (!drv_createTable(*tableSchema))
        createTable_ERR;

    //add schema data to kexi__* tables
    if (!internalTable) {
        //update kexi__objects
        if (!storeObjectSchemaData(*tableSchema, true))
            createTable_ERR;

        TableSchema *ts = d->table("kexi__fields");
        if (!ts)
            return false;
        //for sanity: remove field info (if any) for this table id
        if (!Predicate::deleteRow(*this, ts, "t_id", tableSchema->id()))
            return false;

        FieldList *fl = createFieldListForKexi__Fields(d->table("kexi__fields"));
        if (!fl)
            return false;

        foreach(Field *f, *tableSchema->fields()) {
            QList<QVariant> vals;
            buildValuesForKexi__Fields(vals, f);
            if (!insertRecord(*fl, vals))
                createTable_ERR;
        }
        delete fl;

        if (!storeExtendedTableSchemaData(*tableSchema))
            createTable_ERR;
    }

    //finally:
    /* if (replaceExisting) {
        if (existingTable) {
          d->tables.take(existingTable->id());
          delete existingTable;
        }
      }*/

    bool res = commitAutoCommitTransaction(tg.transaction());

    if (res) {
        if (internalTable) {
            //insert the internal table into structures
            insertInternalTable(*tableSchema);
        } else {
            if (previousSchemaStillKept) {
                //remove previous table schema
                d->removeTable(*tableSchema);
            }
            //store one schema object locally:
            d->insertTable(*tableSchema);
        }
        //ok, this table is not created by the connection
        tableSchema->m_conn = this;
    }
    return res;
}

bool Connection::removeObject(uint objId)
{
    clearError();
    //remove table schema from kexi__* tables
    if (!Predicate::deleteRow(*this, d->table("kexi__objects"), "o_id", objId) //schema entry
            || !Predicate::deleteRow(*this, d->table("kexi__objectdata"), "o_id", objId)) {//data blocks
        setError(ERR_DELETE_SERVER_ERROR, tr("Could not remove object's data."));
        return false;
    }
    return true;
}

bool Connection::drv_dropTable(const QString& name)
{
    m_sql = "DROP TABLE " + escapeIdentifier(name);
    return executeSQL(m_sql);
}

//! Drops a table corresponding to the name in the given schema
/*! Drops a table according to the name given by the TableSchema, removing the
    table and column definitions to kexi__* tables.  Checks first that the
    table is not a system table.

    TODO: Should check that a database is currently in use? (c.f. createTable)
*/
tristate Connection::dropTable(Predicate::TableSchema* tableSchema)
{
    return dropTable(tableSchema, true);
}

tristate Connection::dropTable(Predicate::TableSchema* tableSchema, bool alsoRemoveSchema)
{
// Each SQL identifier needs to be escaped in the generated query.
    clearError();
    if (!tableSchema)
        return false;

    QString errmsg = tr("Table \"%1\" cannot be removed.\n");
    //be sure that we handle the correct TableSchema object:
    if (tableSchema->id() < 0
            || this->tableSchema(tableSchema->name()) != tableSchema
            || this->tableSchema(tableSchema->id()) != tableSchema) {
        setError(ERR_OBJECT_NOT_FOUND, errmsg.arg(tableSchema->name())
                 + tr("Unexpected name or identifier."));
        return false;
    }

    tristate res = closeAllTableSchemaChangeListeners(*tableSchema);
    if (true != res)
        return res;

    //sanity checks:
    if (m_driver->isSystemObjectName(tableSchema->name())) {
        setError(ERR_SYSTEM_NAME_RESERVED, errmsg.arg(tableSchema->name())
                 + d->strItIsASystemObject());
        return false;
    }

    TransactionGuard tg;
    if (!beginAutoCommitTransaction(tg))
        return false;

    //for sanity we're checking if this table exists physically
    if (drv_containsTable(tableSchema->name())) {
        if (!drv_dropTable(tableSchema->name()))
            return false;
    }

    TableSchema *ts = d->table("kexi__fields");
    if (!Predicate::deleteRow(*this, ts, "t_id", tableSchema->id())) //field entries
        return false;

    //remove table schema from kexi__objects table
    if (!removeObject(tableSchema->id())) {
        return false;
    }

    if (alsoRemoveSchema) {
//! \todo js: update any structure (e.g. queries) that depend on this table!
        tristate res = removeDataBlock(tableSchema->id(), "extended_schema");
        if (!res)
            return false;
        d->removeTable(*tableSchema);
    }
    return commitAutoCommitTransaction(tg.transaction());
}

tristate Connection::dropTable(const QString& table)
{
    clearError();
    TableSchema* ts = tableSchema(table);
    if (!ts) {
        setError(ERR_OBJECT_NOT_FOUND, tr("Table \"%1\" does not exist.")
                                       .arg(table));
        return false;
    }
    return dropTable(ts);
}

tristate Connection::alterTable(TableSchema& tableSchema, TableSchema& newTableSchema)
{
    clearError();
    tristate res = closeAllTableSchemaChangeListeners(tableSchema);
    if (true != res)
        return res;

    if (&tableSchema == &newTableSchema) {
        setError(ERR_OBJECT_THE_SAME, tr("Could not alter table \"%1\" using the same table.")
                                           .arg(tableSchema.name()));
        return false;
    }
//TODO(js): implement real altering
//TODO(js): update any structure (e.g. query) that depend on this table!
    bool ok, empty;
#if 0//TODO ucomment:
    empty = isEmpty(tableSchema, ok) && ok;
#else
    empty = true;
#endif
    if (empty) {
        ok = createTable(&newTableSchema, true/*replace*/);
    }
    return ok;
}

bool Connection::alterTableName(TableSchema& tableSchema, const QString& newName, bool replace)
{
    clearError();
    if (&tableSchema != d->table(tableSchema.id())) {
        setError(ERR_OBJECT_NOT_FOUND, tr("Unknown table \"%1\"").arg(tableSchema.name()));
        return false;
    }
    if (newName.isEmpty() || !Utils::isIdentifier(newName)) {
        setError(ERR_INVALID_IDENTIFIER, tr("Invalid table name \"%1\"").arg(newName));
        return false;
    }
    const QString oldTableName = tableSchema.name();
    const QString newTableName = newName.toLower().trimmed();
    if (oldTableName.toLower().trimmed() == newTableName) {
        setError(ERR_OBJECT_THE_SAME, tr("Could rename table \"%1\" using the same name.")
                                      .arg(newTableName));
        return false;
    }
//TODO: alter table name for server DB backends!
//TODO: what about objects (queries/forms) that use old name?
//TODO
    TableSchema *tableToReplace = this->tableSchema(newName);
    const bool destTableExists = tableToReplace != 0;
    const int origID = destTableExists ? tableToReplace->id() : -1; //will be reused in the new table
    if (!replace && destTableExists) {
        setError(ERR_OBJECT_EXISTS,
                 tr("Could not rename table \"%1\" to \"%2\". Table \"%3\" already exists.")
                 .arg(tableSchema.name(), newName, newName));
        return false;
    }

//helper:
#define alterTableName_ERR \
    tableSchema.setName(oldTableName) //restore old name

    TransactionGuard tg;
    if (!beginAutoCommitTransaction(tg))
        return false;

    // drop the table replaced (with schema)
    if (destTableExists) {
        if (!replace) {
            return false;
        }
        if (!dropTable(newName)) {
            return false;
        }

        // the new table owns the previous table's id:
        if (!executeSQL(
                    QString::fromLatin1("UPDATE kexi__objects SET o_id=%1 WHERE o_id=%2 AND o_type=%3")
                    .arg(origID, tableSchema.id(), (int)TableObjectType)))
        {
            return false;
        }
        if (!executeSQL(QString::fromLatin1("UPDATE kexi__fields SET t_id=%1 WHERE t_id=%2")
                        .arg(origID, tableSchema.id()))) {
            return false;
        }

        //maintain table ID
        d->changeTableId(tableSchema, origID);
        tableSchema.m_id = origID;
    }

    if (!drv_alterTableName(tableSchema, newTableName)) {
        alterTableName_ERR;
        return false;
    }

    // Update kexi__objects
    //TODO
    if (!executeSQL(QString::fromLatin1("UPDATE kexi__objects SET o_name=%1 WHERE o_id=%2")
                    .arg(m_driver->escapeString(tableSchema.name()), tableSchema.id()))) {
        alterTableName_ERR;
        return false;
    }
//TODO what about caption?

    //restore old name: it will be changed soon!
    tableSchema.setName(oldTableName);

    if (!commitAutoCommitTransaction(tg.transaction())) {
        alterTableName_ERR;
        return false;
    }

    //update tableSchema:
    d->renameTable(tableSchema, newTableName);
    return true;
}

bool Connection::drv_alterTableName(TableSchema& tableSchema, const QString& newName)
{
    const QString oldTableName = tableSchema.name();
    tableSchema.setName(newName);

    if (!executeSQL(QString::fromLatin1("ALTER TABLE %1 RENAME TO %2")
                    .arg(escapeIdentifier(oldTableName), escapeIdentifier(newName)))) {
        tableSchema.setName(oldTableName); //restore old name
        return false;
    }
    return true;
}

bool Connection::dropQuery(Predicate::QuerySchema* querySchema)
{
    clearError();
    if (!querySchema)
        return false;

    TransactionGuard tg;
    if (!beginAutoCommitTransaction(tg))
        return false;

    /* TableSchema *ts = d->tables_byname["kexi__querydata"];
      if (!Predicate::deleteRow(*this, ts, "q_id", querySchema->id()))
        return false;

      ts = d->tables_byname["kexi__queryfields"];
      if (!Predicate::deleteRow(*this, ts, "q_id", querySchema->id()))
        return false;

      ts = d->tables_byname["kexi__querytables"];
      if (!Predicate::deleteRow(*this, ts, "q_id", querySchema->id()))
        return false;*/

    //remove query schema from kexi__objects table
    if (!removeObject(querySchema->id())) {
        return false;
    }

//TODO(js): update any structure that depend on this table!
    d->removeQuery(*querySchema);
    return commitAutoCommitTransaction(tg.transaction());
}

bool Connection::dropQuery(const QString& query)
{
    clearError();
    QuerySchema* qs = querySchema(query);
    if (!qs) {
        setError(ERR_OBJECT_NOT_FOUND, tr("Query \"%1\" does not exist.")
                                       .arg(query));
        return false;
    }
    return dropQuery(qs);
}

bool Connection::drv_createTable(const Predicate::TableSchema& tableSchema)
{
    m_sql = createTableStatement(tableSchema);
    PreDbg << "******** " << m_sql;
    return executeSQL(m_sql);
}

bool Connection::drv_createTable(const QString& tableSchemaName)
{
    TableSchema *ts = d->table(tableSchemaName);
    if (!ts)
        return false;
    return drv_createTable(*ts);
}

bool Connection::beginAutoCommitTransaction(TransactionGuard &tg)
{
    if ((m_driver->d->features & Driver::IgnoreTransactions)
            || !d->autoCommit) {
        tg.setTransaction(Transaction());
        return true;
    }

    // commit current transaction (if present) for drivers
    // that allow single transaction per connection
    if (m_driver->d->features & Driver::SingleTransactions) {
        if (d->default_trans_started_inside) //only commit internally started transaction
            if (!commitTransaction(d->default_trans, true)) {
                tg.setTransaction(Transaction());
                return false; //we have a real error
            }

        d->default_trans_started_inside = d->default_trans.isNull();
        if (!d->default_trans_started_inside) {
            tg.setTransaction(d->default_trans);
            tg.doNothing();
            return true; //reuse externally started transaction
        }
    } else if (!(m_driver->d->features & Driver::MultipleTransactions)) {
        tg.setTransaction(Transaction());
        return true; //no trans. supported at all - just return
    }
    tg.setTransaction(beginTransaction());
    return !error();
}

bool Connection::commitAutoCommitTransaction(const Transaction& trans)
{
    if (m_driver->d->features & Driver::IgnoreTransactions)
        return true;
    if (trans.isNull() || !m_driver->transactionsSupported())
        return true;
    if (m_driver->d->features & Driver::SingleTransactions) {
        if (!d->default_trans_started_inside) //only commit internally started transaction
            return true; //give up
    }
    return commitTransaction(trans, true);
}

bool Connection::rollbackAutoCommitTransaction(const Transaction& trans)
{
    if (trans.isNull() || !m_driver->transactionsSupported())
        return true;
    return rollbackTransaction(trans);
}

#define SET_ERR_TRANS_NOT_SUPP \
    { setError(ERR_UNSUPPORTED_DRV_FEATURE, \
                   tr("Transactions are not supported for \"%1\" driver.").arg( m_driver->name() )); }

#define SET_BEGIN_TR_ERROR \
    { if (!error()) \
            setError(ERR_ROLLBACK_OR_COMMIT_TRANSACTION, tr("Begin transaction failed")); }

Transaction Connection::beginTransaction()
{
    if (!checkIsDatabaseUsed())
        return Transaction();
    Transaction trans;
    if (m_driver->d->features & Driver::IgnoreTransactions) {
        //we're creating dummy transaction data here,
        //so it will look like active
        trans.m_data = new TransactionData(this);
        d->transactions.append(trans);
        return trans;
    }
    if (m_driver->d->features & Driver::SingleTransactions) {
        if (d->default_trans.active()) {
            setError(ERR_TRANSACTION_ACTIVE, tr("Transaction already started."));
            return Transaction();
        }
        if (!(trans.m_data = drv_beginTransaction())) {
            SET_BEGIN_TR_ERROR;
            return Transaction();
        }
        d->default_trans = trans;
        d->transactions.append(trans);
        return d->default_trans;
    }
    if (m_driver->d->features & Driver::MultipleTransactions) {
        if (!(trans.m_data = drv_beginTransaction())) {
            SET_BEGIN_TR_ERROR;
            return Transaction();
        }
        d->transactions.append(trans);
        return trans;
    }

    SET_ERR_TRANS_NOT_SUPP;
    return Transaction();
}

bool Connection::commitTransaction(const Transaction trans, bool ignore_inactive)
{
    if (!isDatabaseUsed())
        return false;
// if (!checkIsDatabaseUsed())
    //return false;
    if (!m_driver->transactionsSupported()
            && !(m_driver->d->features & Driver::IgnoreTransactions)) {
        SET_ERR_TRANS_NOT_SUPP;
        return false;
    }
    Transaction t = trans;
    if (!t.active()) { //try default tr.
        if (!d->default_trans.active()) {
            if (ignore_inactive)
                return true;
            clearError();
            setError(ERR_NO_TRANSACTION_ACTIVE, tr("Transaction not started."));
            return false;
        }
        t = d->default_trans;
        d->default_trans = Transaction(); //now: no default tr.
    }
    bool ret = true;
    if (!(m_driver->d->features & Driver::IgnoreTransactions))
        ret = drv_commitTransaction(t.m_data);
    if (t.m_data)
        t.m_data->m_active = false; //now this transaction if inactive
    if (!d->dont_remove_transactions) //true=transaction obj will be later removed from list
        d->transactions.removeAt(d->transactions.indexOf(t));
    if (!ret && !error())
        setError(ERR_ROLLBACK_OR_COMMIT_TRANSACTION, tr("Error on commit transaction"));
    return ret;
}

bool Connection::rollbackTransaction(const Transaction trans, bool ignore_inactive)
{
    if (!isDatabaseUsed())
        return false;
// if (!checkIsDatabaseUsed())
//  return false;
    if (!m_driver->transactionsSupported()
            && !(m_driver->d->features & Driver::IgnoreTransactions)) {
        SET_ERR_TRANS_NOT_SUPP;
        return false;
    }
    Transaction t = trans;
    if (!t.active()) { //try default tr.
        if (!d->default_trans.active()) {
            if (ignore_inactive)
                return true;
            clearError();
            setError(ERR_NO_TRANSACTION_ACTIVE, tr("Transaction not started."));
            return false;
        }
        t = d->default_trans;
        d->default_trans = Transaction(); //now: no default tr.
    }
    bool ret = true;
    if (!(m_driver->d->features & Driver::IgnoreTransactions))
        ret = drv_rollbackTransaction(t.m_data);
    if (t.m_data)
        t.m_data->m_active = false; //now this transaction if inactive
    if (!d->dont_remove_transactions) //true=transaction obj will be later removed from list
        d->transactions.removeAt(d->transactions.indexOf(t));
    if (!ret && !error())
        setError(ERR_ROLLBACK_OR_COMMIT_TRANSACTION, tr("Error on rollback transaction"));
    return ret;
}

#undef SET_ERR_TRANS_NOT_SUPP
#undef SET_BEGIN_TR_ERROR

/*bool Connection::duringTransaction()
{
  return drv_duringTransaction();
}*/

Transaction& Connection::defaultTransaction() const
{
    return d->default_trans;
}

void Connection::setDefaultTransaction(const Transaction& trans)
{
    if (!isDatabaseUsed())
        return;
// if (!checkIsDatabaseUsed())
    // return;
    if (!(m_driver->d->features & Driver::IgnoreTransactions)
            && (!trans.active() || !m_driver->transactionsSupported())) {
        return;
    }
    d->default_trans = trans;
}

const QList<Transaction>& Connection::transactions()
{
    return d->transactions;
}

bool Connection::autoCommit() const
{
    return d->autoCommit;
}

bool Connection::setAutoCommit(bool on)
{
    if (d->autoCommit == on || m_driver->d->features & Driver::IgnoreTransactions)
        return true;
    if (!drv_setAutoCommit(on))
        return false;
    d->autoCommit = on;
    return true;
}

TransactionData* Connection::drv_beginTransaction()
{
    QString old_sql = m_sql; //don't
    if (!executeSQL("BEGIN"))
        return 0;
    return new TransactionData(this);
}

bool Connection::drv_commitTransaction(TransactionData *)
{
    return executeSQL("COMMIT");
}

bool Connection::drv_rollbackTransaction(TransactionData *)
{
    return executeSQL("ROLLBACK");
}

bool Connection::drv_setAutoCommit(bool /*on*/)
{
    return true;
}

Cursor* Connection::executeQuery(const QString& statement, uint cursor_options)
{
    if (statement.isEmpty())
        return 0;
    Cursor *c = prepareQuery(statement, cursor_options);
    if (!c)
        return 0;
    if (!c->open()) {//err - kill that
        setError(c);
        delete c;
        return 0;
    }
    return c;
}

Cursor* Connection::executeQuery(QuerySchema& query, const QList<QVariant>& params,
                                 uint cursor_options)
{
    Cursor *c = prepareQuery(query, params, cursor_options);
    if (!c)
        return 0;
    if (!c->open()) {//err - kill that
        setError(c);
        delete c;
        return 0;
    }
    return c;
}

Cursor* Connection::executeQuery(QuerySchema& query, uint cursor_options)
{
    return executeQuery(query, QList<QVariant>(), cursor_options);
}

Cursor* Connection::executeQuery(TableSchema& table, uint cursor_options)
{
    return executeQuery(*table.query(), cursor_options);
}

Cursor* Connection::prepareQuery(TableSchema& table, uint cursor_options)
{
    return prepareQuery(*table.query(), cursor_options);
}

Cursor* Connection::prepareQuery(QuerySchema& query, const QList<QVariant>& params,
                                 uint cursor_options)
{
    Cursor* cursor = prepareQuery(query, cursor_options);
    if (cursor)
        cursor->setQueryParameters(params);
    return cursor;
}

bool Connection::deleteCursor(Cursor *cursor)
{
    if (!cursor)
        return false;
    if (cursor->connection() != this) {//illegal call
        PreWarn << "Connection::deleteCursor(): Cannot delete the cursor not owned by the same connection!";
        return false;
    }
    const bool ret = cursor->close();
    delete cursor;
    return ret;
}

bool Connection::setupObjectSchemaData(const RecordData &data, SchemaData &sdata)
{
    //not found: retrieve schema
    /* Predicate::Cursor *cursor;
      if (!(cursor = executeQuery( QString("select * from kexi__objects where o_id='%1'").arg(objId) )))
        return false;
      if (!cursor->moveFirst()) {
        deleteCursor(cursor);
        return false;
      }*/
    //if (!ok) {
    //deleteCursor(cursor);
    //return 0;
// }
    bool ok;
    sdata.m_id = data[0].toInt(&ok);
    if (!ok) {
        return false;
    }
    sdata.m_name = data[2].toString();
    if (!Utils::isIdentifier(sdata.m_name)) {
        setError(ERR_INVALID_IDENTIFIER, tr("Invalid object name \"%1\"").arg(sdata.m_name));
        return false;
    }
    sdata.m_caption = data[3].toString();
    sdata.m_desc = data[4].toString();

// PreDbg<<"@@@ Connection::setupObjectSchemaData() == " << sdata.schemaDataDebugString();
    return true;
}

tristate Connection::loadObjectSchemaData(int objectID, SchemaData &sdata)
{
    RecordData data;
    if (true != querySingleRecord(QString::fromLatin1(
                                      "SELECT o_id, o_type, o_name, o_caption, o_desc FROM kexi__objects WHERE o_id=%1")
                                  .arg(objectID), data))
        return cancelled;
    return setupObjectSchemaData(data, sdata);
}

tristate Connection::loadObjectSchemaData(int objectType, const QString& objectName, SchemaData &sdata)
{
    RecordData data;
    if (true != querySingleRecord(QString::fromLatin1("SELECT o_id, o_type, o_name, o_caption, o_desc "
                                  "FROM kexi__objects WHERE o_type=%1 AND lower(o_name)=%2")
                                  .arg(QString::number(objectType), 
                                       m_driver->valueToSQL(Field::Text, objectName.toLower())), data))
        return cancelled;
    return setupObjectSchemaData(data, sdata);
}

bool Connection::storeObjectSchemaData(SchemaData &sdata, bool newObject)
{
    TableSchema *ts = d->table("kexi__objects");
    if (!ts)
        return false;
    if (newObject) {
        int existingID;
        if (true == querySingleNumber(QString::fromLatin1(
                                          "SELECT o_id FROM kexi__objects WHERE o_type=%1 AND lower(o_name)=%2")
                                      .arg(QString::number(sdata.type()), m_driver->valueToSQL(Field::Text, sdata.name().toLower())), existingID)) {
            //we already have stored a schema data with the same name and type:
            //just update it's properties as it would be existing object
            sdata.m_id = existingID;
            newObject = false;
        }
    }
    if (newObject) {
        FieldList *fl;
        bool ok;
        if (sdata.id() <= 0) {//get new ID
            fl = ts->subList("o_type", "o_name", "o_caption", "o_desc");
            ok = fl != 0;
            if (ok && !insertRecord(*fl, QVariant(sdata.type()), QVariant(sdata.name()),
                                    QVariant(sdata.caption()), QVariant(sdata.description())))
                ok = false;
            delete fl;
            if (!ok)
                return false;
            //fetch newly assigned ID
//! @todo safe to cast it?
            int obj_id = (int)lastInsertedAutoIncValue("o_id", *ts);
            PreDbg << "######## NEW obj_id == " << obj_id;
            if (obj_id <= 0)
                return false;
            sdata.m_id = obj_id;
            return true;
        } else {
            fl = ts->subList("o_id", "o_type", "o_name", "o_caption", "o_desc");
            ok = fl != 0;
            if (ok && !insertRecord(*fl, QVariant(sdata.id()), QVariant(sdata.type()), QVariant(sdata.name()),
                                    QVariant(sdata.caption()), QVariant(sdata.description())))
                ok = false;
            delete fl;
            return ok;
        }
    }
    //existing object:
    return executeSQL(
               QString::fromLatin1("UPDATE kexi__objects SET o_type=%2, o_caption=%3, o_desc=%4 WHERE o_id=%1")
               .arg(QString::number(sdata.id()), QString::number(sdata.type()),
                    m_driver->valueToSQL(Predicate::Field::Text, sdata.caption()),
                    m_driver->valueToSQL(Predicate::Field::Text, sdata.description())));
}

tristate Connection::querySingleRecordInternal(RecordData &data, const QString* sql, QuerySchema* query,
        bool addLimitTo1)
{
    Q_ASSERT(sql || query);
//! @todo does not work with non-SQL data sources
    if (sql)
        m_sql = m_driver->addLimitTo1(*sql, addLimitTo1);
    Predicate::Cursor *cursor;
    if (!(cursor = sql ? executeQuery(m_sql) : executeQuery(*query))) {
        PreWarn << "Connection::querySingleRecord(): !executeQuery() " << m_sql;
        return false;
    }
    if (!cursor->moveFirst()
            || cursor->eof()
            || !cursor->storeCurrentRow(data)) {
        const tristate result = cursor->error() ? false : cancelled;
        PreWarn << "Connection::querySingleRecord(): !cursor->moveFirst() || cursor->eof() || cursor->storeCurrentRow(data) m_sql=" << m_sql;
        setError(cursor);
        deleteCursor(cursor);
        return result;
    }
    return deleteCursor(cursor);
}

tristate Connection::querySingleRecord(const QString& sql, RecordData &data, bool addLimitTo1)
{
    return querySingleRecordInternal(data, &sql, 0, addLimitTo1);
}

tristate Connection::querySingleRecord(QuerySchema& query, RecordData &data, bool addLimitTo1)
{
    return querySingleRecordInternal(data, 0, &query, addLimitTo1);
}

bool Connection::checkIfColumnExists(Cursor *cursor, uint column)
{
    if (column >= cursor->fieldCount()) {
        setError(ERR_CURSOR_RECORD_FETCHING, tr("Column %1 does not exist for the query.").arg(column));
        return false;
    }
    return true;
}

tristate Connection::querySingleString(const QString& sql, QString &value, uint column, bool addLimitTo1)
{
    Predicate::Cursor *cursor;
    m_sql = m_driver->addLimitTo1(sql, addLimitTo1);
    if (!(cursor = executeQuery(m_sql))) {
        PreWarn << "Connection::querySingleRecord(): !executeQuery() " << m_sql;
        return false;
    }
    if (!cursor->moveFirst() || cursor->eof()) {
        const tristate result = cursor->error() ? false : cancelled;
        PreWarn << "Connection::querySingleRecord(): !cursor->moveFirst() || cursor->eof() " << m_sql;
        deleteCursor(cursor);
        return result;
    }
    if (!checkIfColumnExists(cursor, column)) {
        deleteCursor(cursor);
        return false;
    }
    value = cursor->value(column).toString();
    return deleteCursor(cursor);
}

tristate Connection::querySingleNumber(const QString& sql, int &number, uint column, bool addLimitTo1)
{
    static QString str;
    static bool ok;
    const tristate result = querySingleString(sql, str, column, addLimitTo1);
    if (result != true)
        return result;
    number = str.toInt(&ok);
    return ok;
}

bool Connection::queryStringList(const QString& sql, QStringList& list, uint column)
{
    Predicate::Cursor *cursor;
    clearError();
    m_sql = sql;
    if (!(cursor = executeQuery(m_sql))) {
        PreWarn << "Connection::queryStringList(): !executeQuery() " << m_sql;
        return false;
    }
    cursor->moveFirst();
    if (cursor->error()) {
        setError(cursor);
        deleteCursor(cursor);
        return false;
    }
    if (!cursor->eof() && !checkIfColumnExists(cursor, column)) {
        deleteCursor(cursor);
        return false;
    }
    list.clear();
    while (!cursor->eof()) {
        list.append(cursor->value(column).toString());
        if (!cursor->moveNext() && cursor->error()) {
            setError(cursor);
            deleteCursor(cursor);
            return false;
        }
    }
    return deleteCursor(cursor);
}

bool Connection::resultExists(const QString& sql, bool &success, bool addLimitTo1)
{
    Predicate::Cursor *cursor;
    //optimization
    if (m_driver->beh->SELECT_1_SUBQUERY_SUPPORTED) {
        //this is at least for sqlite
        if (addLimitTo1 && sql.left(6).toUpper() == "SELECT")
            m_sql = m_driver->addLimitTo1(QString::fromLatin1("SELECT 1 FROM (") + sql + ")", addLimitTo1);
        else
            m_sql = sql;
    } else {
        if (addLimitTo1 && sql.left(6).toUpper() == "SELECT")
            m_sql = m_driver->addLimitTo1(sql, addLimitTo1);
        else
            m_sql = sql;
    }
    if (!(cursor = executeQuery(m_sql))) {
        PreWarn << "Connection::querySingleRecord(): !executeQuery() " << m_sql;
        success = false;
        return false;
    }
    if (!cursor->moveFirst() || cursor->eof()) {
        success = !cursor->error();
        PreWarn << "Connection::querySingleRecord(): !cursor->moveFirst() || cursor->eof() " << m_sql;
        setError(cursor);
        deleteCursor(cursor);
        return false;
    }
    success = deleteCursor(cursor);
    return true;
}

bool Connection::isEmpty(TableSchema& table, bool &success)
{
    return !resultExists(selectStatement(*table.query()), success);
}

//! Used by addFieldPropertyToExtendedTableSchemaData()
static void createExtendedTableSchemaMainElementIfNeeded(
    QDomDocument& doc, QDomElement& extendedTableSchemaMainEl,
    bool& extendedTableSchemaStringIsEmpty)
{
    if (!extendedTableSchemaStringIsEmpty)
        return;
    //init document
    extendedTableSchemaMainEl = doc.createElement("EXTENDED_TABLE_SCHEMA");
    doc.appendChild(extendedTableSchemaMainEl);
    extendedTableSchemaMainEl.setAttribute("version", QString::number(PREDICATE_EXTENDED_TABLE_SCHEMA_VERSION));
    extendedTableSchemaStringIsEmpty = false;
}

//! Used by addFieldPropertyToExtendedTableSchemaData()
static void createExtendedTableSchemaFieldElementIfNeeded(QDomDocument& doc,
        QDomElement& extendedTableSchemaMainEl, const QString& fieldName, QDomElement& extendedTableSchemaFieldEl,
        bool append = true)
{
    if (!extendedTableSchemaFieldEl.isNull())
        return;
    extendedTableSchemaFieldEl = doc.createElement("field");
    if (append)
        extendedTableSchemaMainEl.appendChild(extendedTableSchemaFieldEl);
    extendedTableSchemaFieldEl.setAttribute("name", fieldName);
}

/*! @internal used by storeExtendedTableSchemaData()
 Creates DOM node for \a propertyName and \a propertyValue.
 Creates enclosing EXTENDED_TABLE_SCHEMA element if EXTENDED_TABLE_SCHEMA is true.
 Updates extendedTableSchemaStringIsEmpty and extendedTableSchemaMainEl afterwards.
 If extendedTableSchemaFieldEl is null, creates <field> element (with optional
 "custom" attribute is \a custom is false). */
static void addFieldPropertyToExtendedTableSchemaData(
    Field *f, const char* propertyName, const QVariant& propertyValue,
    QDomDocument& doc, QDomElement& extendedTableSchemaMainEl,
    QDomElement& extendedTableSchemaFieldEl,
    bool& extendedTableSchemaStringIsEmpty,
    bool custom = false)
{
    createExtendedTableSchemaMainElementIfNeeded(doc,
            extendedTableSchemaMainEl, extendedTableSchemaStringIsEmpty);
    createExtendedTableSchemaFieldElementIfNeeded(
        doc, extendedTableSchemaMainEl, f->name(), extendedTableSchemaFieldEl);

    //create <property>
    QDomElement extendedTableSchemaFieldPropertyEl = doc.createElement("property");
    extendedTableSchemaFieldEl.appendChild(extendedTableSchemaFieldPropertyEl);
    if (custom)
        extendedTableSchemaFieldPropertyEl.setAttribute("custom", "true");
    extendedTableSchemaFieldPropertyEl.setAttribute("name", propertyName);
    QDomElement extendedTableSchemaFieldPropertyValueEl;
    switch (propertyValue.type()) {
    case QVariant::String:
        extendedTableSchemaFieldPropertyValueEl = doc.createElement("string");
        break;
    case QVariant::ByteArray:
        extendedTableSchemaFieldPropertyValueEl = doc.createElement("cstring");
        break;
    case QVariant::Int:
    case QVariant::Double:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:
        extendedTableSchemaFieldPropertyValueEl = doc.createElement("number");
        break;
    case QVariant::Bool:
        extendedTableSchemaFieldPropertyValueEl = doc.createElement("bool");
        break;
    default:
//! @todo add more QVariant types
        PreFatal << "addFieldPropertyToExtendedTableSchemaData(): impl. error";
    }
    extendedTableSchemaFieldPropertyEl.appendChild(extendedTableSchemaFieldPropertyValueEl);
    extendedTableSchemaFieldPropertyValueEl.appendChild(
        doc.createTextNode(propertyValue.toString()));
}

bool Connection::storeExtendedTableSchemaData(TableSchema& tableSchema)
{
//! @todo future: save in older versions if neeed
    QDomDocument doc("EXTENDED_TABLE_SCHEMA");
    QDomElement extendedTableSchemaMainEl;
    bool extendedTableSchemaStringIsEmpty = true;

    //for each field:
    foreach(Field* f, *tableSchema.fields()) {
        QDomElement extendedTableSchemaFieldEl;
        if (f->visibleDecimalPlaces() >= 0/*nondefault*/ && Predicate::supportsVisibleDecimalPlacesProperty(f->type())) {
            addFieldPropertyToExtendedTableSchemaData(
                f, "visibleDecimalPlaces", f->visibleDecimalPlaces(), doc,
                extendedTableSchemaMainEl, extendedTableSchemaFieldEl,
                extendedTableSchemaStringIsEmpty);
        }
        // boolean field with "not null"

        // add custom properties
        const Field::CustomPropertiesMap customProperties(f->customProperties());
        for (Field::CustomPropertiesMap::ConstIterator itCustom = customProperties.constBegin();
                itCustom != customProperties.constEnd(); ++itCustom) {
            addFieldPropertyToExtendedTableSchemaData(
                f, itCustom.key(), itCustom.value(), doc,
                extendedTableSchemaMainEl, extendedTableSchemaFieldEl, extendedTableSchemaStringIsEmpty,
                /*custom*/true);
        }
        // save lookup table specification, if present
        LookupFieldSchema *lookupFieldSchema = tableSchema.lookupFieldSchema(*f);
        if (lookupFieldSchema) {
            createExtendedTableSchemaFieldElementIfNeeded(
                doc, extendedTableSchemaMainEl, f->name(), extendedTableSchemaFieldEl, false/* !append */);
            LookupFieldSchema::saveToDom(*lookupFieldSchema, doc, extendedTableSchemaFieldEl);

            if (extendedTableSchemaFieldEl.hasChildNodes()) {
                // this element provides the definition, so let's append it now
                createExtendedTableSchemaMainElementIfNeeded(doc, extendedTableSchemaMainEl,
                        extendedTableSchemaStringIsEmpty);
                extendedTableSchemaMainEl.appendChild(extendedTableSchemaFieldEl);
            }
        }
    }

    // Store extended schema information (see ExtendedTableSchemaInformation in Kexi Wiki)
    if (extendedTableSchemaStringIsEmpty) {
#ifdef KEXI_DEBUG_GUI
        Utils::addAlterTableActionDebug(QString("** Extended table schema REMOVED."));
#endif
        if (!removeDataBlock(tableSchema.id(), "extended_schema"))
            return false;
    } else {
#ifdef KEXI_DEBUG_GUI
        Utils::addAlterTableActionDebug(QString("** Extended table schema set to:\n") + doc.toString(4));
#endif
        if (!storeDataBlock(tableSchema.id(), doc.toString(1), "extended_schema"))
            return false;
    }
    return true;
}

bool Connection::loadExtendedTableSchemaData(TableSchema& tableSchema)
{
#define loadExtendedTableSchemaData_ERR \
    { setError(tr("Error while loading extended table schema information.")); \
        return false; }
#define loadExtendedTableSchemaData_ERR2(details) \
    { setError(tr("Error while loading extended table schema information."), details); \
        return false; }
#define loadExtendedTableSchemaData_ERR3(data) \
    { setError(tr("Error while loading extended table schema information."), \
                   tr("Invalid XML data: ") + data.left(1024) ); \
        return false; }

    // Load extended schema information, if present (see ExtendedTableSchemaInformation in Kexi Wiki)
    QString extendedTableSchemaString;
    tristate res = loadDataBlock(tableSchema.id(), extendedTableSchemaString, "extended_schema");
    if (!res)
        loadExtendedTableSchemaData_ERR;
    // extendedTableSchemaString will be just empty if there is no such data block

#ifdef PREDICATE_LOOKUP_FIELD_TEST
//<temp. for LookupFieldSchema tests>
    if (tableSchema.name() == "cars") {
        LookupFieldSchema *lookupFieldSchema = new LookupFieldSchema();
        lookupFieldSchema->rowSource().setType(LookupFieldSchema::RowSource::Table);
        lookupFieldSchema->rowSource().setName("persons");
        lookupFieldSchema->setBoundColumn(0); //id
        lookupFieldSchema->setVisibleColumn(3); //surname
        tableSchema.setLookupFieldSchema("owner", lookupFieldSchema);
    }
//</temp. for LookupFieldSchema tests>
#endif

    if (extendedTableSchemaString.isEmpty())
        return true;

    QDomDocument doc;
    QString errorMsg;
    int errorLine, errorColumn;
    if (!doc.setContent(extendedTableSchemaString, &errorMsg, &errorLine, &errorColumn))
        loadExtendedTableSchemaData_ERR2(tr("Error in XML data: \"%1\" in line %2, column %3.\nXML data: ")
            .arg(errorMsg, errorLine, errorColumn) + extendedTableSchemaString.left(1024));

//! @todo look at the current format version (PREDICATE_EXTENDED_TABLE_SCHEMA_VERSION)

    if (doc.doctype().name() != "EXTENDED_TABLE_SCHEMA")
        loadExtendedTableSchemaData_ERR3(extendedTableSchemaString);

    QDomElement docEl = doc.documentElement();
    if (docEl.tagName() != "EXTENDED_TABLE_SCHEMA")
        loadExtendedTableSchemaData_ERR3(extendedTableSchemaString);

    for (QDomNode n = docEl.firstChild(); !n.isNull(); n = n.nextSibling()) {
        QDomElement fieldEl = n.toElement();
        if (fieldEl.tagName() == "field") {
            Field *f = tableSchema.field(fieldEl.attribute("name"));
            if (f) {
                //set properties of the field:
//! @todo more properties
                for (QDomNode propNode = fieldEl.firstChild();
                        !propNode.isNull(); propNode = propNode.nextSibling()) {
                    QDomElement propEl = propNode.toElement();
                    bool ok;
                    int intValue;
                    if (propEl.tagName() == "property") {
                        QByteArray propertyName = propEl.attribute("name").toLatin1();
                        if (propEl.attribute("custom") == "true") {
                            //custom property
                            f->setCustomProperty(propertyName,
                                                 Predicate::loadPropertyValueFromDom(propEl.firstChild()));
                        } else if (propertyName == "visibleDecimalPlaces"
                                   && Predicate::supportsVisibleDecimalPlacesProperty(f->type())) {
                            intValue = Predicate::loadIntPropertyValueFromDom(propEl.firstChild(), &ok);
                            if (ok)
                                f->setVisibleDecimalPlaces(intValue);
                        }
//! @todo more properties...
                    } else if (propEl.tagName() == "lookup-column") {
                        LookupFieldSchema *lookupFieldSchema = LookupFieldSchema::loadFromDom(propEl);
                        if (lookupFieldSchema)
                            lookupFieldSchema->debug();
                        tableSchema.setLookupFieldSchema(f->name(), lookupFieldSchema);
                    }
                }
            } else {
                PreWarn << "Connection::loadExtendedTableSchemaData(): no such field \""
                << fieldEl.attribute("name") << "\" in table \"" << tableSchema.name() << "\"";
            }
        }
    }

    return true;
}

Predicate::Field* Connection::setupField(const RecordData &data)
{
    bool ok = true;
    int f_int_type = data.at(1).toInt(&ok);
    if (f_int_type <= Field::InvalidType || f_int_type > Field::LastType)
        ok = false;
    if (!ok)
        return 0;
    Field::Type f_type = (Field::Type)f_int_type;
    int f_len = qMax(0, data.at(3).toInt(&ok));
    if (!ok)
        return 0;
    int f_prec = data.at(4).toInt(&ok);
    if (!ok)
        return 0;
    int f_constr = data.at(5).toInt(&ok);
    if (!ok)
        return 0;
    int f_opts = data.at(6).toInt(&ok);
    if (!ok)
        return 0;

    if (!Utils::isIdentifier(data.at(2).toString())) {
        setError(ERR_INVALID_IDENTIFIER, tr("Invalid object name \"%1\"")
                                         .arg(data.at(2).toString()));
        ok = false;
        return 0;
    }

    Field *f = new Field(
        data.at(2).toString(), f_type, f_constr, f_opts, f_len, f_prec);

    f->setDefaultValue(Predicate::stringToVariant(data.at(7).toString(), Field::variantType(f_type), ok));
    if (!ok) {
        PreWarn << "Connection::setupTableSchema() problem with Predicate::stringToVariant("
        << data.at(7).toString() << ")";
    }
    ok = true; //problem with defaultValue is not critical

    f->m_caption = data.at(9).toString();
    f->m_desc = data.at(10).toString();
    return f;
}

Predicate::TableSchema* Connection::setupTableSchema(const RecordData &data)
{
    TableSchema *t = new TableSchema(this);
    if (!setupObjectSchemaData(data, *t)) {
        delete t;
        return 0;
    }

    Predicate::Cursor *cursor;
    if (!(cursor = executeQuery(
                       QString::fromLatin1("SELECT t_id, f_type, f_name, f_length, f_precision, f_constraints, "
                                           "f_options, f_default, f_order, f_caption, f_help"
                                           " FROM kexi__fields WHERE t_id=%1 ORDER BY f_order").arg(t->m_id)))) {
        delete t;
        return 0;
    }
    if (!cursor->moveFirst()) {
        if (!cursor->error() && cursor->eof()) {
            setError(tr("Table has no fields defined."));
        }
        deleteCursor(cursor);
        delete t;
        return 0;
    }

    // For each field: load its schema
    RecordData fieldData;
    bool ok = true;
    while (!cursor->eof()) {
//  PreDbg<<"@@@ f_name=="<<cursor->value(2).asCString();
        if (!cursor->storeCurrentRow(fieldData)) {
            ok = false;
            break;
        }
        Field *f = setupField(fieldData);
        if (!f) {
            ok = false;
            break;
        }
        t->addField(f);
        cursor->moveNext();
    }

    if (!ok) {//error:
        deleteCursor(cursor);
        delete t;
        return 0;
    }

    if (!deleteCursor(cursor)) {
        delete t;
        return 0;
    }

    if (!loadExtendedTableSchemaData(*t)) {
        delete t;
        return 0;
    }
    //store locally:
    d->insertTable(*t);
    return t;
}

TableSchema* Connection::tableSchema(const QString& tableName)
{
    TableSchema *t = d->table(tableName);
    if (t)
        return t;
    //not found: retrieve schema
    RecordData data;
    if (true != querySingleRecord(QString::fromLatin1(
                                      "SELECT o_id, o_type, o_name, o_caption, o_desc FROM kexi__objects WHERE lower(o_name)='%1'"
                                      " AND o_type=%2")
                                  .arg(tableName, Predicate::TableObjectType), data))
        return 0;

    return setupTableSchema(data);
}

TableSchema* Connection::tableSchema(int tableId)
{
    TableSchema *t = d->table(tableId);
    if (t)
        return t;
    //not found: retrieve schema
    RecordData data;
    if (true != querySingleRecord(QString::fromLatin1(
                                      "SELECT o_id, o_type, o_name, o_caption, o_desc FROM kexi__objects WHERE o_id=%1")
                                  .arg(tableId), data))
        return 0;

    return setupTableSchema(data);
}

tristate Connection::loadDataBlock(int objectID, QString &dataString, const QString& dataID)
{
    if (objectID <= 0)
        return false;
    return querySingleString(
               QString("SELECT o_data FROM kexi__objectdata WHERE o_id=") + QString::number(objectID)
               + " AND " + Predicate::sqlWhere(m_driver, Predicate::Field::Text, "o_sub_id",
                                               dataID.isEmpty() ? QVariant() : QVariant(dataID)),
               dataString);
}

bool Connection::storeDataBlock(int objectID, const QString &dataString, const QString& dataID)
{
    if (objectID <= 0)
        return false;
    QString sql(QString::fromLatin1(
                    "SELECT kexi__objectdata.o_id FROM kexi__objectdata WHERE o_id=%1").arg(objectID));
    QString sql_sub(Predicate::sqlWhere(m_driver, Predicate::Field::Text, "o_sub_id", 
                                        dataID.isEmpty() ? QVariant() : QVariant(dataID)));

    bool ok, exists;
    exists = resultExists(sql + " and " + sql_sub, ok);
    if (!ok)
        return false;
    if (exists) {
        return executeSQL("UPDATE kexi__objectdata SET o_data="
                          + m_driver->valueToSQL(Predicate::Field::LongText, dataString)
                          + " WHERE o_id=" + QString::number(objectID) + " AND " + sql_sub);
    }
    return executeSQL(
               QString::fromLatin1("INSERT INTO kexi__objectdata (o_id, o_data, o_sub_id) VALUES (")
               + QString::number(objectID) + "," + m_driver->valueToSQL(Predicate::Field::LongText, dataString)
               + "," + m_driver->valueToSQL(Predicate::Field::Text, dataID) + ")");
}

bool Connection::removeDataBlock(int objectID, const QString& dataID)
{
    if (objectID <= 0)
        return false;
    if (dataID.isEmpty())
        return Predicate::deleteRow(*this, "kexi__objectdata", "o_id", QString::number(objectID));
    else
        return Predicate::deleteRow(*this, "kexi__objectdata",
                                 "o_id", Predicate::Field::Integer, objectID, "o_sub_id", Predicate::Field::Text, dataID);
}

Predicate::QuerySchema* Connection::setupQuerySchema(const RecordData &data)
{
    bool ok = true;
    const int objID = data[0].toInt(&ok);
    if (!ok)
        return false;
    QString sqlText;
    if (!loadDataBlock(objID, sqlText, "sql")) {
        setError(ERR_OBJECT_NOT_FOUND,
                 tr("Could not find definition for query \"%1\". Removing this query is recommended.")
                 .arg(data[2].toString()));
        return 0;
    }
    d->parser()->parse(sqlText);
    Predicate::QuerySchema *query = d->parser()->query();
    //error?
    if (!query) {
        setError(ERR_SQL_PARSE_ERROR,
                 tr("<p>Could not load definition for query \"%1\". "
                    "SQL statement for this query is invalid:<br><tt>%2</tt></p>\n"
                    "<p>You can open this query in Text View and correct it.</p>")
                    .arg(data[2].toString(), d->parser()->statement()));
        return 0;
    }
    if (!setupObjectSchemaData(data, *query)) {
        delete query;
        return 0;
    }
    d->insertQuery(*query);
    return query;
}

QuerySchema* Connection::querySchema(const QString& queryName)
{
    QString m_queryName = queryName.toLower();
    QuerySchema *q = d->query(m_queryName);
    if (q)
        return q;
    //not found: retrieve schema
    RecordData data;
    if (true != querySingleRecord(QString::fromLatin1(
                                      "SELECT o_id, o_type, o_name, o_caption, o_desc FROM kexi__objects WHERE lower(o_name)='%1'"
                                      " AND o_type=%2")
                                  .arg(m_queryName, Predicate::QueryObjectType), data))
        return 0;

    return setupQuerySchema(data);
}

QuerySchema* Connection::querySchema(int queryId)
{
    QuerySchema *q = d->query(queryId);
    if (q)
        return q;
    //not found: retrieve schema
    clearError();
    RecordData data;
    if (true != querySingleRecord(QString::fromLatin1(
                                      "SELECT o_id, o_type, o_name, o_caption, o_desc FROM kexi__objects WHERE o_id=%1")
                                  .arg(queryId), data))
        return 0;

    return setupQuerySchema(data);
}

bool Connection::setQuerySchemaObsolete(const QString& queryName)
{
    QuerySchema* oldQuery = querySchema(queryName);
    if (!oldQuery)
        return false;
    d->setQueryObsolete(*oldQuery);
    return true;
}

void Connection::insertInternalTable(TableSchema& tableSchema)
{
    d->insertInternalTable(tableSchema);
}

TableSchema* Connection::newKexiDBSystemTableSchema(const QString& tsname)
{
    TableSchema *ts = new TableSchema(tsname.toLower());
    insertInternalTable(*ts);
    return ts;
}

bool Connection::isInternalTableSchema(const QString& tableName)
{
    return (d->predicateSystemTables().contains(d->table(tableName)))
           // these are here for compatiblility because we're no longer instantiate
           // them but can exist in projects created with previous Kexi versions:
           || tableName == "kexi__final" || tableName == "kexi__useractions";
}

//! Creates kexi__* tables.
bool Connection::setupKexiDBSystemSchema()
{
    if (!d->predicateSystemTables().isEmpty())
        return true; //already set up

    TableSchema *t_objects = newKexiDBSystemTableSchema("kexi__objects");
    t_objects->addField(new Field("o_id", Field::Integer, Field::PrimaryKey | Field::AutoInc, Field::Unsigned))
    .addField(new Field("o_type", Field::Byte, 0, Field::Unsigned))
    .addField(new Field("o_name", Field::Text))
    .addField(new Field("o_caption", Field::Text))
    .addField(new Field("o_desc", Field::LongText));

    t_objects->debug();

    TableSchema *t_objectdata = newKexiDBSystemTableSchema("kexi__objectdata");
    t_objectdata->addField(new Field("o_id", Field::Integer, Field::NotNull, Field::Unsigned))
    .addField(new Field("o_data", Field::LongText))
    .addField(new Field("o_sub_id", Field::Text));

    TableSchema *t_fields = newKexiDBSystemTableSchema("kexi__fields");
    t_fields->addField(new Field("t_id", Field::Integer, 0, Field::Unsigned))
    .addField(new Field("f_type", Field::Byte, 0, Field::Unsigned))
    .addField(new Field("f_name", Field::Text))
    .addField(new Field("f_length", Field::Integer))
    .addField(new Field("f_precision", Field::Integer))
    .addField(new Field("f_constraints", Field::Integer))
    .addField(new Field("f_options", Field::Integer))
    .addField(new Field("f_default", Field::Text))
    //these are additional properties:
    .addField(new Field("f_order", Field::Integer))
    .addField(new Field("f_caption", Field::Text))
    .addField(new Field("f_help", Field::LongText));

    /* TableSchema *t_querydata = newKexiDBSystemTableSchema("kexi__querydata");
      t_querydata->addField( new Field("q_id", Field::Integer, 0, Field::Unsigned) )
      .addField( new Field("q_sql", Field::LongText ) )
      .addField( new Field("q_valid", Field::Boolean ) );

      TableSchema *t_queryfields = newKexiDBSystemTableSchema("kexi__queryfields");
      t_queryfields->addField( new Field("q_id", Field::Integer, 0, Field::Unsigned) )
      .addField( new Field("f_order", Field::Integer ) )
      .addField( new Field("f_id", Field::Integer ) )
      .addField( new Field("f_tab_asterisk", Field::Integer, 0, Field::Unsigned) )
      .addField( new Field("f_alltab_asterisk", Field::Boolean) );

      TableSchema *t_querytables = newKexiDBSystemTableSchema("kexi__querytables");
      t_querytables->addField( new Field("q_id", Field::Integer, 0, Field::Unsigned) )
      .addField( new Field("t_id", Field::Integer, 0, Field::Unsigned) )
      .addField( new Field("t_order", Field::Integer, 0, Field::Unsigned) );*/

    TableSchema *t_db = newKexiDBSystemTableSchema("kexi__db");
    t_db->addField(new Field("db_property", Field::Text, Field::NoConstraints, Field::NoOptions, 32))
    .addField(new Field("db_value", Field::LongText));

    /* moved to KexiProject...
      TableSchema *t_parts = newKexiDBSystemTableSchema("kexi__parts");
      t_parts->addField( new Field("p_id", Field::Integer, Field::PrimaryKey | Field::AutoInc, Field::Unsigned) )
      .addField( new Field("p_name", Field::Text) )
      .addField( new Field("p_mime", Field::Text ) )
      .addField( new Field("p_url", Field::Text ) );
    */

    /*UNUSED
      TableSchema *t_final = newKexiDBSystemTableSchema("kexi__final");
      t_final->addField( new Field("p_id", Field::Integer, 0, Field::Unsigned) )
      .addField( new Field("property", Field::LongText ) )
      .addField( new Field("value", Field::BLOB) );

      TableSchema *t_useractions = newKexiDBSystemTableSchema("kexi__useractions");
      t_useractions->addField( new Field("p_id", Field::Integer, 0, Field::Unsigned) )
      .addField( new Field("scope", Field::Integer ) )
      .addField( new Field("name", Field::LongText ) )
      .addField( new Field("text", Field::LongText ) )
      .addField( new Field("icon", Field::LongText ) )
      .addField( new Field("method", Field::Integer ) )
      .addField( new Field("arguments", Field::LongText) );
    */
    return true;
}

void Connection::removeMe(TableSchema *ts)
{
    if (ts && !m_destructor_started)
        d->takeTable(*ts);
}

QString Connection::anyAvailableDatabaseName()
{
    if (!d->availableDatabaseName.isEmpty()) {
        return d->availableDatabaseName;
    }
    return m_driver->beh->ALWAYS_AVAILABLE_DATABASE_NAME;
}

void Connection::setAvailableDatabaseName(const QString& dbName)
{
    d->availableDatabaseName = dbName;
}

//! @internal used in updateRow(), insertRow(),
inline void updateRowDataWithNewValues(QuerySchema &query, RecordData& data, Predicate::RowEditBuffer::DBMap& b,
                                       QHash<QueryColumnInfo*, int>& columnsOrderExpanded)
{
    columnsOrderExpanded = query.columnsOrder(QuerySchema::ExpandedList);
    QHash<QueryColumnInfo*, int>::ConstIterator columnsOrderExpandedIt;
    for (Predicate::RowEditBuffer::DBMap::ConstIterator it = b.constBegin();it != b.constEnd();++it) {
        columnsOrderExpandedIt = columnsOrderExpanded.find(it.key());
        if (columnsOrderExpandedIt == columnsOrderExpanded.constEnd()) {
            PreWarn << "(Connection) updateRowDataWithNewValues(): \"now also assign new value in memory\" step "
            "- could not find item '" << it.key()->aliasOrName() << "'";
            continue;
        }
        data[ columnsOrderExpandedIt.value()] = it.value();
    }
}

bool Connection::updateRow(QuerySchema &query, RecordData& data, RowEditBuffer& buf, bool useROWID)
{
// Each SQL identifier needs to be escaped in the generated query.
// query.debug();

    PreDbg << "Connection::updateRow..";
    clearError();
    //--get PKEY
    if (buf.dbBuffer().isEmpty()) {
        PreDbg << " -- NO CHANGES DATA!";
        return true;
    }
    TableSchema *mt = query.masterTable();
    if (!mt) {
        PreWarn << " -- NO MASTER TABLE!";
        setError(ERR_UPDATE_NO_MASTER_TABLE,
                 tr("Could not update row because there is no master table defined."));
        return false;
    }
    IndexSchema *pkey = (mt->primaryKey() && !mt->primaryKey()->fields()->isEmpty()) ? mt->primaryKey() : 0;
    if (!useROWID && !pkey) {
        PreWarn << " -- NO MASTER TABLE's PKEY!";
        setError(ERR_UPDATE_NO_MASTER_TABLES_PKEY,
                 tr("Could not update row because master table has no primary key defined."));
//! @todo perhaps we can try to update without using PKEY?
        return false;
    }
    //update the record:
    m_sql = "UPDATE " + escapeIdentifier(mt->name()) + " SET ";
    QString sqlset, sqlwhere;
    sqlset.reserve(1024);
    sqlwhere.reserve(1024);
    Predicate::RowEditBuffer::DBMap b = buf.dbBuffer();

    //gather the fields which are updated ( have values in RowEditBuffer)
    FieldList affectedFields;
    for (Predicate::RowEditBuffer::DBMap::ConstIterator it = b.constBegin();it != b.constEnd();++it) {
        if (it.key()->field->table() != mt)
            continue; // skip values for fields outside of the master table (e.g. a "visible value" of the lookup field)
        if (!sqlset.isEmpty())
            sqlset += ",";
        Field* currentField = it.key()->field;
        affectedFields.addField(currentField);
        sqlset += (escapeIdentifier(currentField->name()) + "=" +
                   m_driver->valueToSQL(currentField, it.value()));
    }
    if (pkey) {
        const QVector<int> pkeyFieldsOrder(query.pkeyFieldsOrder());
        PreDbg << pkey->fieldCount() << " ? " << query.pkeyFieldsCount();
        if (pkey->fieldCount() != query.pkeyFieldsCount()) { //sanity check
            PreWarn << " -- NO ENTIRE MASTER TABLE's PKEY SPECIFIED!";
            setError(ERR_UPDATE_NO_ENTIRE_MASTER_TABLES_PKEY,
                     tr("Could not update row because it does not contain entire master table's primary key."));
            return false;
        }
        if (!pkey->fields()->isEmpty()) {
            uint i = 0;
            foreach(Field *f, *pkey->fields()) {
                if (!sqlwhere.isEmpty())
                    sqlwhere += " AND ";
                QVariant val(data.at(pkeyFieldsOrder.at(i)));
                if (val.isNull() || !val.isValid()) {
                    setError(ERR_UPDATE_NULL_PKEY_FIELD,
                             tr("Primary key's field \"%1\" cannot be empty.").arg(f->name()));
                    //js todo: pass the field's name somewhere!
                    return false;
                }
                sqlwhere += (escapeIdentifier(f->name()) + "=" +
                             m_driver->valueToSQL(f, val));
                i++;
            }
        }
    } else {//use ROWID
        sqlwhere = (escapeIdentifier(m_driver->beh->ROW_ID_FIELD_NAME) + "="
                    + m_driver->valueToSQL(Field::BigInteger, data[data.size()-1]));
    }
    m_sql += (sqlset + " WHERE " + sqlwhere);
    PreDbg << " -- SQL == " << ((m_sql.length() > 400) ? (m_sql.left(400) + "[.....]") : m_sql);

    // preprocessing before update
    if (!drv_beforeUpdate(mt->name(), affectedFields))
        return false;

    bool res = executeSQL(m_sql);

    // postprocessing after update
    if (!drv_afterUpdate(mt->name(), affectedFields))
        return false;

    if (!res) {
        setError(ERR_UPDATE_SERVER_ERROR, tr("Row updating on the server failed."));
        return false;
    }
    //success: now also assign new values in memory:
    QHash<QueryColumnInfo*, int> columnsOrderExpanded;
    updateRowDataWithNewValues(query, data, b, columnsOrderExpanded);
    return true;
}

bool Connection::insertRow(QuerySchema &query, RecordData& data, RowEditBuffer& buf, bool getROWID)
{
// Each SQL identifier needs to be escaped in the generated query.
    PreDbg << "Connection::updateRow..";
    clearError();
    //--get PKEY
    /*disabled: there may be empty rows (with autoinc)
    if (buf.dbBuffer().isEmpty()) {
      PreDbg << " -- NO CHANGES DATA!";
      return true; }*/
    TableSchema *mt = query.masterTable();
    if (!mt) {
        PreWarn << " -- NO MASTER TABLE!";
        setError(ERR_INSERT_NO_MASTER_TABLE,
                 tr("Could not insert row because there is no master table defined."));
        return false;
    }
    IndexSchema *pkey = (mt->primaryKey() && !mt->primaryKey()->fields()->isEmpty()) ? mt->primaryKey() : 0;
    if (!getROWID && !pkey)
        PreWarn << " -- WARNING: NO MASTER TABLE's PKEY";

    QString sqlcols, sqlvals;
    sqlcols.reserve(1024);
    sqlvals.reserve(1024);

    //insert the record:
    m_sql = "INSERT INTO " + escapeIdentifier(mt->name()) + " (";
    Predicate::RowEditBuffer::DBMap b = buf.dbBuffer();

    // add default values, if available (for any column without value explicitly set)
    const QueryColumnInfo::Vector fieldsExpanded(query.fieldsExpanded(QuerySchema::Unique));
    uint fieldsExpandedCount = fieldsExpanded.count();
    for (uint i = 0; i < fieldsExpandedCount; i++) {
        QueryColumnInfo *ci = fieldsExpanded.at(i);
        if (ci->field && Predicate::isDefaultValueAllowed(ci->field)
                && !ci->field->defaultValue().isNull()
                && !b.contains(ci)) {
            PreDbg << "Connection::insertRow(): adding default value '" << ci->field->defaultValue().toString()
            << "' for column '" << ci->field->name() << "'";
            b.insert(ci, ci->field->defaultValue());
        }
    }

    //collect fields which have values in RowEditBuffer
    FieldList affectedFields;

    if (b.isEmpty()) {
        // empty row inserting requested:
        if (!getROWID && !pkey) {
            PreWarn << "MASTER TABLE's PKEY REQUIRED FOR INSERTING EMPTY ROWS: INSERT CANCELLED";
            setError(ERR_INSERT_NO_MASTER_TABLES_PKEY,
                     tr("Could not insert row because master table has no primary key defined."));
            return false;
        }
        if (pkey) {
            const QVector<int> pkeyFieldsOrder(query.pkeyFieldsOrder());
//   PreDbg << pkey->fieldCount() << " ? " << query.pkeyFieldsCount();
            if (pkey->fieldCount() != query.pkeyFieldsCount()) { //sanity check
                PreWarn << "NO ENTIRE MASTER TABLE's PKEY SPECIFIED!";
                setError(ERR_INSERT_NO_ENTIRE_MASTER_TABLES_PKEY,
                         tr("Could not insert row because it does not contain entire master table's primary key."));
                return false;
            }
        }
        //at least one value is needed for VALUES section: find it and set to NULL:
        Field *anyField = mt->anyNonPKField();
        if (!anyField) {
            if (!pkey) {
                PreWarn << "WARNING: NO FIELD AVAILABLE TO SET IT TO NULL";
                return false;
            } else {
                //try to set NULL in pkey field (could not work for every SQL engine!)
                anyField = pkey->fields()->first();
            }
        }
        sqlcols += escapeIdentifier(anyField->name());
        sqlvals += m_driver->valueToSQL(anyField, QVariant()/*NULL*/);
        affectedFields.addField(anyField);
    } else {
        // non-empty row inserting requested:
        for (Predicate::RowEditBuffer::DBMap::ConstIterator it = b.constBegin();it != b.constEnd();++it) {
            if (it.key()->field->table() != mt)
                continue; // skip values for fields outside of the master table (e.g. a "visible value" of the lookup field)
            if (!sqlcols.isEmpty()) {
                sqlcols += ",";
                sqlvals += ",";
            }
            Field* currentField = it.key()->field;
            affectedFields.addField(currentField);
            sqlcols += escapeIdentifier(currentField->name());
            sqlvals += m_driver->valueToSQL(currentField, it.value());
        }
    }
    m_sql += (sqlcols + ") VALUES (" + sqlvals + ")");
// PreDbg << " -- SQL == " << m_sql;

    // do driver specific pre-processing
    if (!drv_beforeInsert(mt->name(), affectedFields))
        return false;

    bool res = executeSQL(m_sql);

    // do driver specific post-processing
    if (!drv_afterInsert(mt->name(), affectedFields))
        return false;

    if (!res) {
        setError(ERR_INSERT_SERVER_ERROR, tr("Row inserting on the server failed."));
        return false;
    }
    //success: now also assign a new value in memory:
    QHash<QueryColumnInfo*, int> columnsOrderExpanded;
    updateRowDataWithNewValues(query, data, b, columnsOrderExpanded);

    //fetch autoincremented values
    QueryColumnInfo::List *aif_list = query.autoIncrementFields();
    quint64 ROWID = 0;
    if (pkey && !aif_list->isEmpty()) {
        //! @todo now only if PKEY is present, this should also work when there's no PKEY
        QueryColumnInfo *id_columnInfo = aif_list->first();
//! @todo safe to cast it?
        quint64 last_id = lastInsertedAutoIncValue(
                              id_columnInfo->field->name(), id_columnInfo->field->table()->name(), &ROWID);
        if (last_id == (quint64) - 1 || last_id <= 0) {
            //! @todo show error
//! @todo remove just inserted row. How? Using ROLLBACK?
            return false;
        }
        RecordData aif_data;
        QString getAutoIncForInsertedValue = QString::fromLatin1("SELECT ")
                                             + query.autoIncrementSQLFieldsList(m_driver)
                                             + QString::fromLatin1(" FROM ")
                                             + escapeIdentifier(id_columnInfo->field->table()->name())
                                             + QString::fromLatin1(" WHERE ")
                                             + escapeIdentifier(id_columnInfo->field->name()) + "="
                                             + QString::number(last_id);
        if (true != querySingleRecord(getAutoIncForInsertedValue, aif_data)) {
            //! @todo show error
            return false;
        }
        uint i = 0;
        foreach(QueryColumnInfo *ci, *aif_list) {
//   PreDbg << "Connection::insertRow(): AUTOINCREMENTED FIELD " << fi->field->name() << " == "
//    << aif_data[i].toInt();
            (data[ columnsOrderExpanded.value(ci)] = aif_data.value(i)).convert(ci->field->variantType());        //cast to get proper type
            i++;
        }
    } else {
        ROWID = drv_lastInsertRowID();
//  PreDbg << "Connection::insertRow(): new ROWID == " << (uint)ROWID;
        if (m_driver->beh->ROW_ID_FIELD_RETURNS_LAST_AUTOINCREMENTED_VALUE) {
            PreWarn << "Connection::insertRow(): m_driver->beh->ROW_ID_FIELD_RETURNS_LAST_AUTOINCREMENTED_VALUE";
            return false;
        }
    }
    if (getROWID && /*sanity check*/data.size() > (int)fieldsExpanded.size()) {
//  PreDbg << "Connection::insertRow(): new ROWID == " << (uint)ROWID;
        data[data.size()-1] = ROWID;
    }
    return true;
}

bool Connection::deleteRow(QuerySchema &query, RecordData& data, bool useROWID)
{
// Each SQL identifier needs to be escaped in the generated query.
    PreWarn << "Connection::deleteRow..";
    clearError();
    TableSchema *mt = query.masterTable();
    if (!mt) {
        PreWarn << " -- NO MASTER TABLE!";
        setError(ERR_DELETE_NO_MASTER_TABLE,
                 tr("Could not delete row because there is no master table defined."));
        return false;
    }
    IndexSchema *pkey = (mt->primaryKey() && !mt->primaryKey()->fields()->isEmpty()) ? mt->primaryKey() : 0;

//! @todo allow to delete from a table without pkey
    if (!useROWID && !pkey) {
        PreWarn << " -- WARNING: NO MASTER TABLE's PKEY";
        setError(ERR_DELETE_NO_MASTER_TABLES_PKEY,
                 tr("Could not delete row because there is no primary key for master table defined."));
        return false;
    }

    //update the record:
    m_sql = "DELETE FROM " + escapeIdentifier(mt->name()) + " WHERE ";
    QString sqlwhere;
    sqlwhere.reserve(1024);

    if (pkey) {
        const QVector<int> pkeyFieldsOrder(query.pkeyFieldsOrder());
        PreDbg << pkey->fieldCount() << " ? " << query.pkeyFieldsCount();
        if (pkey->fieldCount() != query.pkeyFieldsCount()) { //sanity check
            PreWarn << " -- NO ENTIRE MASTER TABLE's PKEY SPECIFIED!";
            setError(ERR_DELETE_NO_ENTIRE_MASTER_TABLES_PKEY,
                     tr("Could not delete row because it does not contain entire master table's primary key."));
            return false;
        }
        uint i = 0;
        foreach(Field *f, *pkey->fields()) {
            if (!sqlwhere.isEmpty())
                sqlwhere += " AND ";
            QVariant val(data.at(pkeyFieldsOrder.at(i)));
            if (val.isNull() || !val.isValid()) {
                setError(ERR_DELETE_NULL_PKEY_FIELD, tr("Primary key's field \"%1\" cannot be empty.")
                                                     .arg(f->name()));
//js todo: pass the field's name somewhere!
                return false;
            }
            sqlwhere += (escapeIdentifier(f->name()) + "=" +
                         m_driver->valueToSQL(f, val));
            i++;
        }
    } else {//use ROWID
        sqlwhere = (escapeIdentifier(m_driver->beh->ROW_ID_FIELD_NAME) + "="
                    + m_driver->valueToSQL(Field::BigInteger, data[data.size()-1]));
    }
    m_sql += sqlwhere;
    PreDbg << " -- SQL == " << m_sql;

    if (!executeSQL(m_sql)) {
        setError(ERR_DELETE_SERVER_ERROR, tr("Row deletion on the server failed."));
        return false;
    }
    return true;
}

bool Connection::deleteAllRows(QuerySchema &query)
{
    clearError();
    TableSchema *mt = query.masterTable();
    if (!mt) {
        PreWarn << " -- NO MASTER TABLE!";
        return false;
    }
    IndexSchema *pkey = mt->primaryKey();
    if (!pkey || pkey->fields()->isEmpty())
        PreWarn << "Connection::deleteAllRows -- WARNING: NO MASTER TABLE's PKEY";

    m_sql = "DELETE FROM " + escapeIdentifier(mt->name());

    PreDbg << " -- SQL == " << m_sql;

    if (!executeSQL(m_sql)) {
        setError(ERR_DELETE_SERVER_ERROR, tr("Row deletion on the server failed."));
        return false;
    }
    return true;
}

void Connection::registerForTableSchemaChanges(TableSchemaChangeListenerInterface& listener,
        TableSchema &schema)
{
    QSet<TableSchemaChangeListenerInterface*>* listeners = d->tableSchemaChangeListeners.value(&schema);
    if (!listeners) {
        listeners = new QSet<TableSchemaChangeListenerInterface*>();
        d->tableSchemaChangeListeners.insert(&schema, listeners);
    }
    listeners->insert(&listener);
}

void Connection::unregisterForTableSchemaChanges(TableSchemaChangeListenerInterface& listener,
        TableSchema &schema)
{
    QSet<TableSchemaChangeListenerInterface*>* listeners = d->tableSchemaChangeListeners.value(&schema);
    if (!listeners)
        return;
    listeners->remove(&listener);
}

void Connection::unregisterForTablesSchemaChanges(TableSchemaChangeListenerInterface& listener)
{
    foreach(QSet<TableSchemaChangeListenerInterface*> *listeners, d->tableSchemaChangeListeners)
    listeners->remove(&listener);
}

QSet<Connection::TableSchemaChangeListenerInterface*>*
Connection::tableSchemaChangeListeners(TableSchema& tableSchema) const
{
    PreDbg << d->tableSchemaChangeListeners.count();
    return d->tableSchemaChangeListeners.value(&tableSchema);
}

tristate Connection::closeAllTableSchemaChangeListeners(TableSchema& tableSchema)
{
    QSet<Connection::TableSchemaChangeListenerInterface*> *listeners
    = d->tableSchemaChangeListeners.value(&tableSchema);
    if (!listeners)
        return true;
//Qt4??? QSet<Connection::TableSchemaChangeListenerInterface*>::ConstIterator tmpListeners(*listeners); //safer copy
    tristate res = true;
    //try to close every window
    for (QSet<Connection::TableSchemaChangeListenerInterface*>::ConstIterator it(listeners->constBegin());
            it != listeners->constEnd() && res == true; ++it) {
        res = (*it)->closeListener();
    }
    return res;
}

void Connection::setReadOnly(bool set)
{
    if (d->isConnected)
        return; //sanity
    d->readOnly = set;
}

bool Connection::isReadOnly() const
{
    return d->readOnly;
}

void Connection::addCursor(Predicate::Cursor& cursor)
{
    d->cursors.insert(&cursor);
}

void Connection::takeCursor(Predicate::Cursor& cursor)
{
    d->cursors.remove(&cursor);
}

PreparedStatement Connection::prepareStatement(PreparedStatement::Type type,
    FieldList& fields, const QStringList& whereFieldNames)
{
//! @todo move to ConnectionInterface just like we moved execute() and prepare() to PreparedStatementInterface...
    PreparedStatementInterface *iface = prepareStatementInternal();
    if (!iface)
        return PreparedStatement();
    return PreparedStatement(*iface, type, fields, whereFieldNames);
}
