/* This file is part of the KDE project
   Copyright (C) 2004-2014 Jarosław Staniek <staniek@kde.org>

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

#ifndef PREDICATE_UTILS_H
#define PREDICATE_UTILS_H

#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QtDebug>

#include <Predicate/Global>
#include <Predicate/QuerySchema>

class QDomNode;
class QDomElement;
class QDomDocument;

namespace Predicate
{

class Connection;
class Driver;

//! for convenience
PREDICATE_EXPORT bool deleteRecord(Connection* conn, TableSchema *table,
                                          const QString &keyname, const QString& keyval);

PREDICATE_EXPORT bool deleteRecord(Connection* conn, const QString &tableName,
                                          const QString &keyname, const QString &keyval);

PREDICATE_EXPORT bool deleteRecord(Connection* conn, TableSchema *table,
                                          const QString& keyname, int keyval);

PREDICATE_EXPORT bool deleteRecord(Connection* conn, const QString &tableName,
                                          const QString &keyname, int keyval);

/*! Deletes record with two generic criterias. */
PREDICATE_EXPORT bool deleteRecord(Connection* conn, const QString &tableName,
                                   const QString &keyname1, Field::Type keytype1, const QVariant& keyval1,
                                   const QString &keyname2, Field::Type keytype2, const QVariant& keyval2);

/*! Deletes record with three generic criterias. */
PREDICATE_EXPORT bool deleteRecord(Connection* conn, const QString &tableName,
                                    const QString &keyname1, Field::Type keytype1, const QVariant& keyval1,
                                    const QString &keyname2, Field::Type keytype2, const QVariant& keyval2,
                                    const QString &keyname3, Field::Type keytype3, const QVariant& keyval3);

typedef QList<uint> TypeGroupList;

/*! @return list of types for type group @a typeGroup. */
PREDICATE_EXPORT const TypeGroupList typesForGroup(Field::TypeGroup typeGroup);

/*! @return list of i18n-ed type names for type group @a typeGroup. */
PREDICATE_EXPORT QStringList typeNamesForGroup(Field::TypeGroup typeGroup);

/*! @return list of (not-i18n-ed) type names for type group @a typeGroup. */
PREDICATE_EXPORT QStringList typeStringsForGroup(Field::TypeGroup typeGroup);

/*! @return default field type for type group @a typeGroup,
 for example, Field::Integer for Field::IntegerGroup.
 It is used e.g. in KexiAlterTableDialog, to properly fill
 'type' property when user selects type group for a field. */
PREDICATE_EXPORT Field::Type defaultTypeForGroup(Field::TypeGroup typeGroup);

/*! @return a slightly simplified type name for @a field.
 For BLOB type it returns i18n-ed "Image" string or other, depending on the mime type.
 For numbers (either floating-point or integer) it returns i18n-ed "Number: string.
 For other types it the same string as Field::typeGroupName() is returned. */
//! @todo support names of other BLOB subtypes
PREDICATE_EXPORT QString simplifiedTypeName(const Field& field);

/*! @return true if @a v represents an empty (but not null) value.
 Values of some types (as for strings) can be both empty and not null. */
PREDICATE_EXPORT bool isEmptyValue(Field *f, const QVariant &v);

/*! Sets @a msg to an error message retrieved from resultable @a resultable, and @a details
 to details of this error (server message and result number).
 Does nothing if @a result is empty. In this case @a msg and @a details strings are not overwritten.
 If @a msg is not empty, @a result message is appended to @a details.
 */
PREDICATE_EXPORT void getHTMLErrorMesage(const Resultable& resultable, QString& msg, QString &details);

/*! This methods works like above, but appends both a message and a description
 to @a msg. */
PREDICATE_EXPORT void getHTMLErrorMesage(const Resultable& resultable, QString& msg);

/*! This methods works like above, but works on @a result's  members instead. */
PREDICATE_EXPORT void getHTMLErrorMesage(const Resultable& resultable, ResultInfo *info);

/*! Function useful for building WHERE parts of sql statements.
Constructs an sql string like "fielname = value" for specific @a drv driver,
 field type @a t, @a fieldName and @a value. If @a value is null, "fieldname is NULL"
 string is returned. */
PREDICATE_EXPORT EscapedString sqlWhere(Driver *drv, Field::Type t,
                                        const QString& fieldName, const QVariant& value);

/*! @return identifier for object @a objName of type @a objType
 or 0 if such object does not exist. */
PREDICATE_EXPORT int idForObjectName(Connection* conn, const QString& objName, int objType);

/*! Variant class providing a pointer to table or query. */
class PREDICATE_EXPORT TableOrQuerySchema
{
public:
    /*! Creates a new TableOrQuerySchema variant object, retrieving table or query schema
     using @a conn connection and @a name. If both table and query exists for @a name,
     table has priority over query.
     You should check whether a query or table has been found by testing
     (query() || table()) expression. */
    TableOrQuerySchema(Connection *conn, const QByteArray& name);

    /*! Creates a new TableOrQuerySchema variant object, retrieving table or query schema
     using @a conn connection and @a name. If @a table is true, @a name is assumed
     to be a table name, otherwise @a name is assumed to be a query name.
     You should check whether a query or table has been found by testing
     (query() || table()) expression. */
    TableOrQuerySchema(Connection *conn, const QByteArray& name, bool table);

    /*! Creates a new TableOrQuerySchema variant object. @a tableOrQuery must be of
     class TableSchema or QuerySchema.
     You should check whether a query or table has been found by testing
     (query() || table()) expression. */
    explicit TableOrQuerySchema(FieldList *tableOrQuery);

    /*! Creates a new TableOrQuerySchema variant object, retrieving table or query schema
     using @a conn connection and @a id.
     You should check whether a query or table has been found by testing
     (query() || table()) expression. */
    TableOrQuerySchema(Connection *conn, int id);

    /*! Creates a new TableOrQuerySchema variant object, keeping a pointer so @a table
     object. */
    explicit TableOrQuerySchema(TableSchema* table);

    /*! Creates a new TableOrQuerySchema variant object, keeping a pointer so @a query
     object. */
    explicit TableOrQuerySchema(QuerySchema* query);

    //! @return a pointer to the query if it's provided
    QuerySchema* query() const {
        return m_query;
    }

    //! @return a pointer to the table if it's provided
    TableSchema* table() const {
        return m_table;
    }

    //! @return name of a query or table
    QByteArray name() const;

    //! @return caption (if present) or name of the table/query
    QString captionOrName() const;

    //! @return number of fields
    uint fieldCount() const;

    //! @return all columns for the table or the query
    const QueryColumnInfo::Vector columns(bool unique = false);

    /*! @return a field of the table or the query schema for name @a name
     or 0 if there is no such field. */
    Field* field(const QString& name);

    /*! Like Field* field(const QString& name);
     but returns all information associated with field/column @a name. */
    QueryColumnInfo* columnInfo(const QString& name);

    /*! @return connection object, for table or query or 0 if there's no table or query defined. */
    Connection* connection() const;

protected:
    QByteArray m_name; //!< the name is kept here because m_table and m_table can be 0
    //! and we still want name() and acptionOrName() work.
    TableSchema* m_table;
    QuerySchema* m_query;
};

//! @todo perhaps use quint64 here?
/*! @return number of records that can be retrieved after executing @a sql statement
 within a connection @a conn. The statement should be of type SELECT.
 For SQL data sources it does not fetch any records, only "COUNT(*)"
 SQL aggregation is used at the backed.
 -1 is returned if error occurred. */
int recordCount(Connection* conn, const EscapedString& sql);

//! @todo perhaps use quint64 here?
/*! @return number of records that can be retrieved from @a tableSchema.
 The table must be created or retrieved using a Connection object,
 i.e. tableSchema.connection() must not return 0.
 For SQL data sources it does not fetch any records, only "COUNT(*)"
 SQL aggregation is used at the backed.
 -1 is returned if error occurred. */
PREDICATE_EXPORT int recordCount(const TableSchema& tableSchema);

//! @todo perhaps use quint64 here?
/*! Like above but operates on a query schema. */
PREDICATE_EXPORT int recordCount(QuerySchema* querySchema);

//! @todo perhaps use quint64 here?
/*! Like above but operates on a table or query schema variant. */
PREDICATE_EXPORT int recordCount(TableOrQuerySchema* tableOrQuery);

/*! @return a number of columns that can be retrieved from table or query schema.
 In case of query, expanded fields are counted. Can return -1 if @a tableOrQuery
 has neither table or query assigned. */
PREDICATE_EXPORT int fieldCount(TableOrQuerySchema* tableOrQuery);

/*! shows connection test dialog with a progress bar indicating connection testing
 (within a second thread).
 @a data is used to perform a (temporary) test connection. @a msgHandler is used to display errors.
 On successful connecting, a message is displayed. After testing, temporary connection is closed. */
PREDICATE_EXPORT void connectionTestDialog(QWidget* parent, const ConnectionData& data,
        MessageHandler& msgHandler);

//! Used in splitToTableAndFieldParts().
enum SplitToTableAndFieldPartsOptions {
    FailIfNoTableOrFieldName = 0, //!< default value for splitToTableAndFieldParts()
    SetFieldNameIfNoTableName = 1 //!< see splitToTableAndFieldParts()
};

/*! Splits @a string like "table.field" into "table" and "field" parts.
 On success, a table name is passed to @a tableName and a field name is passed to @a fieldName.
 The function fails if either:
 - @a string is empty, or
 - @a string does not contain '.' character and @a option is FailIfNoTableOrFieldName
    (the default), or
 - '.' character is the first of last character of @a string (in this case table name
   or field name could become empty what is not allowed).

 If @a option is SetFieldNameIfNoTableName and @a string does not contain '.',
 @a string is passed to @a fieldName and @a tableName is set to QString()
 without failure.

 If function fails, @a tableName and @a fieldName remain unchanged.
 @return true on success. */
PREDICATE_EXPORT bool splitToTableAndFieldParts(const QString& string,
        QString *tableName, QString *fieldName,
        SplitToTableAndFieldPartsOptions option = FailIfNoTableOrFieldName);

/*! @return true if @a type supports "visibleDecimalPlaces" property. */
PREDICATE_EXPORT bool supportsVisibleDecimalPlacesProperty(Field::Type type);

/*! @return string constructed by converting @a value.
 * If @a decimalPlaces is < 0, all meaningful fractional digits are returned.
 * If @a automatically is 0, just integer part is returned.
 * If @a automatically is > 0, fractional part should take exactly
   N digits: if the fractional part is shorter than N, additional zeros are appended.
   For example, "12.345" becomes "12.345000" if N=6.

 No rounding is actually performed.
 KLocale::formatNumber() and KLocale::decimalSymbol() are used to get locale settings.

 @see Predicate::Field::visibleDecimalPlaces() */
PREDICATE_EXPORT QString formatNumberForVisibleDecimalPlaces(double value, int decimalPlaces);

//! @return true if @a propertyName is a builtin field property.
PREDICATE_EXPORT bool isBuiltinTableFieldProperty(const QByteArray& propertyName);

//! @return true if @a propertyName is an extended field property.
PREDICATE_EXPORT bool isExtendedTableFieldProperty(const QByteArray& propertyName);

//! @return true if @a propertyName is belongs to lookup field's schema.
PREDICATE_EXPORT bool isLookupFieldSchemaProperty(const QByteArray& propertyName);

/*! @return type of field for integer value @a type.
 If @a type cannot be casted to Predicate::Field::Type, Predicate::Field::InvalidType is returned.
 This can be used when type information is deserialized from a string or QVariant. */
PREDICATE_EXPORT Field::Type intToFieldType(int type);

/*! Gets property values for @a field.
 Properties from extended schema are included. @a values is cleared before filling.
 The same number of properties in the same order is returned.
 This function is used e.g. for altering table design.
 */
PREDICATE_EXPORT void getFieldProperties(const Field &field, QMap<QByteArray, QVariant> *values);

/*! Sets property values for @a field. @return true if all the values are valid and allowed.
 On failure contents of @a field is undefined.
 Properties from extended schema are also supported.
 This function is used e.g. by AlterTableHandler when property information comes in form of text.
 */
PREDICATE_EXPORT bool setFieldProperties(Field *field, const QMap<QByteArray, QVariant>& values);

/*! Sets property value for @a field. @return true if the property has been found and
 the value is valid for this property. On failure contents of @a field is undefined.
 Properties from extended schema are also supported as well as
   QVariant customProperty(const QString& propertyName) const;

 This function is used e.g. by AlterTableHandler when property information comes in form of text.
 */
PREDICATE_EXPORT bool setFieldProperty(Field *field, const QByteArray& propertyName,
                                       const QVariant& value);

/*! @return property value loaded from a DOM @a node, written in a QtDesigner-like
 notation: &lt;number&gt;int&lt;/number&gt; or &lt;bool&gt;bool&lt;/bool&gt;, etc. Supported types are
 "string", "cstring", "bool", "number". For invalid values null QVariant is returned.
 You can check the validity of the returned value using QVariant::type(). */
PREDICATE_EXPORT QVariant loadPropertyValueFromDom(const QDomNode& node, bool *ok);

/*! Convenience version of loadPropertyValueFromDom(). @return int value. */
PREDICATE_EXPORT int loadIntPropertyValueFromDom(const QDomNode& node, bool* ok);

/*! Convenience version of loadPropertyValueFromDom(). @return QString value. */
PREDICATE_EXPORT QString loadStringPropertyValueFromDom(const QDomNode& node, bool* ok);

/*! Saves integer element for value @a value to @a doc document within parent element
 @a parentEl. The value will be enclosed in "number" element and "elementName" element.
 Example: saveNumberElementToDom(doc, parentEl, "height", 15) will create
 @code
  <height><number>15</number></height>
 @endcode
 @return the reference to element created with tag elementName. */
PREDICATE_EXPORT QDomElement saveNumberElementToDom(QDomDocument *doc, QDomElement *parentEl,
        const QString& elementName, int value);

/*! Saves boolean element for value @a value to @a doc document within parent element
 @a parentEl. Like saveNumberElementToDom() but creates "bool" tags. True/false values will be
 saved as "true"/"false" strings.
 @return the reference to element created with tag elementName. */
PREDICATE_EXPORT QDomElement saveBooleanElementToDom(QDomDocument *doc, QDomElement *parentEl,
        const QString& elementName, bool value);

/*! @return an empty value that can be set for a database field of type @a type having
 "null" property set. Empty string is returned for text type, 0 for integer
 or floating-point types, false for boolean type, empty null byte array for BLOB type.
 For date, time and date/time types current date, time, date+time is returned, respectively.
 Returns null QVariant for unsupported values like Predicate::Field::InvalidType.
 This function is efficient (uses a cache) and is heavily used by the AlterTableHandler
 for filling new columns. */
PREDICATE_EXPORT QVariant emptyValueForType(Field::Type type);

/*! @return a value that can be set for a database field of type @a type having
 "notEmpty" property set. It works in a similar way as
 @ref QVariant emptyValueForType( Predicate::Field::Type type ) with the following differences:
 - " " string (a single space) is returned for Text and LongText types
 - a byte array with saved "filenew" PNG image (icon) for BLOB type
 Returns null QVariant for unsupported values like Predicate::Field::InvalidType.
 This function is efficient (uses a cache) and is heavily used by the AlterTableHandler
 for filling new columns. */
PREDICATE_EXPORT QVariant notEmptyValueForType(Field::Type type);

/*! @return escaped identifier string @a string using PredicateSQL dialect,
            i.e. doubles double quotes and inserts the string into double quotes.
    If the identifier does not contain double quote, @a string is returned.
    Use it for user-visible backend-independent statements. */
PREDICATE_EXPORT QString escapeIdentifier(const QString& string);

PREDICATE_EXPORT QByteArray escapeIdentifier(const QByteArray& string);

/*! @return escaped string @a string using PredicateSQL dialect,
            i.e. doubles single quotes and inserts the string into single quotes.
    Quotes are always added.
    Also escapes \\n, \\r, \\t, \\\\, \\0.
    Use it for user-visible backend-independent statements. */
PREDICATE_EXPORT QString escapeString(const QString& string);

//! Escaping types used in escapeBLOB().
enum BLOBEscapingType {
    BLOBEscapeXHex = 1,        //!< escaping like X'1FAD', used by sqlite (hex numbers)
    BLOBEscape0xHex,           //!< escaping like 0x1FAD, used by mysql (hex numbers)
    BLOBEscapeHex,              //!< escaping like 1FAD without quotes or prefixes
    BLOBEscapeOctal           //!< escaping like 'zk\\000$x', used by pgsql
    //!< (only non-printable characters are escaped using octal numbers)
    //!< See http://www.postgresql.org/docs/8.1/interactive/datatype-binary.html
};

//! @todo reverse function for BLOBEscapeOctal is available: processBinaryData() in PostgresqlCursor.cpp - move it here
/*! @return a string containing escaped, printable representation of @a array.
 Escaping is controlled by @a type. For empty array, QString() is returned,
 so if you want to use this function in an SQL statement, empty arrays should be
 detected and "NULL" string should be put instead.
 This is helper, used in Driver::escapeBLOB() and Predicate::variantToString(). */
PREDICATE_EXPORT QString escapeBLOB(const QByteArray& array, BLOBEscapingType type);

/*! @return byte array converted from @a data of length @a length.
 @a data is escaped in format used by PostgreSQL's bytea datatype
 described at http://www.postgresql.org/docs/8.1/interactive/datatype-binary.html
 This function is used by PostgreSQL Predicate and migration drivers. */
PREDICATE_EXPORT QByteArray pgsqlByteaToByteArray(const char* data, int length);

/*! @return int list converted from string list.
   If @a ok is not 0, *ok is set to result of the conversion. */
PREDICATE_EXPORT QList<int> stringListToIntList(const QStringList &list, bool *ok);

/*! @return string converted from list @a list.
   Separators are ',' characters, "," and "\\" are escaped.
    @see Predicate::deserializeList() */
PREDICATE_EXPORT QString serializeList(const QStringList &list);

/*! @return string list converted from @a data which was built using serializeList().
   Separators are ',' characters, escaping is assumed as "\\,". */
PREDICATE_EXPORT QStringList deserializeList(const QString &data);

/*! @return int list converted from @a data which was built using serializeList().
   Separators are ',' characters, escaping is assumed as "\\,".
   If @a ok is not 0, *ok is set to result of the conversion.
   @see Predicate::stringListToIntList() */
PREDICATE_EXPORT QList<int> deserializeIntList(const QString &data, bool *ok);

/*! @return string value serialized from a variant value @a v.
 This functions works like QVariant::toString() except the case when @a v is of type:
 - QByteArray - in this case Predicate::escapeBLOB(v.toByteArray(), Predicate::BLOBEscapeHex) is used.
 - QStringList - in this case Predicate::serializeList(v.toStringList()) is used.

 This function is needed for handling values of random type, for example "defaultValue"
 property of table fields can contain value of any type.
 Note: the returned string is an unescaped string. */
PREDICATE_EXPORT QString variantToString(const QVariant& v);

/*! @return variant value of type @a type for a string @a s that was previously serialized using
 @ref variantToString( const QVariant& v ) function.
 @a ok is set to the result of the operation. With exception for types mentioned in documentation
 of variantToString(), QVariant::convert() is used for conversion. */
PREDICATE_EXPORT QVariant stringToVariant(const QString& s, QVariant::Type type, bool* ok);

/*! @return true if setting default value for @a field field is allowed. Fields with unique
 (and thus primary key) flags set do not accept  default values.
 False is returned aslo if @a field is 0. */
PREDICATE_EXPORT bool isDefaultValueAllowed(Field* field);

/*! Gets limits for values of type @a type. The result is put into @a minValue and @a maxValue.
 Supported types are Byte, ShortInteger, Integer and BigInteger
 Results for BigInteger or non-integer types are the same as for Integer due
 to limitation of int type.
 Signed integers are assumed. */
//! @todo add support for unsigned flag
PREDICATE_EXPORT void getLimitsForType(Field::Type type, int &minValue, int &maxValue);

/*! @return type that's maximum of two integer types @a t1 and @a t2, e.g. Integer for (Byte, Integer).
 If one of the types is not of the integer group, Field::InvalidType is returned.
 Returned type may not fit to the result of evaluated expression that involves the arguments.
 For example, 100 is within Byte type, maximumForIntegerTypes(Byte, Byte) is Byte but result
 of 100 * 100 exceeds the range of Byte. */
PREDICATE_EXPORT Field::Type maximumForIntegerTypes(Field::Type t1, Field::Type t2);

/*! @return QVariant value converted from null-terminated @a data string.
 In case of BLOB type, @a data is not null terminated, so passing length is needed. */
inline QVariant cstringToVariant(const char* data, Field* f, int length = -1)
{
    if (!data)
        return QVariant();
    // from most to least frequently used types:

    if (!f || f->isTextType())
        return QString::fromUtf8(data, length);
    if (f->isIntegerType()) {
        if (f->type() == Field::BigInteger)
            return QVariant(QString::fromLatin1(data, length).toLongLong());
        return QVariant(QString::fromLatin1(data, length).toInt());
    }
    if (f->isFPNumericType())
        return QString::fromLatin1(data, length).toDouble();
    if (f->type() == Field::BLOB)
        return QByteArray(data, length);
    // the default
//! @todo date/time?
    QVariant result(QString::fromUtf8(data, length));
    if (!result.convert(Field::variantType(f->type())))
        return QVariant();
    return result;
}

/*! @return default file-based driver mime type
 (typically something like "application/x-kexiproject-sqlite") */
PREDICATE_EXPORT QString defaultFileBasedDriverMimeType();

/*! @return icon name for default file-based driver
 (typically icon for something like "application/x-kexiproject-sqlite").
 @see Predicate::defaultFileBasedDriverMimeType() */
PREDICATE_EXPORT QString defaultFileBasedDriverIcon();

/*! @return default file-based driver name (currently, "sqlite"). */
PREDICATE_EXPORT QString defaultFileBasedDriverName();

#ifdef PREDICATE_DEBUG_GUI
//! A prototype of handler for GUI debugger
typedef void(*DebugGUIHandler)(const QString&);

//! Sets handler for GUI debugger
PREDICATE_EXPORT void setDebugGUIHandler(DebugGUIHandler handler);

//! Outputs string @a text to the GUI debugger
PREDICATE_EXPORT void debugGUI(const QString& text);

//! A prototype of handler for GUI debugger (specialized for the Alter Table feature)
typedef void(*AlterTableActionDebugGUIHandler)(const QString&, int);

//! Sets handler for GUI debugger (specialized for the Alter Table feature)
PREDICATE_EXPORT void setAlterTableActionDebugHandler(AlterTableActionDebugGUIHandler handler);

//! Outputs string @a text to the GUI debugger (specialized for the Alter Table feature);
//! @a nestingLevel can be provided for nested outputs.
PREDICATE_EXPORT void alterTableActionDebugGUI(const QString& text, int nestingLevel = 0);
#endif

//! @return @a string if it is not empty, else returns @a stringIfEmpty.
/*! This function is an optimization in cases when @a string is a result of expensive
 functioncall because any evaluation will be performed once, not twice. Another advantage
 is simpified code through the functional approach.
 The function expects bool isEmpty() method to be present in type T, so T can typically
 be QString or QByteArray. */
template<typename T>
T iifNotEmpty(const T &string, const T &stringIfEmpty)
{
    return string.isEmpty() ? stringIfEmpty : string;
}

//! @overload iifNotEmpty(const T &string, const T &stringIfEmpty)
template<typename T>
T iifNotEmpty(const QByteArray &string, const T &stringIfEmpty)
{
    return iifNotEmpty(QString(string), stringIfEmpty);
}

//! @overload iifNotEmpty(const T &string, const T &stringIfEmpty)
template<typename T>
T iifNotEmpty(const T &string, const QByteArray &stringIfEmpty)
{
    return iifNotEmpty(string, QString(stringIfEmpty));
}

/*! @return a list of paths that Predicate will search when dynamically loading libraries (plugins)
 This is basicaly list of directories returned QCoreApplication::libraryPaths() that have readable
 subdirectory "predicate".
 @see QCoreApplication::libraryPaths() */
PREDICATE_EXPORT QStringList libraryPaths();

/*! @return new temporary name suitable for creating new table.
 The name has mask tmp__{baseName}{rand} where baseName is passed as argument and {rand}
 is a 10 digits long hexadecimal number. @a baseName can be empty. It is adviced to use
 the returned name as quickly as possible for creating new physical table.
 It is not 100% guaranteed that table with this name will not exist at an attempt of creation
 but it is very unlikely. The function checks for existence of table in connection @a conn.*/
PREDICATE_EXPORT QString temporaryTableName(Connection *conn, const QString &baseName);

/*! @return absolute path to "sqlite3" program.
 Empty string is returned if the program was not found. */
PREDICATE_EXPORT QString sqlite3ProgramPath();

/*! Imports file in SQL format from @a inputFileName into @a outputFileName.
 Works for any SQLite 3 dump file. Requires access to executing the "sqlite3" command.
 File named @a outputFileName will be silently overwritten with a new SQLite 3 database file.
 @return true on success. */
PREDICATE_EXPORT bool importSqliteFile(const QString &inputFileName, const QString &outputFileName);

} // namespace Predicate

//! Sends information about table or query schema @a schema to debug output @a dbg.
PREDICATE_EXPORT QDebug operator<<(QDebug dbg, const Predicate::TableOrQuerySchema& schema);

#endif
