/* This file is part of the KDE project
   Copyright (C) 2003 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2003-2010 Jarosław Staniek <staniek@kde.org>

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

#ifndef PREDICATE_TABLESCHEMA_H
#define PREDICATE_TABLESCHEMA_H

#include <QList>
#include <QString>
#include <QPointer>
#include <QVector>
#include <QtDebug>

#include <Predicate/FieldList>
#include <Predicate/IndexSchema>
#include <Predicate/Relationship>

namespace Predicate
{

class Connection;
class LookupFieldSchema;

/*! Provides information about native database table
  that can be stored using Predicate database engine.
*/
class PREDICATE_EXPORT TableSchema : public FieldList, public Object
{
public:
    typedef QList<TableSchema*> List; //!< Type of tables list
    typedef QList<TableSchema>::ConstIterator ListIterator; //!< Iterator for tables list

    explicit TableSchema(const QString& name);
    explicit TableSchema(const Object& object);
    TableSchema();

    /*! Copy constructor.
     if @a copyId is true, it's copied as well, otherwise the table id becomes -1,
     what is usable when we want to store the copy as an independent table. */
    TableSchema(const TableSchema& ts, bool copyId = true);

    /*! Copy constructor like @ref TableSchema(const TableSchema&, bool).
     @a id is set as the table identifier. This is rarely usable, e.g.
     in project and data migration routines when we need to need deal with unique identifiers;
     @see KexiMigrate::performImport(). */
    TableSchema(const TableSchema& ts, int id);

    virtual ~TableSchema();

    /*! Inserts @a field into a specified position (@a index).
     'order' property of @a field is set automatically. */
    virtual FieldList& insertField(uint index, Field *field);

    /*! Reimplemented for internal reasons. */
    virtual bool removeField(Field *field);

    /*! @return list of fields that are primary key of this table.
     This method never returns 0 value,
     if there is no primary key, empty IndexSchema object is returned.
     IndexSchema object is owned by the table schema. */
    IndexSchema* primaryKey() const {
        return m_pkey;
    }

    /*! Sets table's primary key index to @a pkey. Pass pkey==0 if you want to unassign
     existing primary key ("primary" property of given IndexSchema object will be
     cleared then so this index becomes ordinary index, still existing on table indeices list).

     If this table already has primary key assigned,
     it is unassigned using setPrimaryKey(0) call.

     Before assigning as primary key, you should add the index to indices list
     with addIndex() (this is not done automatically!).
    */
    void setPrimaryKey(IndexSchema *pkey);

    const IndexSchema::ListIterator indicesIterator() const {
        return IndexSchema::ListIterator(m_indices.constBegin());
    }

    const IndexSchema::List* indices() const {
        return &m_indices;
    }

    /*! Removes all fields from the list, clears name and all other properties.
      @see FieldList::clear() */
    virtual void clear();

    /*! Sends information about fields of this table schema to debug output @a dbg. */
    QDebug debugFields(QDebug dbg) const;

    /*! @return connection object if table was created/retrieved using a connection,
      otherwise 0. */
    Connection* connection() const;

    /*! @return true if this is Predicate storage system's table
     (used internally by Predicate). This helps in hiding such tables
     in applications (if desired) and will also enable lookup of system
     tables for schema export/import functionality.

     Any internal Predicate system table's schema (kexi__*) has
     cleared its Object part, e.g. id=-1 for such table,
     and no description, caption and so on. This is because
     it represents a native database table rather that extended Kexi table.

     isPredicateSystem()==true implies isNative()==true.

     By default (after allocation), TableSchema object
     has this property set to false. */
    bool isPredicateSystem() const {
        return m_isPredicateSystem;
    }

    /*! Sets PredicateSystem flag to on or off. When on, native flag is forced to be on.
     When off, native flag is not affected.
     @see isPredicateSystem() */
    void setPredicateSystem(bool set);

    /*! @return query schema object that is defined by "select * from <this_table_name>"
     This query schema object is owned by the table schema object.
     It is convenient way to get such a query when it is not available otherwise.
     Always returns non-0. */
    QuerySchema* query();

    /*! @return any field not being a part of primary key of this table.
     If there is no such field, returns 0. */
    Field* anyNonPKField();

    /*! Sets lookup field schema @a lookupFieldSchema for @a fieldName.
     Passing null @a lookupFieldSchema will remove the previously set lookup field.
     @return true if @a lookupFieldSchema has been added,
     or false if there is no such field @a fieldName. */
    bool setLookupFieldSchema(const QString& fieldName, LookupFieldSchema *lookupFieldSchema);

    /*! @return lookup field schema for @a field.
     0 is returned if there is no such field in the table or this Field.has no lookup schema.
     Note that even id non-zero is returned here, you may want to check whether lookup field's
     recordSource().name() is empty (if so, the field should behave as there was no lookup field
     defined at all). */
    LookupFieldSchema *lookupFieldSchema(const Field& field) const;

    /*! @overload LookupFieldSchema *TableSchema::lookupFieldSchema( Field& field ) const */
    LookupFieldSchema *lookupFieldSchema(const QString& fieldName);

    /*! @return list of lookup field schemas for this table.
     The order is the same as the order of fields within the table. */
    QVector<LookupFieldSchema*> lookupFields() const;

protected:
    /*! Automatically retrieves table schema via connection. */
    explicit TableSchema(Connection *conn, const QString & name = QString());

    IndexSchema::List m_indices;

    Connection *m_conn;

    IndexSchema *m_pkey;

    QuerySchema *m_query; //!< cached query schema that is defined by "select * from <this_table_name>"

    class Private;
    Private * const d;

private:
    //! Used by some ctors.
    void init(Connection* conn);

    //! Used by some ctors.
    void init(const TableSchema& ts, bool copyId);

    bool m_isPredicateSystem;

    friend class Connection;
};

/*! Internal table with a name @a name. Rarely used.
 Use Connection::createTable() to create a table using this schema.
 The table will not be visible as user table.
 For example, 'kexi__blobs' table is created this way by Kexi application. */
class PREDICATE_EXPORT InternalTableSchema : public TableSchema
{
public:
    explicit InternalTableSchema(const QString& name);
    explicit InternalTableSchema(const TableSchema& ts);
    virtual ~InternalTableSchema();
};

} //namespace Predicate

//! Sends information about table schema @a table to debug output @a dbg.
PREDICATE_EXPORT QDebug operator<<(QDebug dbg, const Predicate::TableSchema& table);

#endif
