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

#ifndef KEXIDB_INDEX_H
#define KEXIDB_INDEX_H

#include <qvaluelist.h>
#include <qstring.h>

#include <kexidb/fieldlist.h>
#include <kexidb/schemadata.h>

namespace KexiDB {

class Connection;
class TableSchema;
class QuerySchema;

/*! 
*/

class KEXI_DB_EXPORT IndexSchema : public FieldList, public SchemaData
{
	public:
		typedef QPtrList<IndexSchema> List;

		/*! Constructs empty index schema object 
		 that is assigned to \a table, and will be owned by this table.
		 Any fields added with addField() won't be owned by index,
		 but by its table. Do not forget to add this fields to table,
		 because adding these to IndexSchema is not enough. */
		IndexSchema(TableSchema *tableSchema);

//		/*! Constructs empty index schema object 
//		 that is assigned to \a table.
//		 Any fields added with addField() won't be owned by index.  */
//		IndexSchema(TableSchema *table);
		
		/*! Destroys the index. Field objects are not deleted. */
		~IndexSchema();

//		void setName(const QString& name);

		/*! Adds field at the and of field list. 
		 Field will not be owned by index. Field must belong to a table
		 the index is bulit on, otherwise field couldn't be added. */
		virtual FieldList& addField(Field *field);

		/*! \return table on that index is built */
		TableSchema* table() const;

		/*! \return true if index is auto-generated.
			Auto-generated index is one-filed index
			that was generated 
			for CREATE TABLE statement when the field has UNIQUE 
			constraint enabled.
		*/
		bool isAutoGenerated() const;
		
		/*! Sets auto-generated flag. \sa isAutoGenerated(). */
		void setAutoGenerated(bool set);

		/*! \return true if this is index is primary key. 
			This can be one or multifield. */
		bool isPrimary() const;
		
		/*! Sets primary flag. \sa isPrimary(). */
		void setPrimary(bool set);

		/*! Shows debug information about index. */
		virtual void debug() const;
	protected:

	//js	QStringList m_primaryKeys;
//		Connection *m_conn;
		TableSchema *m_tableSchema; //! table on that index is built
		
		bool m_primary : 1;
		bool m_isAutoGenerated : 1;

	friend class Connection;
	friend class TableSchema;
	friend class QuerySchema;
};

} //namespace KexiDB

#endif
