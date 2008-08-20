/* This file is part of the KDE project
   Copyright (C) 2003-2007 Jarosław Staniek <staniek@kde.org>

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

#include "indexschema.h"
#include "driver.h"
#include "connection.h"
#include "tableschema.h"

#include <assert.h>

#include <kdebug.h>

using namespace KexiDB;

IndexSchema::IndexSchema(TableSchema *tableSchema)
        : FieldList(false)//fields are not owned by IndexSchema object
        , SchemaData(KexiDB::IndexObjectType)
        , m_tableSchema(tableSchema)
        , m_primary(false)
        , m_unique(false)
        , m_isAutoGenerated(false)
        , m_isForeignKey(false)
{
//Qt 4 m_master_owned_rels.setAutoDelete(true); //rels at the master-side are owned
}

IndexSchema::IndexSchema(const IndexSchema& idx, TableSchema& parentTable)
// : FieldList(static_cast<const FieldList&>(idx))//fields are not owned by IndexSchema object
        : FieldList(false)//fields are not owned by IndexSchema object
        , SchemaData(static_cast<const SchemaData&>(idx))
        , m_tableSchema(&parentTable)
        , m_primary(idx.m_primary)
        , m_unique(idx.m_unique)
        , m_isAutoGenerated(idx.m_isAutoGenerated)
        , m_isForeignKey(idx.m_isForeignKey)
{
//Qt 4 m_master_owned_rels.setAutoDelete(true); //rels at the master-side are owned

    //deep copy of the fields
    foreach(Field *f, idx.m_fields) {
        Field *parentTableField = parentTable.field(f->name());
        if (!parentTableField) {
            KexiDBWarn << "IndexSchema::IndexSchema(const IndexSchema& idx, const TableSchema& parentTable): "
            "cannot find field '" << f->name() << " in parentTable. Empty index will be created!" << endl;
            FieldList::clear();
            break;
        }
        addField(parentTableField);
    }

//js TODO: copy relationships!
// Reference::List m_refs_to; //! list of references to table (of this index)
// Reference::List m_refs_from; //! list of references from the table (of this index),
//         //! this index is foreign key for these references
//         //! and therefore - owner of these
}

IndexSchema::~IndexSchema()
{
    /* It's a list of relationships to the table (of this index), i.e. any such relationship in which
     the table is at 'master' side will be cleared and relationships will be destroyed.
     So, we need to detach all these relationships from details-side, corresponding indices.
    */
    foreach(Relationship* rel, m_master_owned_rels) {
        if (rel->detailsIndex()) {
            rel->detailsIndex()->detachRelationship(rel);
        }
    }
    qDeleteAll(m_master_owned_rels);
}

FieldList& IndexSchema::addField(Field *field)
{
    if (field->table() != m_tableSchema) {
        KexiDBDbg << "IndexSchema::addField(" << (field ? field->name() : 0)
        << "): WARNING: field doas not belong to the same table '"
        << (field && field->table() ? field->table()->name() : 0)
        << "'as index!" << endl;
        return *this;
    }
    return FieldList::addField(field);
}


KexiDB::TableSchema* IndexSchema::table() const
{
    return m_tableSchema;
}

bool IndexSchema::isAutoGenerated() const
{
    return m_isAutoGenerated;
}

void IndexSchema::setAutoGenerated(bool set)
{
    m_isAutoGenerated = set;
}

bool IndexSchema::isPrimaryKey() const
{
    return m_primary;
}

void IndexSchema::setPrimaryKey(bool set)
{
    m_primary = set;
    if (m_primary)
        m_unique = true;
}

bool IndexSchema::isUnique() const
{
    return m_unique;
}

void IndexSchema::setUnique(bool set)
{
    m_unique = set;
    if (!m_unique)
        m_primary = false;
}

void IndexSchema::setForeignKey(bool set)
{
    m_isForeignKey = set;
    if (m_isForeignKey) {
        setUnique(false);
    }
    if (fieldCount() == 1) {
        m_fields.first()->setForeignKey(true);
    }
}

QString IndexSchema::debugString()
{
    return QString("INDEX ") + schemaDataDebugString() + "\n"
           + (m_isForeignKey ? "FOREIGN KEY " : "")
           + (m_isAutoGenerated ? "AUTOGENERATED " : "")
           + (m_primary ? "PRIMARY " : "")
           + ((!m_primary) && m_unique ? "UNIQUE " : "")
           + FieldList::debugString();
}

void IndexSchema::attachRelationship(Relationship *rel)
{
    attachRelationship(rel, true);
}

void IndexSchema::attachRelationship(Relationship *rel, bool ownedByMaster)
{
    if (!rel)
        return;
    if (rel->masterIndex() == this) {
        if (ownedByMaster) {
            if (!m_master_owned_rels.contains(rel)) {
                m_master_owned_rels.insert(rel);
            }
        } else {//not owned
            if (!m_master_rels.contains(rel)) {
                m_master_rels.append(rel);
            }
        }
    } else if (rel->detailsIndex() == this) {
        if (!m_details_rels.contains(rel)) {
            m_details_rels.append(rel);
        }
    }
}

void IndexSchema::detachRelationship(Relationship *rel)
{
    if (!rel)
        return;
    m_master_owned_rels.remove(rel);   //for sanity
    m_master_rels.takeAt(m_master_rels.indexOf(rel));   //for sanity
    m_details_rels.takeAt(m_details_rels.indexOf(rel));   //for sanity
}
