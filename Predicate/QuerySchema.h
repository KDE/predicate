/* This file is part of the KDE project
   Copyright (C) 2003-2014 Jarosław Staniek <staniek@kde.org>

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

#ifndef PREDICATE_QUERYSCHEMA_H
#define PREDICATE_QUERYSCHEMA_H

#include <QVector>
#include <QHash>

#include <Predicate/FieldList>
#include <Predicate/Object>
#include <Predicate/TableSchema>
#include <Predicate/Relationship>
#include <Predicate/QuerySchemaParameter>

namespace Predicate
{

class Connection;
class QueryAsterisk;
class QuerySchemaPrivate;

//! @short Helper class that assigns additional information for the column in a query
/*! The following information is assigned:
   - alias
   - visibility
  QueryColumnInfo::Vector is created and returned by QuerySchema::fieldsExpanded().
  It is efficiently cached within the QuerySchema object.
*/
class PREDICATE_EXPORT QueryColumnInfo
{
public:
    typedef QVector<QueryColumnInfo*> Vector;
    typedef QList<QueryColumnInfo*> List;
    typedef QList<QueryColumnInfo*>::ConstIterator ListIterator;

    QueryColumnInfo(Field *f, const QString& _alias, bool _visible,
                    QueryColumnInfo *foreignColumn = 0);
    ~QueryColumnInfo();

    //! @return alias if it is not empty, field's name otherwise.
    QString aliasOrName() const;

    //! @return field's caption if it is not empty, field's alias otherwise.
    //! If alias is also empty - returns field's name.
    inline QString captionOrAliasOrName() const {
        return field->caption().isEmpty() ? aliasOrName() : field->caption();
    }

    Field *field;
    QString alias;

    /*! @return index of column with visible lookup value within the 'fields expanded' vector.
     -1 means no visible lookup value is available because there is no lookup for the column defined.
     Cached for efficiency as we use this information frequently.
     @see LookupFieldSchema::visibleVolumn() */
    inline int indexForVisibleLookupValue() const {
        return m_indexForVisibleLookupValue;
    }

    /*! Sets index of column with visible lookup value within the 'fields expanded' vector. */
    inline void setIndexForVisibleLookupValue(int index) {
        m_indexForVisibleLookupValue = index;
    }

    //! @return non-0 if this column is a visible column for other column
    QueryColumnInfo *foreignColumn() const {
        return m_foreignColumn;
    }

    //! true if this column is visible to the user (and its data is fetched by the engine)
    bool visible;

private:
    /*! Index of column with visible lookup value within the 'fields expanded' vector.
     @see indexForVisibleLookupValue() */
    int m_indexForVisibleLookupValue;

    //! Non-0 if this column is a visible column for @a m_foreignColumn
    QueryColumnInfo *m_foreignColumn;
};

//! @short Predicate::OrderByColumn provides information about a single query column used for sorting
/*! The column can be expression or table field. */
class PREDICATE_EXPORT OrderByColumn
{
public:
    typedef QList<OrderByColumn*>::ConstIterator ListConstIterator;
    OrderByColumn();
    explicit OrderByColumn(QueryColumnInfo& column, bool ascending = true, int pos = -1);

    //! Like above but used when the field @a field is not present on the list of columns.
    //! (e.g. SELECT a FROM t ORDER BY b; where T is a table with fields (a,b)).
    explicit OrderByColumn(Field& field, bool ascending = true);

    ~OrderByColumn();

    /*! @return copy of this OrderByColumn object.
     In @a fromQuery and @a toQuery is needed if column() is assigned to this info.
     Then, column info within @a toQuery will be assigned to the new OrderByColumn object,
     corresponding to column() from "this" OrderByColumn object. */
    OrderByColumn* copy(QuerySchema* fromQuery, QuerySchema* toQuery) const;

    //! A column to sort.
    inline QueryColumnInfo* column() const {
        return m_column;
    }

    /*! A helper for column() that allows you to know that sorting column
     was defined by providing its position. -1 by default.
     Example query: SELECT a, b FROM T ORDER BY 2 */
    inline int position() const {
        return m_pos;
    }

    //! A field to sort, used only in case when the second constructor was used.
    inline Field *field() const {
        return m_field;
    }

    //! @return true if ascending sorting should be performed (the default).
    inline bool ascending() const {
        return m_ascending;
    }

    //! @return true if this column is thesame as @a col
    bool operator== (const OrderByColumn& col) const {
        return m_column == col.m_column && m_field == col.m_field
               && m_ascending == col.m_ascending;
    }

    /*! @return a string like "name ASC" usable for building a SQL statement.
     If @a includeTableNames is true (the default) field is output in a form
     of "tablename.fieldname" (but only if fieldname is not a name of alias).

     @a escapingType can be used to alter default escaping type.
     If @a conn is not provided for DriverEscaping, no escaping is performed. */
    EscapedString toSQLString(bool includeTableName = true,
                              Connection *conn = 0,
                              Predicate::EscapingType escapingType = Predicate::DriverEscaping) const;

protected:
    //! Column to sort
    QueryColumnInfo* m_column; //!< 0 if m_field is non-0.
    int m_pos; //!< A helper for m_column that allows to know that sorting column
               //!< was defined by providing its position. -1 by default.
               //!< e.g. SELECT a, b FROM T ORDER BY 2
    Field* m_field; //!< Used only in case when the second contructor is used.

    bool m_ascending; //!< true if ascending sorting should be performed (the default).
};

//! A base for Predicate::OrderByColumnList
typedef QList<OrderByColumn*> OrderByColumnListBase;

//! @short Predicate::OrderByColumnList provides list of sorted columns for a query schema
class PREDICATE_EXPORT OrderByColumnList : protected OrderByColumnListBase
{
public:
    /*! Constructs empty list of ordered columns. */
    OrderByColumnList();

    /*! A copy constructor. */
    OrderByColumnList(const OrderByColumnList& other,
                      QuerySchema* fromQuery, QuerySchema* toQuery);

    ~OrderByColumnList();

    class PREDICATE_EXPORT const_iterator : public OrderByColumnListBase::const_iterator
    {
    public:
        inline const_iterator()
                : OrderByColumnListBase::const_iterator() {}
        inline const_iterator(const OrderByColumnListBase::const_iterator &o)
                : OrderByColumnListBase::const_iterator(o) {}
    };

    class PREDICATE_EXPORT iterator : public OrderByColumnListBase::iterator
    {
    public:
        inline iterator()
                : OrderByColumnListBase::iterator() {}
        inline iterator(const OrderByColumnListBase::iterator &o)
                : OrderByColumnListBase::iterator(o) {}
    };

    /*! Appends multiple fields for sorting. @a querySchema
     is used to find appropriate field or alias name.
     @return false if there is at least one name for which a field or alias name does not exist
     (all the newly appended fields are removed in this case) */
    bool appendFields(QuerySchema& querySchema,
                      const QString& field1, bool ascending1 = true,
                      const QString& field2 = QString(), bool ascending2 = true,
                      const QString& field3 = QString(), bool ascending3 = true,
                      const QString& field4 = QString(), bool ascending4 = true,
                      const QString& field5 = QString(), bool ascending5 = true);

    /*! Appends column @a columnInfo. Ascending sorting is set is @a ascending is true. */
    void appendColumn(QueryColumnInfo& columnInfo, bool ascending = true);

    /*! Appends a field @a field. Ascending sorting is set is @a ascending is true.
     Read documentation of @ref OrderByColumn(const Field& field, bool ascending = true)
     for more info. */
    void appendField(Field& field, bool ascending = true);

    /*! Appends field with a name @a field. Ascending sorting is set is @a ascending is true.
     @return true on successful appending, and false if there is no such field or alias
     name in the @a querySchema. */
    bool appendField(QuerySchema& querySchema, const QString& fieldName,
                     bool ascending = true);

    /*! Appends a column that is at position @a pos (counted from 0).
     @return true on successful adding and false if there is no such position @a pos. */
    bool appendColumn(QuerySchema& querySchema, bool ascending = true, int pos = -1);

    /*! @return true if the list is empty. */
    bool isEmpty() const {
        return OrderByColumnListBase::isEmpty();
    }

    /*! @return number of elements of the list. */
    uint count() const {
        return OrderByColumnListBase::count();
    }

    /*! Removes all elements from the list (deletes them). */
    void clear();

    iterator begin() {
        return OrderByColumnListBase::begin();
    }
    iterator end() {
        return OrderByColumnListBase::end();
    }
    const_iterator constBegin() const {
        return OrderByColumnListBase::constBegin();
    }
    const_iterator constEnd() const {
        return OrderByColumnListBase::constEnd();
    }

    /*! @return a string like "name ASC, 2 DESC" usable for building a SQL statement.
     If @a includeTableNames is true (the default) fields are output in a form
     of "tablename.fieldname".

     @a escapingType can be used to alter default escaping type.
     If @a conn is not provided for DriverEscaping, no escaping is performed. */
    EscapedString toSQLString(bool includeTableNames = true,
                              Connection *conn = 0,
                              Predicate::EscapingType escapingType = Predicate::DriverEscaping) const;
};

//! @short Predicate::QuerySchema provides information about database query
/*! The query that can be executed using Predicate-compatible SQL database engine
 or used as an introspection tool. Predicate parser builds QuerySchema objects
 by parsing SQL statements. */
class PREDICATE_EXPORT QuerySchema : public FieldList, public Object
{
public:
    /*! Creates empty query object (without columns). */
    QuerySchema();

    /*! Creates query schema object that is equivalent to "SELECT * FROM table"
     sql command. Schema of @a table is used to contruct this query --
     it is defined by just adding all the fields to the query in natural order.
     To avoid problems (e.g. with fields added outside of Kexi using ALTER TABLE)
     we do not use "all-tables query asterisk" (see QueryAsterisk) item to achieve
     this effect.

     Properties such as the name and caption of the query are inherited
     from table schema.

     We consider that query schema based on @a table is not (a least yet) stored
     in a system table, so query connection is set to NULL
     (even if @a tableSchema's connection is not NULL).
     Id of the created query is set to 0. */
    explicit QuerySchema(TableSchema *tableSchema);

    /*! Copy constructor. Creates deep copy of @a querySchema.
     QueryAsterisk objects are deeply copied while only pointers to Field objects are copied. */
    QuerySchema(const QuerySchema& querySchema);

    virtual ~QuerySchema();

    /*! Inserts @a field to the columns list at @a position.
     Inserted field will not be owned by this QuerySchema object,
     but still by corresponding TableSchema.

     As @a field object you can also pass Predicate::QueryAsterisk,
     (see QueryAsterisk class description).

     Note: After inserting a field, corresponding table will be automatically
     added to query's tables list if it is not present there (see tables()).
     Field must have its table assigned.

     Added field will be visible. Use insertField(position, field, false)
     to add invisible field.
    */
    virtual FieldList& insertField(uint position, Field *field);

    /* Like above method, but you can also set column's visibility.
     New column is not bound explicitly to any table.
    */
    FieldList& insertField(uint position, Field *field, bool visible);

    /* Like above method, but you can also explicitly bound the new column
     to specific position on tables list.
     If @a visible is true (the default), the field will be visible.
     If bindToTable==-1, no particular table should be bound.
     @see tableBoundToColumn(uint columnPosition) */
    FieldList& insertField(uint position, Field *field,
                           int bindToTable, bool visible = true);

    /*! Adds @a field to the columns list.
     If @a visible is true (the default), the field will be visible.
     @see insertField() */
    FieldList& addField(Field* field, bool visible = true);

    /*! Adds @a field to the columns list. Also binds to a table
     at @a bindToTable position. Use bindToTable==-1 if no table should be bound.
     If @a visible is true (the default), the field will be visible.
     @see insertField()
     @see tableBoundToColumn(uint columnPosition)
    */
    FieldList& addField(Field* field, int bindToTable,
                        bool visible = true);

    /*! Removes field from the columns list. Use with care. */
    virtual bool removeField(Field *field);

    /*! Adds a field built on top of @a expr expression.
     This creates a new Field object and adds it to the query schema using addField().
     @a expr will be owned by the query object. */
    FieldList& addExpression(const Expression& expr, bool visible = true);

    /*! @return visibility flag for column at @a position.
     By default column is visible. */
    bool isColumnVisible(uint position) const;

    //! Sets visibility flag for column at @a position to @a v.
    void setColumnVisible(uint position, bool v);

    /*! Adds @a asterisk at the and of columns list. */
    FieldList& addAsterisk(QueryAsterisk *asterisk, bool visible = true);

    /*! Removes all columns and their aliases from the columns list,
     removes all tables and their aliases from the tables list within this query.
     Sets master table information to NULL.
     Does not destroy any objects though. Clears name and all other properties.
     @see FieldList::clear() */
    virtual void clear();

    /*! If query was created using a connection,
      returns this connection object, otherwise NULL. */
    Connection* connection() const;

    /*! @return table that is master to this query.
     All potentially-editable columns within this query belong just to this table.
     This method also can return NULL if there are no tables at all,
     or if previously assigned master table schema has been removed
     with removeTable().
     Every query that has at least one table defined, should have
     assigned a master table.
     If no master table is assigned explicitly, but only one table used in this query,
     a single table is returned here, even if there are table aliases,
     (e.g. "T" table is returned for "SELECT T1.A, T2.B FROM T T1, T T2" statement). */
    TableSchema* masterTable() const;

    /*! Sets master table of this query to @a table.
      This table should be also added to query's tables list
      using addTable(). If @a table equals NULL, nothing is performed.
      @see masterTable() */
    void setMasterTable(TableSchema *table);

    /*! @return list of tables used in a query.
     This also includes master table.
     @see masterTable() */
    TableSchema::List* tables() const;

    /*! Adds @a table schema as one of tables used in a query.
     If @a alias is not empty, it will be assigned to this table
     using setTableAlias(position, alias). */
    void addTable(TableSchema *table, const QString& alias = QString());

    /*! Removes @a table schema from this query.
     This does not destroy @a table object but only takes it out of the list.
     If this table was master for the query, master table information is also
     invalidated. */
    void removeTable(TableSchema *table);

    /*! @return table with name @a tableName or 0 if this query has no such table. */
    TableSchema* table(const QString& tableName) const;

    /*! @return true if the query uses @a table. */
    bool contains(TableSchema *table) const;

    /*! Convenience function.
     @return table field by searching through all tables in this query.
     The field does not need to be included on the list of query columns.
     Similarly, query aliases are not taken into account.

     @a tableOrTableAndFieldName string may contain table name and field name
     with '.' character between them, e.g. "mytable.myfield".
     This is recommended way to avoid ambiguity.
     0 is returned if the query has no such
     table defined of the table has no such field defined.
     If you do not provide a table name, the first field found is returned.

     QuerySchema::table("mytable")->field("myfield") could be
     alternative for findTableField("mytable.myfield") but it can crash
     if "mytable" is not defined in the query.

     @see Predicate::splitToTableAndFieldParts()
    */
    Field* findTableField(const QString &tableOrTableAndFieldName) const;

    /*! @return alias of a column at @a position or null string
     If there is no alias for this column
     or if there is no such column within the query defined.
     If the column is an expression and has no alias defined,
     a new unique alias will be generated automatically on this call.
    */
    QString columnAlias(uint position) const;

    /*! @return number of column aliases */
    int columnAliasesCount() const;

    /*! Provided for convenience.
     @return true if a column at @a position has non empty alias defined
     within the query.
     If there is no alias for this column,
     or if there is no such column in the query defined, false is returned. */
    bool hasColumnAlias(uint position) const;

    /*! Sets @a alias for a column at @a position, within the query.
     Passing empty string to @a alias clears alias for a given column. */
    void setColumnAlias(uint position, const QString& alias);

    /*! @return a table position (within FROM section),
     that is bound to column at @a columnPosition (within SELECT section).
     This information can be used to find if there is alias defined for
     a table that is referenced by a given column.

     For example, for "SELECT t2.id FROM table1 t1, table2 t2" query statement,
     columnBoundToTable(0) returns 1, what means that table at position 1
     (within FROM section) is bound to column at position 0, so we can
     now call tableAlias(1) to see if we have used alias for this column (t2.id)
     or just a table name (table2.id).

     These checks are performed e.g. by Connection::selectStatement()
     to construct a statement string maximally identical to originally
     defined query statement.

     -1 is returned if:
      - @a columnPosition is out of range (i.e. < 0 or >= fieldCount())
      - a column at @a columnPosition is not bound to any table (i.e.
        no database field is used for this column,
        e.g. "1" constant for "SELECT 1 from table" query statement)
    */
    int tableBoundToColumn(uint columnPosition) const;

    /*! @return number of table aliases */
    int tableAliasesCount() const;

    /*! @return alias of a table at @a position (within FROM section)
     or null string if there is no alias for this table
     or if there is no such table within the query defined. */
    QString tableAlias(uint position) const;

    /*! @return alias of a table @a tableName (within FROM section)
     or empty value if there is no alias for this table
     or if there is no such table within the query defined. */
    QString tableAlias(const QString& tableName) const;

    /*! @return alias of a table @a tableName (within FROM section).
    If there is no alias for this table, its name is returned.
    Empty value is returned if there is no such table within the query defined. */
    QString tableAliasOrName(const QString& tableName) const;

    /*! @return table position (within FROM section) that has attached
     alias @a name.
     If there is no such alias, -1 is returned.
     Only first table's position attached for this alias is returned.
     It is not especially bad, since aliases rarely can be duplicated,
     what leads to ambiguity.
     Duplicated aliases are only allowed for trivial queries that have
     no database fields used within their columns,
     e.g. "SELECT 1 from table1 t, table2 t" is ok
     but "SELECT t.id from table1 t, table2 t" is not.
    */
    int tablePositionForAlias(const QString& name) const;

    /*! @return position (within the FROM section) of table @a tableName.
     -1 is returend if there's no such table declared in the FROM section.
     @see tablePositions()
    */
    int tablePosition(const QString& tableName) const;

    /*! @return a list of all occurrences of table @a tableName (within the FROM section).
     E.g. for "SELECT * FROM table t, table t2" tablePositions("table") returns {0, 1} list.
     Empty list is returned if there's no table @a tableName used in the FROM section at all.
     @see tablePosition() */
    QList<int> tablePositions(const QString& tableName) const;

    /*! Provided for convenience.
     @return true if a table at @a position (within FROM section of the query)
     has non empty alias defined.
     If there is no alias for this table,
     or if there is no such table in the query defined, false is returned. */
    bool hasTableAlias(uint position) const;

    /*! @return column position that has defined alias @a name.
     If there is no such alias, -1 is returned. */
    int columnPositionForAlias(const QString& name) const;

    /*! Sets @a alias for a table at @a position (within FROM section
     of the query).
     Passing empty sting to @a alias clears alias for a given table
     (only for specified @a position). */
    void setTableAlias(uint position, const QString& alias);

    /*! @return a list of relationships defined for this query */
    Relationship::List* relationships() const;

    /*! Adds a new relationship defined by @a field1 and @a field2.
     Both fields should belong to two different tables of this query.
     This is convenience function useful for a typical cases.
     It automatically creates Relationship object for this query.
     If one of the fields are primary keys, it will be detected
     and appropriate master-detail relation will be established.
     This functiuon does nothing if the arguments are invalid. */
    Relationship* addRelationship(Field *field1, Field *field2);

    /*! @return list of QueryAsterisk objects defined for this query */
    Field::List* asterisks() const;

    /*! @return field for @a identifier or 0 if no field for this name
     was found within the query. fieldsExpanded() method is used
     to lookup expanded list of the query fields, so queries with asterisks
     are processed well.
     If a Field.has alias defined, name is not taken into account,
     but only its alias. If a Field.has no alias:
     - field's name is checked
     - field's table and field's name are checked in a form of "tablename.fieldname",
       so you can provide @a identifier in this form to avoid ambiguity.

     If there are more than one fields with the same name equal to @a identifier,
     first-found is returned (checking is performed from first to last query field).
     Structures needed to compute result of this method are cached,
     so only first usage costs o(n) - another usages cost o(1).

     Example:
     Let query be defined by "SELECT T.B AS X, T.* FROM T" statement and let T
     be table containing fields A, B, C.
     Expanded list of columns for the query is: T.B AS X, T.A, T.B, T.C.
     - Calling field("B") will return a pointer to third query column (not the first,
       because it is covered by "X" alias). Additionally, calling field("X")
       will return the same pointer.
     - Calling field("T.A") will return the same pointer as field("A").
     */
    virtual Field* field(const QString& name, bool expanded) const;

    /*! This is overloaded method Field* field(const QString& name, bool expanded)
     with expanded = true. This method is also a product of inheritance from FieldList.  */
    virtual Field* field(const QString& name) const;

    /*! Like QuerySchema::field(const QString& name) but returns not only Field
     object for @a identifier but entire QueryColumnInfo object.
     @a identifier can be:
     - a fieldname
     - an aliasname
     - a tablename.fieldname
     - a tablename.aliasname
     Note that if there are two occurrrences of the same name,
     only the first is accessible using this method. For instance,
     calling columnInfo("name") for "SELECT t1.name, t2.name FROM t1, t2" statement
     will only return the column related to t1.name and not t2.name, so you'll need to
     explicitly specify "t2.name" as the identifier to get the second column. */
    QueryColumnInfo* columnInfo(const QString& identifier, bool expanded = true) const;

    /*! Options used in fieldsExpanded(). */
    enum FieldsExpandedOptions {
        Default,                      //!< All fields are returned even if duplicated
        Unique,                       //!< Unique list of fields is returned
        WithInternalFields,           //!< Like Default but internal fields (for lookup) are appended
        WithInternalFieldsAndRecordId //!< Like WithInternalFields but record ID (big int type) field
        //!< is appended after internal fields
    };

    /*! @return fully expanded list of fields.
     QuerySchema::fields() returns vector of fields used for the query columns,
     but in a case when there are asterisks defined for the query,
     it does not expand QueryAsterisk objects to field lists but return every
     asterisk as-is.
     This could be inconvenient when you need just a fully expanded list of fields,
     so this method does the work for you.

     If @a options is Unique, each field is returned in the vector only once
     (first found field is selected).
     Note however, that the same field can be returned more than once if it has attached
     a different alias.
     For example, let t be TABLE( a, b ) and let query be defined
     by "SELECT *, a AS alfa FROM t" statement. Both fieldsExpanded(Default)
     and fieldsExpanded(Unique) will return [ a, b, a (alfa) ] list.
     On the other hand, for query defined by "SELECT *, a FROM t" statement,
     fieldsExpanded(Default) will return [ a, b, a ] list while
     fieldsExpanded(Unique) will return [ a, b ] list.

     If @a options is WithInternalFields or WithInternalFieldsAndRecordID,
     additional internal fields are also appended to the vector.

     If @a options is WithInternalFieldsAndRecordId,
     one fake BigInteger column is appended to make space for Record ID column used
     by Predicate::Cursor implementations. For example, let persons be TABLE( surname, city_id ),
     let city_number reference cities.is in TABLE cities( id, name ) and let query q be defined
     by "SELECT * FROM t" statement. If we want to display persons' city names instead of city_id's.
     To do this, cities.name has to be retrieved as well, so the following statement should be used:
     "SELECT * FROM persons, cities.name LEFT OUTER JOIN cities ON persons.city_id=cities.id".
     Thus, calling fieldsExpanded(WithInternalFieldsAndRecordId) will return 4 elements instead of 2:
     persons.surname, persons.city_id, cities.name, {ROWID}. The {ROWID} item is the placeholder
     used for fetching ROWID by Predicate cursors.

     By default, all fields are returned in the vector even
     if there are multiple occurrences of one or more (options == Default).

     Note: You should assign the resulted vector in your space - it will be shared
     and implicity copied on any modification.
     This method's result is cached by QuerySchema object.
    @todo js: UPDATE CACHE!
    */
    QueryColumnInfo::Vector fieldsExpanded(FieldsExpandedOptions options = Default) const;

    /*! @return list of fields internal fields used for lookup columns. */
    QueryColumnInfo::Vector internalFields() const;

    /*! @return info for expanded of internal field at index @a index.
     The returned field can be either logical or internal (for lookup),
     the latter case is true if @a index &gt;= fieldsExpanded().count().
     Equivalent of QuerySchema::fieldsExpanded(WithInternalFields).at(index). */
    QueryColumnInfo* expandedOrInternalField(uint index) const;

    /*! Options used in columnsOrder(). */
    enum ColumnsOrderOptions {
        UnexpandedList,                 //!< A map for unexpanded list is created
        UnexpandedListWithoutAsterisks, //!< A map for unexpanded list is created, with asterisks skipped
        ExpandedList                    //!< A map for expanded list is created
    };

    /*! @return a hash for fast lookup of query columns' order.
     - If @a options is UnexpandedList, each QueryColumnInfo pointer is mapped to the index
       within (unexpanded) list of fields, i.e. "*" or "table.*" asterisks are considered
       to be single items.
     - If @a options is UnexpandedListWithoutAsterisks, each QueryColumnInfo pointer
       is mapped to the index within (unexpanded) list of columns that come from asterisks
       like "*" or "table.*" are not included in the map at all.
     - If @a options is ExpandedList (the default) this method provides is exactly opposite
       information compared to vector returned by fieldsExpanded().

     This method's result is cached by the QuerySchema object.
     Note: indices of internal fields (see internalFields()) are also returned
     here - in this case the index is counted as a sum of size(e) + i (where "e" is
     the list of expanded fields and i is the column index within internal fields list).
     This feature is used eg. at the end of Connection::updateRecord() where need indices of
     fields (including internal) to update all the values in memory.

     Example use: let t be table (int id, name text, surname text) and q be query
     defined by a statement "select * from t".

     - columnsOrder(ExpandedList) will return the following map: QueryColumnInfo(id)->0,
       QueryColumnInfo(name)->1, QueryColumnInfo(surname)->2.
     - columnsOrder(UnexpandedList) will return the following map: QueryColumnInfo(id)->0,
       QueryColumnInfo(name)->0, QueryColumnInfo(surname)->0 because the column
       list is not expanded. This way you can use the returned index to get Field*
       pointer using field(uint) method of FieldList superclass.
     - columnsOrder(UnexpandedListWithoutAsterisks) will return the following map:
       QueryColumnInfo(id)->0,
    */
    QHash<QueryColumnInfo*, int> columnsOrder(ColumnsOrderOptions options = ExpandedList) const;

    /*! @return table describing order of primary key (PKEY) fields within the query.
     Indexing is performed against vector returned by fieldsExpanded().
     It is usable for e.g. Conenction::updateRecord(), when we need
     to locate each primary key's field in a constant time.

     Returned vector is owned and cached by QuerySchema object. When you assign it,
     it is implicity shared. Its size is equal to number of primary key
     fields defined for master table (masterTable()->primaryKey()->fieldCount()).

     Each element of the returned vector:
     - can belong to [0..fieldsExpanded().count()-1] if there is such
       primary key's field in the fieldsExpanded() list.
     - can be equal to -1 if there is no such primary key's field
       in the fieldsExpanded() list.

     If there are more than one primary key's field included in the query,
     only first-found column (oin the fieldsExpanded() list) for each pkey's field is included.

     Returns empty vector if there is no master table or no master table's pkey.
     @see example for pkeyFieldsCount().
    @todo js: UPDATE CACHE!
    */
    QVector<int> pkeyFieldsOrder() const;

    /*! @return number of master table's primary key fields included in this query.
     This method is useful to quickly check whether the vector returned by pkeyFieldsOrder()
     if filled completely.

     User e.g. in Connection::updateRecord() to check if entire primary
     key information is specified.

     Examples: let table T has (ID1 INTEGER, ID2 INTEGER, A INTEGER) fields,
     and let (ID1, ID2) is T's primary key.
     -# The query defined by "SELECT * FROM T" statement contains all T's
        primary key's fields as T is the master table, and thus pkeyFieldsCount()
        will return 2 (both primary key's fields are in the fieldsExpanded() list),
        and pkeyFieldsOrder() will return vector {0, 1}.
     -# The query defined by "SELECT A, ID2 FROM T" statement, and thus pkeyFieldsCount()
        will return 1 (only one primary key's field is in the fieldsExpanded() list),
        and pkeyFieldsOrder() will return vector {-1, 1}, as second primary key's field
        is at position #1 and first field is not specified at all within the query.
    */
    uint pkeyFieldsCount();

    /*! @return a list of field infos for all auto-incremented fields
     from master table of this query. This result is cached for efficiency.
     fieldsExpanded() is used for that.
    */
    QueryColumnInfo::List* autoIncrementFields() const;

    /*! @return a preset statement (if any). */
    EscapedString statement() const;

    /*! Forces a query statement (i.e. no statement is composed from QuerySchema's content) */
    void setStatement(const EscapedString& statement);

    /*! @return a string that is a result of concatenating all column names
     for @a infolist, with "," between each one.
     This is usable e.g. as argument like "field1,field2"
     for "INSERT INTO (xxx) ..". The result of this method is effectively cached,
     and it is invalidated when set of fields changes (e.g. using clear()
     or addField()).

     This method is similar to FieldList::sqlFieldsList() it just uses
     QueryColumnInfo::List instead of Field::List.

     @a escapingType can be used to alter default escaping type.
     If @a conn is not provided for DriverEscaping, no escaping is performed. */
    static EscapedString sqlColumnsList(const QueryColumnInfo::List& infolist, Connection *conn = 0,
        Predicate::EscapingType escapingType = Predicate::DriverEscaping);

    /*! @return cached sql list created using sqlColumnsList() on a list returned
     by autoIncrementFields(). */
    EscapedString autoIncrementSQLFieldsList(Connection *conn) const;

    /*! Sets a WHERE expression @a exp.
     Previously set WHERE expression will be removed.
     You can pass null expression (Expression()) to remove existing WHERE expresssion. */
    void setWhereExpression(const Expression& expr);

    /*! @return WHERE expression or 0 if this query has no WHERE expression */
    Expression whereExpression() const;

    /*! Adds a part to WHERE expression.
     Simplifies creating of WHERE expression, if used instead
     of setWhereExpression(Expression *expr). */
    void addToWhereExpression(Predicate::Field *field, const QVariant& value, int relation = '=');

    /*! Sets a list of columns for ORDER BY section of the query.
     Each name on the list must be a field or alias present within the query
     and must not be covered by aliases. If one or more names cannot be found
     within the query, the method will have no effect.
     Any previous ORDER BY settings will be removed.

     Note that this information is cleared whenever you call methods that
     modify list of columns (QueryColumnInfo), i.e. insertFiled(),
     addField(), removeField(), addExpression(), etc.
     (because OrderByColumn items can point to a QueryColumnInfo that's removed by these
     methods), so you should use setOrderByColumnList() method after the query
     is completely built. */
    void setOrderByColumnList(const OrderByColumnList& list);

    /*! @return a list of columns listed in ORDER BY section of the query.
     Read notes for @ref setOrderByColumnList(). */
    OrderByColumnList* orderByColumnList();

    /*! @see orderByColumnList() */
    const OrderByColumnList* orderByColumnList() const;

    /*! @return query schema parameters. These are taked from the WHERE section
     (a tree of expression items). */
    QuerySchemaParameterList parameters() const;

protected:
    void init();

    void computeFieldsExpanded() const;

    QuerySchemaPrivate * const d;

    friend class Connection;
    friend class QuerySchemaPrivate;
};

//! @short Predicate::QueryAsterisk class encapsulates information about single asterisk in query definition
/*! There are two types of query asterisks:

 1. "Single-table" asterisk, that references all fields of given table used
 in the query.
 Example SQL statement:
 @code
 SELECT staff.*, cars.model from staff, cars WHERE staff.car = cars.number;
 @endcode
 The "staff.*" element is our "single-table" asterisk;
 this tells us that we want to get all fields of table "staff".

 2. "All-tables" asterisk, that references all fields of all tables used in the query.
 Example SQL statement:
 @code
 SELECT * from staff, cars WHERE staff.car = cars.number;
 @endcode
 The "*" is our "all-tables" asterisk;
 this tells us that we want to get all fields of all used tables (here: "staff" and "cars").

 There can be many asterisks of 1st type defined for given single query.
 There can be one asterisk of 2nd type defined for given single query.
*/
class PREDICATE_EXPORT QueryAsterisk : public Field
{
public:
    /*! Constructs query asterisk definition object.
     Pass table schema to @a table if this asterisk should be
     of type "single-table", otherwise (if you want to define
     "all-tables" type asterisk), omit this parameter.

     QueryAsterisk objects are owned by QuerySchema object
     (not by TableSchema object like for ordinary Field objects)
     for that the QueryAsterisk object was added (using QuerySchema::addField()).
     */
    explicit QueryAsterisk(QuerySchema *query, TableSchema *table = 0);

    /*! Constructs a deep copu of query asterisk definition object @a asterisk. */
    QueryAsterisk(const QueryAsterisk& asterisk);

    virtual ~QueryAsterisk();

    /*! @return Query object for that this asterisk object is defined */
    QuerySchema *query() const {
        return static_cast<QuerySchema*>(m_parent);
    }

    /*! @return Table schema for this asterisk
     if it has "single-table" type (1st type)
     or NULL if it has "all-tables" type (2nd type) defined. */
    virtual TableSchema* table() const {
        return m_table;
    }

    /*! Sets table schema for this asterisk.
     @a table may be NULL - then the asterisk becames "all-tables" type asterisk. */
    virtual void setTable(TableSchema *table);

    /*! This is convenience method that returns true
     if the asterisk has "all-tables" type (2nd type).*/
    bool isSingleTableAsterisk() const {
        return m_table != NULL;
    }

    /*! This is convenience method that returns true
     if the asterisk has "single-tables" type (2nd type).*/
    bool isAllTableAsterisk() const {
        return m_table == NULL;
    }

protected:
    //! @return a deep copy of this object. Used in FieldList(const FieldList& fl).
    virtual Field* copy() const;

    /*! Table schema for this asterisk */
    TableSchema* m_table;

    friend class QuerySchema;
};

} //namespace Predicate

//! Sends information about column info @a info to debug output @a dbg.
PREDICATE_EXPORT QDebug operator<<(QDebug dbg, const Predicate::QueryColumnInfo& info);

//! Sends order-by-column information @a order to debug output @a dbg.
PREDICATE_EXPORT QDebug operator<<(QDebug dbg, const Predicate::OrderByColumn& order);

//! Sends order-by-column-list information @a list to debug output @a dbg.
PREDICATE_EXPORT QDebug operator<<(QDebug dbg, const Predicate::OrderByColumnList& list);

//! Sends query schema information @a query to debug output @a dbg.
PREDICATE_EXPORT QDebug operator<<(QDebug dbg, const Predicate::QuerySchema& query);

//! Sends query asterisk information @a asterisk to debug output @a dbg.
PREDICATE_EXPORT QDebug operator<<(QDebug dbg, const Predicate::QueryAsterisk& asterisk);

#endif
