/* This file is part of the KDE project
   Copyright (C) 2003 Jaroslaw Staniek <js@iidea.pl>

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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef KEXIDB_SQLITECURSOR_H
#define KEXIDB_SQLITECURSOR_H

#include <qstring.h>

#include <kexidb/cursor.h>
#include "connection.h"

namespace KexiDB {

class SQLiteCursorData;

/*! 

*/
class KEXIDB_SQLITE_DRIVER_EXPORT SQLiteCursor : public Cursor
{
	public:
		virtual ~SQLiteCursor();
		/*! Opens the cursor using \a statement */
//		bool open( const QString& statement = QString::null );
		/*! Closes previously opened cursor. 
			If the cursor is closed, nothing happens. */
//		virtual bool close();
		/*! Moves current position to the first record and retrieves it. */
//		virtual bool moveFirst();
		/*! Moves current position to the last record and retrieves it. */
//		virtual bool moveLast();
		/*! Moves current position to the next record and retrieves it. */
//		virtual bool moveNext();
		/*! \return true if current position is after last record. */
//		bool eof();
		/*! \return current internal position of the query. */
//		int at();
		virtual QVariant value(uint i);

		/*! [PROTOTYPE] \return internal buffer data. */
//TODO		virtual const char *** bufferData()
		/*! [PROTOTYPE] \return current record data or NULL if there is no current records. */
		virtual const char ** rowData() const;

		virtual void storeCurrentRow(RowData &data) const;

//		virtual bool save(RowData& data, RowEditBuffer& buf);

		virtual int serverResult() const;
		
		virtual QString serverResultName() const;

		virtual QString serverErrorMsg() const;

	protected:
		/*! Cursor will operate on \a conn, raw \a statement will be used to execute query. */
		SQLiteCursor(Connection* conn, const QString& statement, uint options = NoOptions );

		/*! Cursor will operate on \a conn, \a query schema will be used to execute query. */
		SQLiteCursor(Connection* conn, QuerySchema& query, uint options = NoOptions );

		virtual bool drv_open(const QString& statement);

		virtual bool drv_close();
//		virtual bool drv_moveFirst();
		virtual void drv_getNextRecord();
//unused		virtual bool drv_getPrevRecord();

		virtual void drv_appendCurrentRecordToBuffer();
		virtual void drv_bufferMovePointerNext();
		virtual void drv_bufferMovePointerPrev();
		virtual void drv_bufferMovePointerTo(Q_LLONG at);

//TODO		virtual void drv_storeCurrentRecord();

		//PROTOTYPE:
		/*! Method called when cursor's buffer need to be cleared
			(only for buffered cursor type), eg. in close(). */
		virtual void drv_clearBuffer();
		
		virtual void drv_clearServerResult();

		SQLiteCursorData *d;

	friend class SQLiteConnection;
};

}; //namespace KexiDB

#endif


