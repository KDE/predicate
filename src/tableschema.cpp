/* This file is part of the KDE project
   Copyright (C) 2003 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2003-2004 Jaroslaw Staniek <js@iidea.pl>

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

#include <kexidb/tableschema.h>
#include <kexidb/driver.h>
#include <kexidb/connection.h>

#include <assert.h>

#include <kdebug.h>

namespace KexiDB {
//! @internal
class TableSchema::Private
{
public:
	Private()
	 : anyNonPKField(0)
	{
	}

	Field *anyNonPKField;
};
}
//-------------------------------------


using namespace KexiDB;

TableSchema::TableSchema(const QString& name)
	: FieldList(true)
	, SchemaData(KexiDB::TableObjectType)
	, m_conn(0)
	, m_query(0)
	, m_isKexiDBSystem(false)
{
	m_name = name.lower();
	init();
}

TableSchema::TableSchema(const SchemaData& sdata)
	: FieldList(true)
	, SchemaData(sdata)
	, m_conn(0)
	, m_query(0)
	, m_isKexiDBSystem(false)
{
	init();
}

TableSchema::TableSchema()
	: FieldList(true)
	, SchemaData(KexiDB::TableObjectType)
	, m_conn(0)
	, m_query(0)
	, m_isKexiDBSystem(false)
{
	init();
}

TableSchema::TableSchema(const TableSchema& ts)
	: FieldList(static_cast<const FieldList&>(ts))
	, SchemaData(static_cast<const SchemaData&>(ts))
	, m_conn( ts.m_conn )
	, m_query(0) //not cached
	, m_isKexiDBSystem(false)
{
	d = new Private();
	m_name = ts.m_name;
	m_indices.setAutoDelete( true );
	m_pkey = 0; //will be copied

	//deep copy all members
	IndexSchema::ListIterator idx_it(ts.m_indices);
	for (;idx_it.current();++idx_it) {
		IndexSchema *idx = new IndexSchema(*idx_it.current());
		idx->m_tableSchema = this;
		if (idx->isPrimaryKey()) {//assign pkey
			m_pkey = idx;
		}
		m_indices.append(idx);
	}
}

// used by Connection
TableSchema::TableSchema(Connection *conn, const QString & name)
	: FieldList(true)
	, SchemaData(KexiDB::TableObjectType)
	, m_conn( conn )
	, m_query(0)
	, m_isKexiDBSystem(false)
{
	d = new Private();
	assert(conn);
	m_name = name;
	m_indices.setAutoDelete( true );
	m_pkey = new IndexSchema(this);
	m_indices.append(m_pkey);
}

TableSchema::~TableSchema()
{
	if (m_conn)
		m_conn->removeMe( this );
	delete m_query;
	delete d;
}

void TableSchema::init()
{
	d = new Private();
	m_indices.setAutoDelete( true );
	m_pkey = new IndexSchema(this);
	m_indices.append(m_pkey);
}

void TableSchema::setPrimaryKey(IndexSchema *pkey)
{
	if (m_pkey && m_pkey!=pkey) {
		if (m_pkey->fieldCount()==0) {//this is empty key, probably default - remove it
			m_indices.remove(m_pkey);
		}
		else {
			m_pkey->setPrimaryKey(false); //there can be only one pkey..
			//thats ok, the old pkey is still on indices list, if not empty
		}
//		m_pkey=0; 
	}
	
	if (!pkey) {//clearing - set empty pkey
		pkey = new IndexSchema(this);
	}
	m_pkey = pkey; //todo
	m_pkey->setPrimaryKey(true);
	d->anyNonPKField = 0; //for safety
}

FieldList& TableSchema::insertField(uint index, Field *field)
{
	assert(field);
	FieldList::insertField(index, field);
	if (!field || index>m_fields.count())
		return *this;
	field->setTable(this);
	field->m_order = index; //m_fields.count();
	//update order for next next fields
	Field *f = m_fields.at(index+1);
	for (int i=index+1; f; i++, f = m_fields.next())
		f->m_order = i;

	//Check for auto-generated indices:
	IndexSchema *idx = 0;
	if (field->isPrimaryKey()) {// this is auto-generated single-field unique index
		idx = new IndexSchema(this);
		idx->setAutoGenerated(true);
		idx->addField( field );
		setPrimaryKey(idx);
	}
	if (field->isUniqueKey()) {
		if (!idx) {
			idx = new IndexSchema(this);
			idx->setAutoGenerated(true);
			idx->addField( field );
		}
		idx->setUnique(true);
	}
	if (field->isIndexed()) {// this is auto-generated single-field
		if (!idx) {
			idx = new IndexSchema(this);
			idx->setAutoGenerated(true);
			idx->addField( field );
		}
	}
	if (idx)
		m_indices.append(idx);
	return *this;
}

void TableSchema::removeField(KexiDB::Field *field)
{
	if (d->anyNonPKField && field == d->anyNonPKField) //d->anyNonPKField will be removed!
		d->anyNonPKField = 0;
	FieldList::removeField(field);
}

#if 0 //original		
KexiDB::FieldList& TableSchema::addField(KexiDB::Field* field)
{
	assert(field);
	FieldList::addField(field);
	field->setTable(this);
	field->m_order = m_fields.count();
	//Check for auto-generated indices:

	IndexSchema *idx = 0;
	if (field->isPrimaryKey()) {// this is auto-generated single-field unique index
		idx = new IndexSchema(this);
		idx->setAutoGenerated(true);
		idx->addField( field );
		setPrimaryKey(idx);
	}
	if (field->isUniqueKey()) {
		if (!idx) {
			idx = new IndexSchema(this);
			idx->setAutoGenerated(true);
			idx->addField( field );
		}
		idx->setUnique(true);
	}
	if (field->isIndexed()) {// this is auto-generated single-field
		if (!idx) {
			idx = new IndexSchema(this);
			idx->setAutoGenerated(true);
			idx->addField( field );
		}
	}
	if (idx)
		m_indices.append(idx);
	return *this;
}
#endif

void TableSchema::clear()
{
	m_indices.clear();
	FieldList::clear();
	SchemaData::clear();
	m_conn = 0;
}

/*
void TableSchema::addPrimaryKey(const QString& key)
{
	m_primaryKeys.append(key);
}*/

/*QStringList TableSchema::primaryKeys() const
{
	return m_primaryKeys;
}

bool TableSchema::hasPrimaryKeys() const
{
	return !m_primaryKeys.isEmpty();
}
*/

//const QString& TableSchema::name() const
//{
//	return m_name;
//}

//void TableSchema::setName(const QString& name)
//{
//	m_name=name;
/*	ListIterator it( m_fields );
	Field *field;
	for (; (field = it.current())!=0; ++it) {
	
	int fcnt=m_fields.count();
	for (int i=0;i<fcnt;i++) {
		m_fields[i].setTable(name);
	}*/
//}

/*KexiDB::Field TableSchema::field(unsigned int id) const
{
	if (id<m_fields.count()) return m_fields[id];
	return KexiDB::Field();
}

unsigned int TableSchema::fieldCount() const
{
	return m_fields.count();
}*/

QString TableSchema::debugString()
{
	return QString("TABLE ") + schemaDataDebugString() + "\n" + FieldList::debugString();
}

void TableSchema::setKexiDBSystem(bool set)
{
	if (set)
		m_native=true;
	m_isKexiDBSystem = set;
}

void TableSchema::setNative(bool set)
{
	if (m_isKexiDBSystem && !set) {
		KexiDBDbg << "TableSchema::setNative(): cannot set native off"
			" when KexiDB system flag is set on!" << endl;
		return;
	}
	m_native=set;
}

QuerySchema* TableSchema::query()
{
	if (m_query)
		return m_query;
	m_query = new QuerySchema( this ); //it's owned by me
	return m_query;
}

Field* TableSchema::anyNonPKField()
{
	if (!d->anyNonPKField) {
		Field *f;
		Field::ListIterator it(m_fields);
		it.toLast(); //from the end (higher chances to find)
		for (; (f = it.current()); --it) {
			if (!f->isPrimaryKey() && (!m_pkey || !m_pkey->hasField(f)))
				break;
		}
		d->anyNonPKField = f;
	}
	return d->anyNonPKField;
}

//--------------------------------------

InternalTableSchema::InternalTableSchema(const QString& name)
 : TableSchema(name)
{
}

InternalTableSchema::~InternalTableSchema()
{
}

