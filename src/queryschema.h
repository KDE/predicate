/* This file is part of the KDE project
   Copyright (C) 2003 Jaroslaw Staniek <js@iidea.pl>

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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef KEXIDB_QUERY_H
#define KEXIDB_QUERY_H

#include <qvaluelist.h>
#include <qstring.h>

#include <kexidb/fieldlist.h>
#include <kexidb/schemadata.h>

namespace KexiDB {

class Connection;

/*! KexiDB::QuerySchema provides information about database query
	that can be executed using SQL database engine. 
	
*/

class KEXI_DB_EXPORT QuerySchema : public FieldList, public SchemaData
{
	public:
		/*! Creates empty query object (without fields). */
		QuerySchema();
		/*! Creates query schema object that is equivalent to "SELECT * FROM table" 
		 sql command. Schema of \a table is used to contruct this query.
		 If \a name is omitted, query will inherit its name from 
		 \a table name.

		 We consider that query schema based on \a table is not (yet) stored 
		 in system table, so query connection is set to NULL 
		 (even if \a tables connection is not NULL), and query id is set to 0. */
		QuerySchema(TableSchema* tableSchema);
		
		virtual ~QuerySchema();
		
//		QStringList primaryKeys() const;
//		bool hasPrimaryKeys() const;
		virtual KexiDB::FieldList& addField(KexiDB::Field* field);

//		int id() { return m_id; }
//		Field::List::iterator fields() { return m_fields.begin(); }
//js		void addPrimaryKey(const QString& key);

		/*! Removes all fields from the list, clears name and all other properties. 
			\sa FieldList::clear() */
		virtual void clear();

		//! Outputs debug information.
		virtual void debug() const;

		/*! If query was created using a connection, 
			returns this connection object, otherwise NULL. */
		Connection* connection();
		
		/*! Table that is parent to this query. Only potentially-editable fields 
		 in this query belong to this table. */
		TableSchema* parentTable() const;
	
	protected:
//		/*! Automatically retrieves query schema via connection. */
//		QuerySchema(Connection *conn);

	//js	QStringList m_primaryKeys;
//		Index::List m_indices;
//		QString m_name;

//		int m_id; //! unique identifier used in kexi__objects for this query

//		/*! Connection that was used to retrieve this query schema (may be NULL). */
//js: conn from m_parent_table will be reused		Connection *m_conn; 
		/*! Parent table of the query. (may be NULL)
			Any data modifications can be performed if we know parent table.
			If null, query's records cannot be modified. */
		TableSchema *m_parent_table;

	friend class Connection;
};

} //namespace KexiDB

#endif