/* This file is part of the KDE project
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

#ifndef PREDICATE_FIELDLIST_H
#define PREDICATE_FIELDLIST_H

#include <QList>
#include <QHash>
#include <QString>
#include <QtDebug>

#include <Predicate/Field>
#include <Predicate/Driver>

namespace Predicate
{

class Connection;

/*! Helper class that stores list of fields. */
class PREDICATE_EXPORT FieldList
{
public:
    /*! Creates empty list of fields. If @a owner is true, the list will be
     owner of any added field, what means that these field
     will be removed on the list destruction. Otherwise, the list
     just points any field that was added.
     @see isOwner()
    */
    explicit FieldList(bool owner = false);

    /*! Copy constructor.
     If @a deepCopyFields is true, all fields are deeply copied, else only pointer are copied.
     Reimplemented in QuerySchema constructor. */
    explicit FieldList(const FieldList& fl, bool deepCopyFields = true);

    /*! Destroys the list. If the list owns fields (see constructor),
     these are also deleted. */
    virtual ~FieldList();

    /*! @return number of fields in the list. */
    inline uint fieldCount() const {
        return m_fields.count();
    }

    /*! @return true if the list is empty. */
    inline bool isEmpty() const {
        return m_fields.isEmpty();
    }

    /*! Adds @a field at the and of field list. */
    FieldList& addField(Field *field);

    /*! Inserts @a field into a specified position (@a index).

     Note: You can reimplement this method but you should still call
     this implementation in your subclass. */
    virtual FieldList& insertField(uint index, Field *field);

    /*! Removes field from the field list. Use with care.

     Note: You can reimplement this method but you should still call
     this implementation in your subclass. */
    virtual void removeField(Field *field);

    /*! @return field id or NULL if there is no such a field. */
    inline Field* field(uint id) {
        return m_fields.value(id);
    }

    /*! @return field id or NULL if there is no such a field. */
    inline const Field* field(uint id) const {
        return m_fields.value(id);
    }

    /*! @return field with name @a name or NULL if there is no such a field. */
    virtual Field* field(const QString& name) const;

    /*! @return true if this list contains given @a field. */
    inline bool hasField(const Field& field) const {
        return m_fields.contains(const_cast<Field*>(&field));
    }

    /*! @return first occurrence of @a field in the list
     or -1 if this list does not contain this field. */
    inline int indexOf(const Field& field) const {
        return m_fields.indexOf(const_cast<Field*>(&field));
    }

    /*! @return list of field names for this list. */
    QStringList names() const;

    inline Field::ListIterator fieldsIterator() const {
        return m_fields.constBegin();
    }
    inline Field::ListIterator fieldsIteratorConstEnd() const {
        return m_fields.constEnd();
    }

    inline const Field::List* fields() const {
        return &m_fields;
    }

    /*! @return list of autoincremented fields. The list is owned by this FieldList object. */
    Field::List* autoIncrementFields() const;

    /*! @return true if fields in the list are owned by this list. */
    inline bool isOwner() const {
        return m_fields.autoDelete();
    }

    /*! Removes all fields from the list. */
    virtual void clear();

    /*! Creates and returns a list that contain fields selected by name.
     At least one field (exising on this list) should be selected, otherwise 0 is
     returned. Returned FieldList object is not owned by any parent (so you need
     to destroy yourself) and Field objects included in it are not owned by it
     (but still as before, by 'this' object).
     Returned list can be usable e.g. as argument for Connection::insertRecord().
     0 is returned if at least one name cannot be found.
    */
    FieldList* subList(const QString& n1, const QString& n2 = QString(),
                       const QString& n3 = QString(), const QString& n4 = QString(),
                       const QString& n5 = QString(), const QString& n6 = QString(),
                       const QString& n7 = QString(), const QString& n8 = QString(),
                       const QString& n9 = QString(), const QString& n10 = QString(),
                       const QString& n11 = QString(), const QString& n12 = QString(),
                       const QString& n13 = QString(), const QString& n14 = QString(),
                       const QString& n15 = QString(), const QString& n16 = QString(),
                       const QString& n17 = QString(), const QString& n18 = QString()
                      );

    /*! Like above, but for QStringList. */
    FieldList* subList(const QStringList& list);

    /*! @overload subList(const QStringList&) */
    FieldList* subList(const QList<QByteArray>& list);

    /*! Like above, but with a list of field indices */
    FieldList* subList(const QList<uint>& list);

    /*! @return a string that is a result of all field names concatenated
     and with @a separator. This is usable e.g. as argument like "field1,field2"
     for "INSERT INTO (xxx) ..". The result of this method is effectively cached,
     and it is invalidated when set of fields changes (e.g. using clear()
     or addField()).

     @a tableAlias, if provided is prepended to each field, so the resulting
     names will be in form tableAlias.fieldName. This option is used for building
     queries with joins, where fields have to be spicified without ambiguity.
     See @ref Connection::selectStatement() for example use.

     @a escapingType can be used to alter default escaping type.
     If @a conn is not provided for DriverEscaping, no escaping is performed.
    */
    EscapedString sqlFieldsList(Connection *conn, const QString& separator = QLatin1String(","),
                                const QString& tableAlias = QString(),
                                Predicate::EscapingType escapingType = Predicate::DriverEscaping) const;

    /*! Like above, but this is convenient static function, so you can pass any @a list here. */
    static EscapedString sqlFieldsList(const Field::List& list, Connection *conn,
                                       const QString& separator = QLatin1String(","),
                                       const QString& tableAlias = QString(),
                                       Predicate::EscapingType escapingType = Predicate::DriverEscaping);

    /*! @internal Renames field @a oldName to @a newName.
     Do not use this for physical renaming columns. Use AlterTableHandler instead. */
    void renameField(const QString& oldName, const QString& newName);

    /*! @internal
     @overload void renameField(const QString& oldName, const QString& newName) */
    void renameField(Field *field, const QString& newName);

protected:
    Field::List m_fields;
    mutable QHash<QString, Field*> m_fields_by_name; //!< Fields collected by name. Not used by QuerySchema.
    mutable Field::List *m_autoinc_fields;

private:
    void renameFieldInternal(Predicate::Field *field, const QString& newNameLower);

    //! cached
    mutable EscapedString m_sqlFields;
};

} //namespace Predicate

//! Sends information about field list  @a list to debug output @a dbg.
PREDICATE_EXPORT QDebug operator<<(QDebug dbg, const Predicate::FieldList& list);

#endif
