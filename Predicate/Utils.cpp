/* This file is part of the KDE project
   Copyright (C) 2004-2014 Jarosław Staniek <staniek@kde.org>
   Copyright (c) 2006, 2007 Thomas Braxton <kde.braxton@gmail.com>
   Copyright (c) 1999 Preston Brown <pbrown@kde.org>
   Copyright (c) 1997 Matthias Kalle Dalheimer <kalle@kde.org>

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

#include "Utils.h"
#include "Utils_p.h"

#include "Cursor.h"
#include "DriverManager.h"
#include "LookupFieldSchema.h"
#include "predicate_global.h"
#include "Tools/Static.h"

#include <QMap>
#include <QHash>
#include <QThread>
#include <QtXml>
#include <QBuffer>
#include <QPixmap>
#include <QMutex>
#include <QSet>
#include <QProgressBar>

#include <memory>


using namespace Predicate;

bool Predicate::deleteRecord(Connection* conn, TableSchema *table,
                             const QString &keyname, const QString &keyval)
{
    return table != 0 && conn->executeSQL(EscapedString("DELETE FROM ")
        + table->name() + " WHERE "
        + keyname + '=' + conn->driver()->valueToSQL(Field::Text, QVariant(keyval)));
}

bool Predicate::deleteRecord(Connection* conn, const QString &tableName,
                             const QString &keyname, const QString &keyval)
{
    return conn->executeSQL(EscapedString("DELETE FROM ") + tableName + " WHERE "
                           + keyname + '=' + conn->driver()->valueToSQL(Field::Text, QVariant(keyval)));
}

bool Predicate::deleteRecord(Connection* conn, TableSchema *table,
                             const QString& keyname, int keyval)
{
    return table != 0 && conn->executeSQL(EscapedString("DELETE FROM ")
        + table->name() + " WHERE "
        + keyname + '=' + conn->driver()->valueToSQL(Field::Integer, QVariant(keyval)));
}

bool Predicate::deleteRecord(Connection* conn, const QString &tableName,
                             const QString &keyname, int keyval)
{
    return conn->executeSQL(EscapedString("DELETE FROM ") + tableName + " WHERE "
                           + keyname + '=' + conn->driver()->valueToSQL(Field::Integer, QVariant(keyval)));
}

bool Predicate::deleteRecord(Connection* conn, const QString &tableName,
                             const QString &keyname1, Field::Type keytype1, const QVariant& keyval1,
                             const QString &keyname2, Field::Type keytype2, const QVariant& keyval2)
{
    return conn->executeSQL(EscapedString("DELETE FROM ") + tableName + " WHERE "
        + keyname1 + '=' + conn->driver()->valueToSQL(keytype1, keyval1)
        + " AND " + keyname2 + '=' + conn->driver()->valueToSQL(keytype2, keyval2));
}

bool Predicate::deleteRecord(Connection* conn, const QString &tableName,
                             const QString &keyname1, Field::Type keytype1, const QVariant& keyval1,
                             const QString &keyname2, Field::Type keytype2, const QVariant& keyval2,
                             const QString &keyname3, Field::Type keytype3, const QVariant& keyval3)
{
    return conn->executeSQL(EscapedString("DELETE FROM ") + tableName + " WHERE "
                           + keyname1 + "=" + conn->driver()->valueToSQL(keytype1, keyval1)
                           + " AND " + keyname2 + "=" + conn->driver()->valueToSQL(keytype2, keyval2)
                           + " AND " + keyname3 + "=" + conn->driver()->valueToSQL(keytype3, keyval3));
}

bool Predicate::isEmptyValue(Field *f, const QVariant &v)
{
    if (f->hasEmptyProperty() && v.toString().isEmpty() && !v.toString().isNull())
        return true;
    return v.isNull();
}

EscapedString Predicate::sqlWhere(Driver *drv, Field::Type t,
                            const QString& fieldName, const QVariant& value)
{
    if (value.isNull())
        return EscapedString(fieldName) + " is NULL";
    return EscapedString(fieldName) + '=' + drv->valueToSQL(t, value);
}

//! Cache
struct TypeCache {
    TypeCache() {
        for (Field::Type t = Field::FirstType; t <= Field::LastType; t = Field::Type(int(t) + 1)) {
            const Field::TypeGroup tg = Field::typeGroup(t);
            TypeGroupList list;
            QStringList name_list, str_list;
            if (tlist.contains(tg)) {
                list = tlist.value(tg);
                name_list = nlist.value(tg);
                str_list = slist.value(tg);
            }
            list += t;
            name_list += Field::typeName(t);
            str_list += Field::typeString(t);
            tlist[ tg ] = list;
            nlist[ tg ] = name_list;
            slist[ tg ] = str_list;
        }

        def_tlist[ Field::InvalidGroup ] = Field::InvalidType;
        def_tlist[ Field::TextGroup ] = Field::Text;
        def_tlist[ Field::IntegerGroup ] = Field::Integer;
        def_tlist[ Field::FloatGroup ] = Field::Double;
        def_tlist[ Field::BooleanGroup ] = Field::Boolean;
        def_tlist[ Field::DateTimeGroup ] = Field::Date;
        def_tlist[ Field::BLOBGroup ] = Field::BLOB;
    }

    QHash< Field::TypeGroup, TypeGroupList > tlist;
    QHash< Field::TypeGroup, QStringList > nlist;
    QHash< Field::TypeGroup, QStringList > slist;
    QHash< Field::TypeGroup, Field::Type > def_tlist;
};

PREDICATE_GLOBAL_STATIC(TypeCache, Predicate_typeCache)

const TypeGroupList Predicate::typesForGroup(Field::TypeGroup typeGroup)
{
    return Predicate_typeCache->tlist.value(typeGroup);
}

QStringList Predicate::typeNamesForGroup(Field::TypeGroup typeGroup)
{
    return Predicate_typeCache->nlist.value(typeGroup);
}

QStringList Predicate::typeStringsForGroup(Field::TypeGroup typeGroup)
{
    return Predicate_typeCache->slist.value(typeGroup);
}

Field::Type Predicate::defaultTypeForGroup(Field::TypeGroup typeGroup)
{
    return (typeGroup <= Field::LastTypeGroup) ? Predicate_typeCache->def_tlist.value(typeGroup) : Field::InvalidType;
}

void Predicate::getHTMLErrorMesage(const Resultable& resultable, QString& msg, QString &details)
{
    const Result result(resultable.result());
    if (!result.isError())
        return;
    //lower level message is added to the details, if there is alread message specified
    if (!result.messageTitle().isEmpty())
        msg += QLatin1String("<p>") + result.messageTitle();

    if (msg.isEmpty())
        msg = QLatin1String("<p>") + result.message();
    else
        details += QLatin1String("<p>") + result.message();

    if (!result.serverMessage().isEmpty())
        details += QLatin1String("<p><b>") + QObject::tr("Message from server:")
                   + QLatin1String("</b> ") + result.serverMessage();
    if (!result.recentSQLString().isEmpty())
        details += QLatin1String("<p><b>") + QObject::tr("SQL statement:")
                   + QString::fromLatin1("</b> <tt>%1</tt>").arg(result.recentSQLString().toString());
    int serverResultCode;
    QString serverResultName;
    if (result.serverResultCode() != 0) {
        serverResultCode = result.serverResultCode();
        serverResultName = resultable.serverResultName();
    } /*else {
        serverResultCode = result.previousServerResultCode();
        serverResultName = result.previousServerResultName();
    }*/
    if (   !details.isEmpty()
        && (   !result.serverMessage().isEmpty()
            || !result.recentSQLString().isEmpty()
            || !serverResultName.isEmpty()
            || serverResultCode != 0)
           )
    {
        details += (QLatin1String("<p><b>") + QObject::tr("Server result code:")
                    + QLatin1String("</b> ") + QString::number(serverResultCode));
        if (!serverResultName.isEmpty()) {
            details += QString::fromLatin1(" (%1)").arg(serverResultName);
        }
    }
    else {
        if (!serverResultName.isEmpty()) {
            details += (QLatin1String("<p><b>") + QObject::tr("Server result:")
                        + QLatin1String("</b> ") + serverResultName);
        }
    }

    if (!details.isEmpty() && !details.startsWith(QLatin1String("<qt>"))) {
        if (!details.startsWith(QLatin1String("<p>")))
            details.prepend(QLatin1String("<p>"));
    }
}

void Predicate::getHTMLErrorMesage(const Resultable& resultable, QString& msg)
{
    getHTMLErrorMesage(resultable, msg, msg);
}

void Predicate::getHTMLErrorMesage(const Resultable& resultable, ResultInfo *info)
{
    getHTMLErrorMesage(resultable, info->msg, info->desc);
}

int Predicate::idForObjectName(Connection* conn, const QString& objName, int objType)
{
    RecordData data;
    if (true != conn->querySingleRecord(
                EscapedString("SELECT o_id FROM kexi__objects WHERE o_name='%1' AND o_type=%2")
                .arg(EscapedString(objName), EscapedString::number(objType)), &data))
    {
        return 0;
    }
    bool ok;
    int id = data[0].toInt(&ok);
    return ok ? id : 0;
}

//-----------------------------------------

TableOrQuerySchema::TableOrQuerySchema(Connection *conn, const QByteArray& name)
        : m_name(name)
{
    m_table = conn->tableSchema(QLatin1String(name));
    m_query = m_table ? 0 : conn->querySchema(QLatin1String(name));
    if (!m_table && !m_query) {
        PreWarn << "tableOrQuery is neither table nor query!";
    }
}


TableOrQuerySchema::TableOrQuerySchema(Connection *conn, const QByteArray& name, bool table)
        : m_name(name)
        , m_table(table ? conn->tableSchema(QLatin1String(name)) : 0)
        , m_query(table ? 0 : conn->querySchema(QLatin1String(name)))
{
    if (table && !m_table) {
        PreWarn << "no table specified!";
    }
    if (!table && !m_query) {
        PreWarn << "no query specified!";
    }
}

TableOrQuerySchema::TableOrQuerySchema(FieldList *tableOrQuery)
        : m_table(dynamic_cast<TableSchema*>(tableOrQuery))
        , m_query(dynamic_cast<QuerySchema*>(tableOrQuery))
{
    Q_ASSERT(tableOrQuery);
    if (!m_table && !m_query) {
        PreWarn << "tableOrQuery is nether table nor query!";
    }
}

TableOrQuerySchema::TableOrQuerySchema(Connection *conn, int id)
{
    m_table = conn->tableSchema(id);
    m_query = m_table ? 0 : conn->querySchema(id);
    if (!m_table && !m_query) {
        PreWarn << "no table or query found for id==" << id;
    }
}

TableOrQuerySchema::TableOrQuerySchema(TableSchema* table)
        : m_table(table)
        , m_query(0)
{
    if (!m_table) {
        PreWarn << "no table specified!";
    }
}

TableOrQuerySchema::TableOrQuerySchema(QuerySchema* query)
        : m_table(0)
        , m_query(query)
{
    if (!m_query) {
        PreWarn << "no query specified!";
    }
}

uint TableOrQuerySchema::fieldCount() const
{
    if (m_table)
        return m_table->fieldCount();
    if (m_query)
        return m_query->fieldsExpanded().size();
    return 0;
}

const QueryColumnInfo::Vector TableOrQuerySchema::columns(bool unique)
{
    if (m_table)
        return m_table->query()->fieldsExpanded(unique ? QuerySchema::Unique : QuerySchema::Default);

    if (m_query)
        return m_query->fieldsExpanded(unique ? QuerySchema::Unique : QuerySchema::Default);

    PreWarn << "no query or table specified!";
    return QueryColumnInfo::Vector();
}

QByteArray TableOrQuerySchema::name() const
{
    if (m_table)
        return m_table->name().toLatin1();
    if (m_query)
        return m_query->name().toLatin1();
    return m_name;
}

QString TableOrQuerySchema::captionOrName() const
{
    Object *object = m_table ? static_cast<Object *>(m_table) : static_cast<Object *>(m_query);
    if (!object)
        return QLatin1String(m_name);
    return object->caption().isEmpty() ? object->name() : object->caption();
}

Field* TableOrQuerySchema::field(const QString& name)
{
    if (m_table)
        return m_table->field(name);
    if (m_query)
        return m_query->field(name);

    return 0;
}

QueryColumnInfo* TableOrQuerySchema::columnInfo(const QString& name)
{
    if (m_table)
        return m_table->query()->columnInfo(name);

    if (m_query)
        return m_query->columnInfo(name);

    return 0;
}

//! Sends information about table or query schema @a schema to debug output @a dbg.
QDebug operator<<(QDebug dbg, const TableOrQuerySchema& schema)
{
    if (schema.table())
        dbg.nospace() << *schema.table();
    else if (schema.query())
        dbg.nospace() << *schema.query();
    return dbg.space();
}

Connection* TableOrQuerySchema::connection() const
{
    if (m_table)
        return m_table->connection();
    else if (m_query)
        return m_query->connection();
    return 0;
}


//------------------------------------------

ConnectionTestThread::ConnectionTestThread(ConnectionTestDialog* dlg, const ConnectionData& connData)
        : m_dlg(dlg), m_connData(connData)
{
    connect(this, SIGNAL(error(QString,QString)),
            dlg, SLOT(error(QString,QString)), Qt::QueuedConnection);

    // try to load driver now because it's not supported in different thread
    DriverManager manager;
    m_driver = manager.driver(m_connData.driverName());
    if (manager.result().isError()) {
        emitError(manager.resultable());
        m_driver = 0;
    }
}

void ConnectionTestThread::emitError(const Resultable& resultable)
{
    QString msg;
    QString details;
    Predicate::getHTMLErrorMesage(resultable, msg, details);
    emit error(msg, details);
}

void ConnectionTestThread::run()
{
    if (!m_driver) {
        return;
    }
    std::auto_ptr<Connection> conn(m_driver->createConnection(m_connData));
    if (!conn.get() || m_driver->result().isError()) {
        emitError(*m_driver);
        return;
    }
    if (!conn.get()->connect() || conn.get()->result().isError()) {
        emitError(*conn.get());
        return;
    }
    // SQL database backends like PostgreSQL require executing "USE database"
    // if we really want to know connection to the server succeeded.
    QString tmpDbName;
    if (!conn->useTemporaryDatabaseIfNeeded(&tmpDbName)) {
        emitError(*conn.get());
        return;
    }
    emitError(Resultable());
}

ConnectionTestDialog::ConnectionTestDialog(QWidget* parent,
        const ConnectionData& data,
        MessageHandler& msgHandler)
        : QProgressDialog(parent)
        , m_thread(new ConnectionTestThread(this, data))
        , m_connData(data)
        , m_msgHandler(&msgHandler)
        , m_elapsedTime(0)
        , m_error(false)
        , m_stopWaiting(false)
{
    setWindowTitle(tr("Test Connection"));
    setLabelText(tr("Testing connection to \"%1\" database server...")
                 .arg(data.serverInfoString()));
    setModal(true);
    setRange(0, 0); //to show busy indicator
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(slotTimeout()));
    adjustSize();
    resize(250, height());
}

ConnectionTestDialog::~ConnectionTestDialog()
{
    m_thread->terminate();
    delete m_thread;
}

int ConnectionTestDialog::exec()
{
    //kDebug() << "tid:" << QThread::currentThread() << "this_thread:" << thread();
    m_timer.start(20);
    m_thread->start();
    const int res = QProgressDialog::exec(); // krazy:exclude=qclasses
    m_thread->wait();
    m_timer.stop();
    return res;
}

void ConnectionTestDialog::slotTimeout()
{
    //PreDbg << "tid:" << QThread::currentThread() << "this_thread:" << thread();
    PreDbg << m_error;
    bool notResponding = false;
    if (m_elapsedTime >= 1000*5) {//5 seconds
        m_stopWaiting = true;
        notResponding = true;
    }
    PreDbg << m_elapsedTime << m_stopWaiting << notResponding;
    if (m_stopWaiting) {
        m_timer.disconnect(this);
        m_timer.stop();
        reject();
        PreDbg << "after reject";
        if (m_error) {
            PreDbg << "show?";
            m_msgHandler->showErrorMessage(MessageHandler::Sorry, m_msg, m_details);
            PreDbg << "shown";
            m_error = false;
        } else if (notResponding) {
            m_msgHandler->showErrorMessage(
                MessageHandler::Sorry,
                QObject::tr("Test connection to \"%1\" database server failed. The server is not responding.")
                    .arg(m_connData.serverInfoString()),
                QString(),
                QObject::tr("Test Connection"));
        } else {
            m_msgHandler->showErrorMessage(
                MessageHandler::Information,
                QObject::tr("Test connection to \"%1\" database server established successfully.")
                    .arg(m_connData.serverInfoString()),
                QString(),
                tr("Test Connection"));
        }
        return;
    }
    m_elapsedTime += 20;
    setValue(m_elapsedTime);
}

void ConnectionTestDialog::error(const QString& msg, const QString& details)
{
    //kDebug() << "tid:" << QThread::currentThread() << "this_thread:" << thread();
    PreDbg << msg << details;
    m_stopWaiting = true;
    if (!msg.isEmpty() || !details.isEmpty()) {
        m_error = true;
        m_msg = msg;
        m_details = details;
        PreDbg << "ERR!";
    }
}

void ConnectionTestDialog::reject()
{
    m_thread->terminate();
    m_timer.disconnect(this);
    m_timer.stop();
    QProgressDialog::reject(); // krazy:exclude=qclasses
}

void Predicate::connectionTestDialog(QWidget* parent, const ConnectionData& data,
                                     MessageHandler& msgHandler)
{
    ConnectionTestDialog dlg(parent, data, msgHandler);
    dlg.exec();
}

int Predicate::recordCount(Connection* conn, const EscapedString& sql)
{
    int count = -1; //will be changed only on success of querySingleNumber()
    conn->querySingleNumber(EscapedString("SELECT COUNT() FROM (") + sql
        + ") AS predicate__subquery", &count);
    return count;
}

int Predicate::recordCount(const TableSchema& tableSchema)
{
//! @todo does not work with non-SQL data sources
    if (!tableSchema.connection()) {
        PreWarn << "no tableSchema.connection()";
        return -1;
    }
    int count = -1; //will be changed only on success of querySingleNumber()
    tableSchema.connection()->querySingleNumber(
        EscapedString("SELECT COUNT(*) FROM ")
        + tableSchema.connection()->escapeIdentifier(tableSchema.name()),
        &count
    );
    return count;
}

int Predicate::recordCount(QuerySchema* querySchema)
{
//! @todo does not work with non-SQL data sources
    if (!querySchema->connection()) {
        PreWarn << "no querySchema->connection()";
        return -1;
    }
    int count = -1; //will be changed only on success of querySingleNumber()
    querySchema->connection()->querySingleNumber(
        EscapedString("SELECT COUNT(*) FROM (")
            + querySchema->connection()->selectStatement(querySchema)
            + ") AS predicate__subquery", &count
    );
    return count;
}

int Predicate::recordCount(TableOrQuerySchema* tableOrQuery)
{
    if (tableOrQuery->table())
        return recordCount(*tableOrQuery->table());
    if (tableOrQuery->query())
        return recordCount(tableOrQuery->query());
    return -1;
}

int Predicate::fieldCount(TableOrQuerySchema* tableOrQuery)
{
    if (tableOrQuery->table())
        return tableOrQuery->table()->fieldCount();
    if (tableOrQuery->query())
        return tableOrQuery->query()->fieldsExpanded().count();
    return -1;
}

bool Predicate::splitToTableAndFieldParts(const QString& string,
                                          QString *tableName, QString *fieldName,
                                          SplitToTableAndFieldPartsOptions option)
{
    Q_ASSERT(tableName);
    Q_ASSERT(fieldName);
    const int id = string.indexOf(QLatin1Char('.'));
    if (option & SetFieldNameIfNoTableName && id == -1) {
        tableName->clear();
        *fieldName = string;
        return !fieldName->isEmpty();
    }
    if (id <= 0 || id == int(string.length() - 1))
        return false;
    *tableName = string.left(id);
    *fieldName = string.mid(id + 1);
    return !tableName->isEmpty() && !fieldName->isEmpty();
}

bool Predicate::supportsVisibleDecimalPlacesProperty(Field::Type type)
{
//! @todo add check for decimal type as well
    return Field::isFPNumericType(type);
}

QString Predicate::formatNumberForVisibleDecimalPlaces(double value, int decimalPlaces)
{
//! @todo round?
    if (decimalPlaces < 0) {
        QString s(QString::number(value, 'f', 10 /*reasonable precision*/));
        uint i = s.length() - 1;
        while (i > 0 && s[i] == QLatin1Char('0'))
            i--;
        if (s[i] == QLatin1Char('.')) //remove '.'
            i--;
        s = s.left(i + 1).replace(QLatin1Char('.'), QLocale().decimalPoint());
        return s;
    }
    if (decimalPlaces == 0)
        return QString::number((int)value);
    return QLocale().toString(value, 'g', decimalPlaces);
}

Field::Type Predicate::intToFieldType(int type)
{
    if (type < (int)Field::InvalidType || type > (int)Field::LastType) {
        PreWarn << "invalid type" << type;
        return Field::InvalidType;
    }
    return (Field::Type)type;
}

static bool setIntToFieldType(Field *field, const QVariant& value)
{
    Q_ASSERT(field);
    bool ok;
    const int intType = value.toInt(&ok);
    if (!ok || Field::InvalidType == intToFieldType(intType)) {//for sanity
        PreWarn << "invalid type";
        return false;
    }
    field->setType((Field::Type)intType);
    return true;
}

//! @internal for Predicate::isBuiltinTableFieldProperty()
struct Predicate_BuiltinFieldProperties {
    Predicate_BuiltinFieldProperties() {
#define ADD(name) set.insert(name)
        ADD("type");
        ADD("primaryKey");
        ADD("indexed");
        ADD("autoIncrement");
        ADD("unique");
        ADD("notNull");
        ADD("allowEmpty");
        ADD("unsigned");
        ADD("name");
        ADD("caption");
        ADD("description");
        ADD("maxLength");
        ADD("maxLengthIsDefault");
        ADD("precision");
        ADD("defaultValue");
        ADD("defaultWidth");
        ADD("visibleDecimalPlaces");
//! @todo always update this when new builtins appear!
#undef ADD
    }
    QSet<QByteArray> set;
};

//! for Predicate::isBuiltinTableFieldProperty()
PREDICATE_GLOBAL_STATIC(Predicate_BuiltinFieldProperties, Predicate_builtinFieldProperties)


bool Predicate::isBuiltinTableFieldProperty(const QByteArray& propertyName)
{
    return Predicate_builtinFieldProperties->set.contains(propertyName);
}

namespace Predicate {
void getProperties(const LookupFieldSchema *lookup, QMap<QByteArray, QVariant> *values)
{
    Q_ASSERT(values);
    LookupFieldSchema::RecordSource recordSource;
    if (lookup) {
        recordSource = lookup->recordSource();
    }
    values->insert("rowSource", lookup ? recordSource.name() : QVariant());
    values->insert("rowSourceType", lookup ? recordSource.typeName() : QVariant());
    values->insert("rowSourceValues",
        (lookup && !recordSource.values().isEmpty()) ? recordSource.values() : QVariant());
    values->insert("boundColumn", lookup ? lookup->boundColumn() : QVariant());
    QList<QVariant> variantList;
    if (!lookup || lookup->visibleColumns().count() == 1) {
        values->insert("visibleColumn", lookup ? lookup->visibleColumns().first() : QVariant());
    }
    else {
        QList<uint> visibleColumns = lookup->visibleColumns();
        foreach(const QVariant& variant, visibleColumns) {
            variantList.append(variant);
        }
        values->insert("visibleColumn", variantList);
    }
    QList<int> columnWidths;
    variantList.clear();
    if (lookup) {
        columnWidths = lookup->columnWidths();
        foreach(const QVariant& variant, columnWidths) {
            variantList.append(variant);
        }
    }
    values->insert("columnWidths", lookup ? variantList : QVariant());
    values->insert("showColumnHeaders", lookup ? lookup->columnHeadersVisible() : QVariant());
    values->insert("listRows", lookup ? lookup->maximumListRows() : QVariant());
    values->insert("limitToList", lookup ? lookup->limitToList() : QVariant());
    values->insert("displayWidget", lookup ? uint(lookup->displayWidget()) : QVariant());
}
} // namespace Predicate

void Predicate::getFieldProperties(const Field &field, QMap<QByteArray, QVariant> *values)
{
    Q_ASSERT(values);
    values->clear();
    // normal values
    values->insert("type", field.type());
    const uint constraints = field.constraints();
    values->insert("primaryKey", constraints & Predicate::Field::PrimaryKey);
    values->insert("indexed", constraints & Predicate::Field::Indexed);
    values->insert("autoIncrement", Predicate::Field::isAutoIncrementAllowed(field.type())
                                    && (constraints & Predicate::Field::AutoInc));
    values->insert("unique", constraints & Predicate::Field::Unique);
    values->insert("notNull", constraints & Predicate::Field::NotNull);
    values->insert("allowEmpty", !(constraints & Predicate::Field::NotEmpty));
    const uint options = field.options();
    values->insert("unsigned", options & Predicate::Field::Unsigned);
    values->insert("name", field.name());
    values->insert("caption", field.caption());
    values->insert("description", field.description());
    values->insert("maxLength", field.maxLength());
    values->insert("maxLengthIsDefault", field.maxLengthStrategy() & Field::DefaultMaxLength);
    values->insert("precision", field.precision());
    values->insert("defaultValue", field.defaultValue());
#ifdef __GNUC__
#warning TODO    values->insert("defaultWidth", field.defaultWidth());
#else
#pragma WARNING(TODO    values->insert("defaultWidth", field.defaultWidth());)
#endif

    if (Predicate::supportsVisibleDecimalPlacesProperty(field.type())) {
        values->insert("visibleDecimalPlaces", field.defaultValue());
    }
    // insert lookup-related values
    LookupFieldSchema *lookup = field.table()->lookupFieldSchema(field);
    Predicate::getProperties(lookup, values);
}

static bool containsLookupFieldSchemaProperties(const QMap<QByteArray, QVariant>& values)
{
    for (QMap<QByteArray, QVariant>::ConstIterator it(values.constBegin());
         it != values.constEnd(); ++it)
    {
        if (Predicate::isLookupFieldSchemaProperty(it.key())) {
            return true;
        }
    }
    return false;
}

bool Predicate::setFieldProperties(Field *field, const QMap<QByteArray, QVariant>& values)
{
    Q_ASSERT(field);
    QMap<QByteArray, QVariant>::ConstIterator it;
    if ((it = values.find("type")) != values.constEnd()) {
        if (!setIntToFieldType(field, *it))
            return false;
    }

#define SET_BOOLEAN_FLAG(flag, value) { \
        constraints |= Field::flag; \
        if (!value) \
            constraints ^= Predicate::Field::flag; \
    }

    Field::Constraints constraints = field->constraints();
    bool ok = true;
    if ((it = values.find("primaryKey")) != values.constEnd())
        SET_BOOLEAN_FLAG(PrimaryKey, (*it).toBool());
    if ((it = values.find("indexed")) != values.constEnd())
        SET_BOOLEAN_FLAG(Indexed, (*it).toBool());
    if ((it = values.find("autoIncrement")) != values.constEnd()
            && Field::isAutoIncrementAllowed(field->type()))
        SET_BOOLEAN_FLAG(AutoInc, (*it).toBool());
    if ((it = values.find("unique")) != values.constEnd())
        SET_BOOLEAN_FLAG(Unique, (*it).toBool());
    if ((it = values.find("notNull")) != values.constEnd())
        SET_BOOLEAN_FLAG(NotNull, (*it).toBool());
    if ((it = values.find("allowEmpty")) != values.constEnd())
        SET_BOOLEAN_FLAG(NotEmpty, !(*it).toBool());
    field->setConstraints(constraints);

    Field::Options options;
    if ((it = values.find("unsigned")) != values.constEnd()) {
        options |= Field::Unsigned;
        if (!(*it).toBool())
            options ^= Field::Unsigned;
    }
    field->setOptions(options);

    if ((it = values.find("name")) != values.constEnd())
        field->setName((*it).toString());
    if ((it = values.find("caption")) != values.constEnd())
        field->setCaption((*it).toString());
    if ((it = values.find("description")) != values.constEnd())
        field->setDescription((*it).toString());
    if ((it = values.find("maxLength")) != values.constEnd())
        field->setMaxLength((*it).isNull() ? 0/*default*/ : (*it).toUInt(&ok));
    if (!ok)
        return false;
    if ((it = values.find("maxLengthIsDefault")) != values.constEnd()
            && (*it).toBool())
    {
        field->setMaxLengthStrategy(Field::DefaultMaxLength);
    }
    if ((it = values.find("precision")) != values.constEnd())
        field->setPrecision((*it).isNull() ? 0/*default*/ : (*it).toUInt(&ok));
    if (!ok)
        return false;
    if ((it = values.find("defaultValue")) != values.constEnd())
        field->setDefaultValue(*it);
#ifdef __GNUC__
#warning TODO defaultWidth
#else
#pragma WARNING(TODO defaultWidth)
#endif
#if 0
    if ((it = values.find("defaultWidth")) != values.constEnd())
        field.setDefaultWidth((*it).isNull() ? 0/*default*/ : (*it).toUInt(&ok));
    if (!ok)
        return false;
#endif

    // -- extended properties
    if ((it = values.find("visibleDecimalPlaces")) != values.constEnd()
            && Predicate::supportsVisibleDecimalPlacesProperty(field->type()))
        field->setVisibleDecimalPlaces((*it).isNull() ? -1/*default*/ : (*it).toInt(&ok));
    if (!ok)
        return false;

    if (field->table() && containsLookupFieldSchemaProperties(values)) {
        LookupFieldSchema *lookup = field->table()->lookupFieldSchema(*field);
        QScopedPointer<LookupFieldSchema> createdLookup;
        if (!lookup) { // create lookup if needed
            createdLookup.reset(lookup = new LookupFieldSchema());
        }
        if (lookup->setProperties(values)) {
            if (createdLookup) {
                if (field->table()->setLookupFieldSchema(field->name(), lookup)) {
                    createdLookup.take(); // ownership passed
                    lookup = 0;
                }
            }
        }
    }

    return true;
#undef SET_BOOLEAN_FLAG
}

//! @internal for isExtendedTableProperty()
struct Predicate_ExtendedProperties {
    Predicate_ExtendedProperties() {
#define ADD(name) set.insert( name )
        ADD("visibledecimalplaces");
        ADD("rowsource");
        ADD("rowsourcetype");
        ADD("rowsourcevalues");
        ADD("boundcolumn");
        ADD("visiblecolumn");
        ADD("columnwidths");
        ADD("showcolumnheaders");
        ADD("listrows");
        ADD("limittolist");
        ADD("displaywidget");
#undef ADD
    }
    QSet<QByteArray> set;
};

//! for isExtendedTableProperty()
PREDICATE_GLOBAL_STATIC(Predicate_ExtendedProperties, Predicate_extendedProperties)

bool Predicate::isExtendedTableFieldProperty(const QByteArray& propertyName)
{
    return Predicate_extendedProperties->set.contains(QByteArray(propertyName).toLower());
}

//! @internal for isLookupFieldSchemaProperty()
struct Predicate_LookupFieldSchemaProperties {
    Predicate_LookupFieldSchemaProperties() {
        QMap<QByteArray, QVariant> tmp;
        Predicate::getProperties(0, &tmp);
        foreach (const QByteArray &p, tmp.keys()) {
            set.insert(p.toLower());
        }
    }
    QSet<QByteArray> set;
};

//! for isLookupFieldSchemaProperty()
PREDICATE_GLOBAL_STATIC(Predicate_LookupFieldSchemaProperties, Predicate_lookupFieldSchemaProperties)

bool Predicate::isLookupFieldSchemaProperty(const QByteArray& propertyName)
{
    return Predicate_lookupFieldSchemaProperties->set.contains(QByteArray(propertyName).toLower());
}

bool Predicate::setFieldProperty(Field *field, const QByteArray& propertyName, const QVariant& value)
{
    Q_ASSERT(field);
#define SET_BOOLEAN_FLAG(flag, value) { \
        constraints |= Field::flag; \
        if (!value) \
            constraints ^= Field::flag; \
        field->setConstraints( constraints ); \
        return true; \
    }
#define GET_INT(method) { \
        const uint ival = value.toUInt(&ok); \
        if (!ok) \
            return false; \
        field->method( ival ); \
        return true; \
    }

    if (propertyName.isEmpty())
        return false;

    bool ok;
    if (Predicate::isExtendedTableFieldProperty(propertyName)) {
        //a little speedup: identify extended property in O(1)
        if ("visibleDecimalPlaces" == propertyName
                && Predicate::supportsVisibleDecimalPlacesProperty(field->type())) {
            GET_INT(setVisibleDecimalPlaces);
        }
        else if (Predicate::isLookupFieldSchemaProperty(propertyName)) {
            if (!field->table()) {
                PreWarn << "Cannot set" << propertyName << "property - no table assigned for field";
            } else {
                LookupFieldSchema *lookup = field->table()->lookupFieldSchema(*field);
                const bool createLookup = !lookup;
                if (createLookup) // create lookup if needed
                    lookup = new LookupFieldSchema();
                if (lookup->setProperty(propertyName, value)) {
                    if (createLookup)
                        field->table()->setLookupFieldSchema(field->name(), lookup);
                    return true;
                }
                if (createLookup)
                    delete lookup; // not set, delete
            }
        }
    } else {//non-extended
        if ("type" == propertyName)
            return setIntToFieldType(field, value);

        Field::Constraints constraints = field->constraints();
        if ("primaryKey" == propertyName)
            SET_BOOLEAN_FLAG(PrimaryKey, value.toBool());
        if ("indexed" == propertyName)
            SET_BOOLEAN_FLAG(Indexed, value.toBool());
        if ("autoIncrement" == propertyName
                && Field::isAutoIncrementAllowed(field->type()))
            SET_BOOLEAN_FLAG(AutoInc, value.toBool());
        if ("unique" == propertyName)
            SET_BOOLEAN_FLAG(Unique, value.toBool());
        if ("notNull" == propertyName)
            SET_BOOLEAN_FLAG(NotNull, value.toBool());
        if ("allowEmpty" == propertyName)
            SET_BOOLEAN_FLAG(NotEmpty, !value.toBool());

        Field::Options options;
        if ("unsigned" == propertyName) {
            options |= Field::Unsigned;
            if (!value.toBool())
                options ^= Field::Unsigned;
            field->setOptions(options);
            return true;
        }

        if ("name" == propertyName) {
            if (value.toString().isEmpty())
                return false;
            field->setName(value.toString());
            return true;
        }
        if ("caption" == propertyName) {
            field->setCaption(value.toString());
            return true;
        }
        if ("description" == propertyName) {
            field->setDescription(value.toString());
            return true;
        }
        if ("maxLength" == propertyName)
            GET_INT(setMaxLength);
        if ("maxLengthIsDefault" == propertyName) {
            field->setMaxLengthStrategy(Field::DefaultMaxLength);
        }
        if ("precision" == propertyName)
            GET_INT(setPrecision);
        if ("defaultValue" == propertyName) {
            field->setDefaultValue(value);
            return true;
        }

#ifdef __GNUC__
#warning TODO defaultWidth
#else
#pragma WARNING(TODO defaultWidth)
#endif
#if 0
        if ("defaultWidth" == propertyName)
            GET_INT(setDefaultWidth);
#endif
        // last chance that never fails: custom field property
        field->setCustomProperty(propertyName, value);
    }

    PreWarn << "property" << propertyName << "not found!";
    return false;
#undef SET_BOOLEAN_FLAG
#undef GET_INT
}

int Predicate::loadIntPropertyValueFromDom(const QDomNode& node, bool* ok)
{
    QByteArray valueType = node.nodeName().toLatin1();
    if (valueType.isEmpty() || valueType != "number") {
        if (ok)
            *ok = false;
        return 0;
    }
    const QString text(QDomNode(node).toElement().text());
    int val = text.toInt(ok);
    return val;
}

QString Predicate::loadStringPropertyValueFromDom(const QDomNode& node, bool* ok)
{
    QByteArray valueType = node.nodeName().toLatin1();
    if (valueType != "string") {
        if (ok)
            *ok = false;
        return QString();
    }
    if (ok)
        *ok = true;
    return QDomNode(node).toElement().text();
}

QVariant Predicate::loadPropertyValueFromDom(const QDomNode& node, bool* ok)
{
    QByteArray valueType = node.nodeName().toLatin1();
    if (valueType.isEmpty()) {
        if (ok)
            *ok = false;
        return QVariant();
    }
    if (ok)
        *ok = true;
    const QString text(QDomNode(node).toElement().text());
    bool _ok;
    if (valueType == "string") {
        return text;
    }
    else if (valueType == "cstring") {
        return text.toLatin1();
    }
    else if (valueType == "number") { // integer or double
        if (text.indexOf(QLatin1Char('.')) != -1) {
            double val = text.toDouble(&_ok);
            if (_ok)
                return val;
        }
        else {
            const int val = text.toInt(&_ok);
            if (_ok)
                return val;
            const qint64 valLong = text.toLongLong(&_ok);
            if (_ok)
                return valLong;
        }
    }
    else if (valueType == "bool") {
        return text.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0
               || text == QLatin1String("1");
    }
    else {
//! @todo add more QVariant types
        PreWarn << "Predicate::loadPropertyValueFromDom(): unknown type '" << valueType << "'";
    }
    if (ok)
        *ok = false;
    return QVariant();
}

QDomElement Predicate::saveNumberElementToDom(QDomDocument *doc, QDomElement *parentEl,
        const QString& elementName, int value)
{
    Q_ASSERT(doc);
    Q_ASSERT(parentEl);
    QDomElement el(doc->createElement(elementName));
    parentEl->appendChild(el);
    QDomElement numberEl(doc->createElement(QLatin1String("number")));
    el.appendChild(numberEl);
    numberEl.appendChild(doc->createTextNode(QString::number(value)));
    return el;
}

QDomElement Predicate::saveBooleanElementToDom(QDomDocument *doc, QDomElement *parentEl,
        const QString& elementName, bool value)
{
    Q_ASSERT(doc);
    Q_ASSERT(parentEl);
    QDomElement el(doc->createElement(elementName));
    parentEl->appendChild(el);
    QDomElement numberEl(doc->createElement(QLatin1String("bool")));
    el.appendChild(numberEl);
    numberEl.appendChild(doc->createTextNode(
                             value ? QLatin1String("true") : QLatin1String("false")));
    return el;
}

//! @internal Used in Predicate::emptyValueForType()
struct Predicate_EmptyValueForTypeCache {
    Predicate_EmptyValueForTypeCache()
            : values(int(Field::LastType) + 1) {
#define ADD(t, value) values.insert(t, value);
        ADD(Field::Byte, 0);
        ADD(Field::ShortInteger, 0);
        ADD(Field::Integer, 0);
        ADD(Field::BigInteger, 0);
        ADD(Field::Boolean, false);
        ADD(Field::Float, 0.0);
        ADD(Field::Double, 0.0);
//! @todo ok? we have no better defaults
        ADD(Field::Text, QLatin1String(" "));
        ADD(Field::LongText, QLatin1String(" "));
        ADD(Field::BLOB, QByteArray());
#undef ADD
    }
    QVector<QVariant> values;
};

//! Used in Predicate::emptyValueForType()
PREDICATE_GLOBAL_STATIC(Predicate_EmptyValueForTypeCache, Predicate_emptyValueForTypeCache)

QVariant Predicate::emptyValueForType(Field::Type type)
{
    const QVariant val(Predicate_emptyValueForTypeCache->values.at(
                           (type <= Field::LastType) ? type : Field::InvalidType));
    if (!val.isNull())
        return val;
    else { //special cases
        if (type == Field::Date)
            return QDate::currentDate();
        if (type == Field::DateTime)
            return QDateTime::currentDateTime();
        if (type == Field::Time)
            return QTime::currentTime();
    }
    PreWarn << "no value for type" << Field::typeName(type);
    return QVariant();
}

//! @internal Used in Predicate::notEmptyValueForType()
struct Predicate_NotEmptyValueForTypeCache {
    Predicate_NotEmptyValueForTypeCache()
            : values(int(Field::LastType) + 1) {
#define ADD(t, value) values.insert(t, value);
        // copy most of the values
        for (int i = int(Field::InvalidType) + 1; i <= Field::LastType; i++) {
            if (i == Field::Date || i == Field::DateTime || i == Field::Time)
                continue; //'current' value will be returned
            if (i == Field::Text || i == Field::LongText) {
                ADD(i, QVariant(QLatin1String("")));
                continue;
            }
            if (i == Field::BLOB) {
//! @todo blobs will contain other mime types too
                QByteArray ba;
//! @todo port to Qt4
#if 0
                QBuffer buffer(&ba);
                buffer.open(QIODevice::WriteOnly);
                QPixmap pm(SmallIcon("document-new"));
                pm.save(&buffer, "PNG"/*! @todo default? */);
#endif
                ADD(i, ba);
                continue;
            }
            ADD(i, Predicate::emptyValueForType((Field::Type)i));
        }
#undef ADD
    }
    QVector<QVariant> values;
};
//! Used in Predicate::notEmptyValueForType()
PREDICATE_GLOBAL_STATIC(Predicate_NotEmptyValueForTypeCache, Predicate_notEmptyValueForTypeCache)

QVariant Predicate::notEmptyValueForType(Field::Type type)
{
    const QVariant val(Predicate_notEmptyValueForTypeCache->values.at(
                           (type <= Field::LastType) ? type : Field::InvalidType));
    if (!val.isNull())
        return val;
    else { //special cases
        if (type == Field::Date)
            return QDate::currentDate();
        if (type == Field::DateTime)
            return QDateTime::currentDateTime();
        if (type == Field::Time)
            return QTime::currentTime();
    }
    PreWarn << "no value for type" << Field::typeName(type);
    return QVariant();
}

QString Predicate::escapeIdentifier(const QString& string)
{
    const QLatin1Char quote('"');
    // find out the length ot the destination string
    const int origStringLength = string.length();
    int newStringLength = 1 + 1;
    for (int i = 0; i < origStringLength; i++) {
        if (string.at(i) == quote)
            newStringLength += 2;
        else
            newStringLength++;
    }
    if (newStringLength == origStringLength)
        return string;
    newStringLength += 2; // for quotes
    // create
    QString escapedQuote(quote);
    escapedQuote.append(QLatin1Char(quote));
    QString newString;
    newString.reserve(newStringLength);
    newString.append(quote);
    for (int i = 0; i < origStringLength; i++) {
        const QChar c = string.at(i);
        if (c == quote)
            newString.append(escapedQuote);
        else
            newString.append(c);
    }
    newString.append(quote);
    return newString;
}

QByteArray Predicate::escapeIdentifier(const QByteArray& string)
{
    const char quote = '"';
    // find out the length ot the destination string
    const int origStringLength = string.length();
    int newStringLength = 1 + 1;
    for (int i = 0; i < origStringLength; i++) {
        if (string.at(i) == quote)
            newStringLength += 2;
        else
            newStringLength++;
    }
    if (newStringLength == origStringLength)
        return string;
    newStringLength += 2; // for quotes
    // create
    QByteArray escapedQuote;
    escapedQuote.append(quote);
    escapedQuote.append(quote);
    QByteArray newString;
    newString.reserve(newStringLength);
    newString.append(quote);
    for (int i = 0; i < origStringLength; i++) {
        const char c = string.at(i);
        if (c == quote)
            newString.append(escapedQuote);
        else
            newString.append(c);
    }
    newString.append(quote);
    return newString;
}

QString Predicate::escapeString(const QString& string)
{
    const char quote = '\'';
    // find out the length ot the destination string
    const int origStringLength = string.length();
    int newStringLength = 1 + 1;
    for (int i = 0; i < origStringLength; i++) {
        const ushort unicode = string.at(i).unicode();
        if (   unicode == quote
            || unicode == '\t'
            || unicode == '\\'
            || unicode == '\n'
            || unicode == '\r'
            || unicode == '\0')
        {
            newStringLength += 2;
        }
        else
            newStringLength++;
    }
    newStringLength += 2; // for quotes
    // create
    QString newString;
    newString.reserve(newStringLength);
    newString.append(QLatin1Char(quote));
    for (int i = 0; i < origStringLength; i++) {
        const QChar c = string.at(i);
        const ushort unicode = c.unicode();
        if (unicode == quote)
            newString.append(QLatin1String("''"));
        else if (unicode == '\t')
            newString.append(QLatin1String("\\t"));
        else if (unicode == '\\')
            newString.append(QLatin1String("\\\\"));
        else if (unicode == '\n')
            newString.append(QLatin1String("\\n"));
        else if (unicode == '\r')
            newString.append(QLatin1String("\\r"));
        else if (unicode == '\0')
            newString.append(QLatin1String("\\0"));
        else
            newString.append(c);
    }
    newString.append(QLatin1Char(quote));
    return newString;
}

QString Predicate::escapeBLOB(const QByteArray& array, BLOBEscapingType type)
{
    const int size = array.size();
    if (size == 0)
        return QString();
    int escaped_length = size * 2;
    if (type == BLOBEscape0xHex || type == BLOBEscapeOctal)
        escaped_length += 2/*0x or X'*/;
    else if (type == BLOBEscapeXHex)
        escaped_length += 3; //X' + '
    QString str;
    str.reserve(escaped_length);
    if (str.capacity() < escaped_length) {
        PreWarn << "no enough memory (cannot allocate" << escaped_length << "chars)";
        return QString();
    }
    if (type == BLOBEscapeXHex)
        str = QString::fromLatin1("X'");
    else if (type == BLOBEscape0xHex)
        str = QString::fromLatin1("0x");
    else if (type == BLOBEscapeOctal)
        str = QString::fromLatin1("'");

    int new_length = str.length(); //after X' or 0x, etc.
    if (type == BLOBEscapeOctal) {
        // only escape nonprintable characters as in Table 8-7:
        // http://www.postgresql.org/docs/8.1/interactive/datatype-binary.html
        // i.e. escape for bytes: < 32, >= 127, 39 ('), 92(\).
        for (int i = 0; i < size; i++) {
            const unsigned char val = array[i];
            if (val < 32 || val >= 127 || val == 39 || val == 92) {
                str[new_length++] = '\\';
                str[new_length++] = '\\';
                str[new_length++] = '0' + val / 64;
                str[new_length++] = '0' + (val % 64) / 8;
                str[new_length++] = '0' + val % 8;
            } else {
                str[new_length++] = val;
            }
        }
    } else {
        for (int i = 0; i < size; i++) {
            const unsigned char val = array[i];
            str[new_length++] = (val / 16) < 10 ? ('0' + (val / 16)) : ('A' + (val / 16) - 10);
            str[new_length++] = (val % 16) < 10 ? ('0' + (val % 16)) : ('A' + (val % 16) - 10);
        }
    }
    if (type == BLOBEscapeXHex || type == BLOBEscapeOctal)
        str[new_length++] = '\'';
    return str;
}

QByteArray Predicate::pgsqlByteaToByteArray(const char* data, int length)
{
    QByteArray array;
    int output = 0;
    for (int pass = 0; pass < 2; pass++) {//2 passes to avoid allocating buffer twice:
        //  0: count #of chars; 1: copy data
        const char* s = data;
        const char* end = s + length;
        if (pass == 1) {
            PreDbg << "processBinaryData(): real size == " << output;
            array.resize(output);
            output = 0;
        }
        for (int input = 0; s < end; output++) {
            //  PreDbg<<(int)s[0]<<" "<<(int)s[1]<<" "<<(int)s[2]<<" "<<(int)s[3]<<" "<<(int)s[4];
            if (s[0] == '\\' && (s + 1) < end) {
                //special cases as in http://www.postgresql.org/docs/8.1/interactive/datatype-binary.html
                if (s[1] == '\'') {// \'
                    if (pass == 1)
                        array[output] = '\'';
                    s += 2;
                } else if (s[1] == '\\') { // 2 backslashes
                    if (pass == 1)
                        array[output] = '\\';
                    s += 2;
                } else if ((input + 3) < length) {// \\xyz where xyz are 3 octal digits
                    if (pass == 1)
                        array[output] = char((int(s[1] - '0') * 8 + int(s[2] - '0')) * 8 + int(s[3] - '0'));
                    s += 4;
                } else {
                    PreWarn << "no octal value after backslash";
                    s++;
                }
            } else {
                if (pass == 1)
                    array[output] = s[0];
                s++;
            }
            //  PreDbg<<output<<": "<<(int)array[output];
        }
    }
    return array;
}

QList<int> Predicate::stringListToIntList(const QStringList &list, bool *ok)
{
    QList<int> result;
    foreach (const QString &item, list) {
        int val = item.toInt(ok);
        if (ok && !*ok) {
            return QList<int>();
        }
        result.append(val);
    }
    if (ok) {
        *ok = true;
    }
    return result;
}

// Based on KConfigGroupPrivate::serializeList() from kconfiggroup.cpp (kdelibs 4)
QString Predicate::serializeList(const QStringList &list)
{
    QString value;

    if (!list.isEmpty()) {
        QStringList::ConstIterator it = list.constBegin();
        const QStringList::ConstIterator end = list.constEnd();

        value = QString(*it).replace(QLatin1Char('\\'), QLatin1String("\\\\"))
                            .replace(QLatin1Char(','), QLatin1String("\\,"));

        while (++it != end) {
            // In the loop, so it is not done when there is only one element.
            // Doing it repeatedly is a pretty cheap operation.
            value.reserve(4096);

            value += QLatin1Char(',')
                     + QString(*it).replace(QLatin1Char('\\'), QLatin1String("\\\\"))
                                   .replace(QLatin1Char(','), QLatin1String("\\,"));
        }

        // To be able to distinguish an empty list from a list with one empty element.
        if (value.isEmpty())
            value = QLatin1String("\\0");
    }

    return value;
}

// Based on KConfigGroupPrivate::deserializeList() from kconfiggroup.cpp (kdelibs 4)
QStringList Predicate::deserializeList(const QString &data)
{
    if (data.isEmpty())
        return QStringList();
    if (data == QLatin1String("\\0"))
        return QStringList(QString());
    QStringList value;
    QString val;
    val.reserve(data.size());
    bool quoted = false;
    for (int p = 0; p < data.length(); p++) {
        if (quoted) {
            val += data[p];
            quoted = false;
        } else if (data[p].unicode() == QLatin1Char('\\')) {
            quoted = true;
        } else if (data[p].unicode() == QLatin1Char(',')) {
            val.squeeze(); // release any unused memory
            value.append(val);
            val.clear();
            val.reserve(data.size() - p);
        } else {
            val += data[p];
        }
    }
    value.append(val);
    return value;
}

QList<int> Predicate::deserializeIntList(const QString &data, bool *ok)
{
    return Predicate::stringListToIntList(
        Predicate::deserializeList(data), ok);
}

QString Predicate::variantToString(const QVariant& v)
 {
    if (v.type() == QVariant::ByteArray) {
        return Predicate::escapeBLOB(v.toByteArray(), Predicate::BLOBEscapeHex);
    }
    else if (v.type() == QVariant::StringList) {
        return serializeList(v.toStringList());
    }
    return v.toString();
}

QVariant Predicate::stringToVariant(const QString& s, QVariant::Type type, bool* ok)
{
    if (s.isNull()) {
        if (ok)
            *ok = true;
        return QVariant();
    }
    switch (type) {
    case QVariant::Invalid:
        if (ok)
            *ok = false;
        return QVariant();
    case QVariant::ByteArray: {//special case: hex string
        const uint len = s.length();
        QByteArray ba;
        ba.resize(len / 2 + len % 2);
        for (uint i = 0; i < (len - 1); i += 2) {
            bool _ok;
            int c = s.mid(i, 2).toInt(&_ok, 16);
            if (!_ok) {
                if (ok)
                    *ok = _ok;
                PreWarn << "Error in digit" << i;
                return QVariant();
            }
            ba[i/2] = (char)c;
        }
        if (ok)
            *ok = true;
        return ba;
    }
    case QVariant::StringList:
        *ok = true;
        return Predicate::deserializeList(s);
    default:;
    }

    QVariant result(s);
    if (!result.convert(type)) {
        if (ok)
            *ok = false;
        return QVariant();
    }
    if (ok)
        *ok = true;
    return result;
}

bool Predicate::isDefaultValueAllowed(Field* field)
{
    return field && !field->isUniqueKey();
}

void Predicate::getLimitsForType(Field::Type type, int &minValue, int &maxValue)
{
    switch (type) {
    case Field::Byte:
//! @todo always ok?
        minValue = 0;
        maxValue = 255;
        break;
    case Field::ShortInteger:
        minValue = -32768;
        maxValue = 32767;
        break;
    case Field::Integer:
    case Field::BigInteger: //cannot return anything larger
    default:
        minValue = (int) - 0x07FFFFFFF;
        maxValue = (int)(0x080000000 - 1);
    }
}

Field::Type Predicate::maximumForIntegerTypes(Field::Type t1, Field::Type t2)
{
    if (!Field::isIntegerType(t1) || !Field::isIntegerType(t2))
        return Field::InvalidType;
    if (t1 == t2)
        return t2;
    if (t1 == Field::ShortInteger && t2 != Field::Integer && t2 != Field::BigInteger)
        return t1;
    if (t1 == Field::Integer && t2 != Field::BigInteger)
        return t1;
    if (t1 == Field::BigInteger)
        return t1;
    return Predicate::maximumForIntegerTypes(t2, t1); //swap
}

QString Predicate::simplifiedTypeName(const Field& field)
{
    if (field.isNumericType())
        return QObject::tr("Number"); //simplify
    else if (field.type() == Field::BLOB)
//! @todo support names of other BLOB subtypes
        return QObject::tr("Image"); //simplify

    return field.typeGroupName();
}

QString Predicate::defaultFileBasedDriverMimeType()
{
    return QLatin1String("application/x-kexiproject-sqlite3");
}

QString Predicate::defaultFileBasedDriverName()
{
    return QLatin1String("sqlite");
}

QString Predicate::defaultFileBasedDriverIcon()
{
//! @todo port to Qt4
#if 1
    return QString();
#else
    KMimeType::Ptr mimeType(KMimeType::mimeType(
                                Predicate::defaultFileBasedDriverMimeType()));
    if (mimeType.isNull()) {
        PreWarn << Predicate::defaultFileBasedDriverMimeType() << "mimetype not installed!";
        return QString();
    }
    return mimeType->iconName();
#endif
}

QStringList Predicate::libraryPaths()
{
    QStringList result;
    foreach (const QString& path, qApp->libraryPaths()) {
        const QString dir(path + QLatin1String("/predicate"));
        if (QDir(dir).exists() && QDir(dir).isReadable()) {
            result += dir;
        }
    }
    return result;
}

QString Predicate::temporaryTableName(Connection *conn, const QString &baseName)
{
    while (true) {
        QString name = QLatin1String("tmp__") + baseName;
        for (int i = 0; i < 10; ++i) {
            name += QString::number(qrand() % 0x10, 16);
        }
        if (!conn->drv_containsTable(name)) {
            return name;
        }
    }
}

QString Predicate::sqlite3ProgramPath()
{
    QString path = Predicate::Utils::findExe(QLatin1String("sqlite3"));
    if (path.isEmpty()) {
        PreWarn << "Could not find program \"sqlite3\"";
    }
    return path;
}

bool Predicate::importSqliteFile(const QString &inputFileName, const QString &outputFileName)
{
    const QString sqlite_app = Predicate::sqlite3ProgramPath();
    if (sqlite_app.isEmpty()) {
        return false;
    }

    QFileInfo fi(inputFileName);
    if (!fi.isReadable()) {
        PreWarn << "No readable input file" << fi.absoluteFilePath();
        return false;
    }
    QFileInfo fo(outputFileName);
    if (QFile(fo.absoluteFilePath()).exists()) {
        if (!QFile::remove(fo.absoluteFilePath())) {
            PreWarn << "Cannot remove output file" << fo.absoluteFilePath();
            return false;
        }
    }
    PreDbg << inputFileName << fi.absoluteDir().path() << fo.absoluteFilePath();

    QProcess p;
    p.start(sqlite_app, QStringList() << fo.absoluteFilePath());
    if (!p.waitForStarted()) {
        PreWarn << "Failed to start program" << sqlite_app;
        return false;
    }
    QByteArray line(".read " + QFile::encodeName(fi.absoluteFilePath()));
    if (p.write(line) != line.length() || !p.waitForBytesWritten()) {
        PreWarn << "Failed to send \".read\" command to program" << sqlite_app;
        return false;
    }
    p.closeWriteChannel();
    if (!p.waitForFinished()) {
        PreWarn << "Failed to finish program" << sqlite_app;
        return false;
    }
    return true;
}

//---------

#ifdef PREDICATE_DEBUG_GUI

static DebugGUIHandler s_debugGUIHandler = 0;

void Predicate::setDebugGUIHandler(DebugGUIHandler handler)
{
    s_debugGUIHandler = handler;
}

void Predicate::debugGUI(const QString& text)
{
    if (s_debugGUIHandler)
        s_debugGUIHandler(text);
}

static AlterTableActionDebugGUIHandler s_alterTableActionDebugHandler = 0;

void Predicate::setAlterTableActionDebugHandler(AlterTableActionDebugGUIHandler handler)
{
    s_alterTableActionDebugHandler = handler;
}

void Predicate::alterTableActionDebugGUI(const QString& text, int nestingLevel)
{
    if (s_alterTableActionDebugHandler)
        s_alterTableActionDebugHandler(text, nestingLevel);
}

#endif // PREDICATE_DEBUG_GUI
