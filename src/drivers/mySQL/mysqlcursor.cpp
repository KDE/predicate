/* This file is part of the KDE project
Copyright (C) 2003 Joseph Wenninger<jowenn@kde.org>

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

#include "mysqlcursor.h"
#include "mysqlconnection.h"
#include "mysqlconnection_p.h"
#include <kexidb/error.h>
#include <klocale.h>
#include <kdebug.h>

#ifdef Q_WS_WIN
# include <config-win.h>
#endif
#include <mysql.h>
#define BOOL bool

using namespace KexiDB;

MySqlCursor::MySqlCursor(KexiDB::Connection* conn, const QString& statement, uint cursor_options)
	: Cursor(conn,statement,cursor_options)
	, d( new MySqlCursorData() )
{
	m_options |= Buffered;
	d->mysql = static_cast<MySqlConnection*>(conn)->d->mysql;
	KexiDBDrvDbg << "MySqlCursor: constructor for query statement" << endl;
}

MySqlCursor::MySqlCursor(Connection* conn, QuerySchema& query, uint options )
	: Cursor( conn, query, options )
	, d( new MySqlCursorData() )
{
	m_options |= Buffered;
	d->mysql = static_cast<MySqlConnection*>(conn)->d->mysql;
	KexiDBDrvDbg << "MySqlCursor: constructor for query statement" << endl;
}

MySqlCursor::~MySqlCursor() {
	close();
}

bool MySqlCursor::drv_open(const QString& statement) {
	KexiDBDrvDbg << "MySqlCursor::drv_open:" << statement << endl;
	// This can't be right?  mysql_real_query takes a length in order that
	// queries can have binary data - but strlen does not allow binary data.
	if(mysql_real_query(d->mysql, statement.utf8(), strlen(statement.utf8())) == 0) {
		if(mysql_errno(d->mysql) == 0) {
			d->mysqlres= mysql_store_result(d->mysql);
			m_fieldCount=mysql_num_fields(d->mysqlres);
			d->numRows=mysql_num_rows(d->mysqlres);
			m_at=0;
			
			m_opened=true;
			m_records_in_buf = d->numRows;
			m_buffering_completed = true;
			m_afterLast=false;
			return true;
			}
	}
	
	setError(ERR_DB_SPECIFIC,QString::fromUtf8(mysql_error(d->mysql)));
	return false;
}

bool MySqlCursor::drv_close() {
	mysql_free_result(d->mysqlres);
	d->mysqlres=0;
	d->mysqlrow=0;
//js: done in superclass:	m_numFields=0;
	d->lengths=0;
	m_opened=false;
	d->numRows=0;
	return true;
}

/*bool MySqlCursor::drv_moveFirst() {
	return true; //TODO
}*/

void MySqlCursor::drv_getNextRecord() {
	KexiDBDrvDbg << "MySqlCursor::drv_getNextRecord" << endl;
	if (at() < d->numRows && at() >=0) {
		d->lengths=mysql_fetch_lengths(d->mysqlres);
		m_result=FetchOK;
	}
	else if (at() >= d->numRows) {
		m_result = FetchEnd;
	}
	else {
		m_result = FetchError;
	}
}

// This isn't going to work right now as it uses d->mysqlrow
QVariant MySqlCursor::value(uint pos) {
	if (!d->mysqlrow) return QVariant();
	if (pos>=m_fieldCount) return QVariant();
	if (d->mysqlrow[pos]==0) return QVariant();
	//js TODO: encode for type using m_fieldsExpanded like in SQLiteCursor::value()
	return QVariant(QString::fromUtf8((const char*)d->mysqlrow[pos]));
}


/* As with sqlite, the DB library returns all values (including numbers) as
   strings. So just put that string in a QVariant and let KexiDB deal with it.
 */
void MySqlCursor::storeCurrentRow(RowData &data) const {
	KexiDBDrvDbg << "MySqlCursor::storeCurrentRow: Position is " << (long)m_at<< endl;
	if (d->numRows<=0)
		return;

	data.reserve(m_fieldCount);
	for( uint i=0; i<m_fieldCount; i++) {
		data[i] = QVariant(d->mysqlrow[i]);
	}
}

void MySqlCursor::drv_appendCurrentRecordToBuffer() {
}


void MySqlCursor::drv_bufferMovePointerNext() {
	d->mysqlrow=mysql_fetch_row(d->mysqlres);
	d->lengths=mysql_fetch_lengths(d->mysqlres);
}

void MySqlCursor::drv_bufferMovePointerPrev() {
	MYSQL_ROW_OFFSET ro=mysql_row_tell(d->mysqlres);
	mysql_data_seek(d->mysqlres,m_at-1);
	d->mysqlrow=mysql_fetch_row(d->mysqlres);
	d->lengths=mysql_fetch_lengths(d->mysqlres);
}


void MySqlCursor::drv_bufferMovePointerTo(Q_LLONG to) {
	MYSQL_ROW_OFFSET ro=mysql_row_tell(d->mysqlres);
	mysql_data_seek(d->mysqlres, to);
	d->mysqlrow=mysql_fetch_row(d->mysqlres);
	d->lengths=mysql_fetch_lengths(d->mysqlres);
}

const char** MySqlCursor::rowData() const {
	//! @todo
	return 0;
}

int MySqlCursor::serverResult()
{
	return d->res;
}

QString MySqlCursor::serverResultName()
{
	return QString::null;
}

void MySqlCursor::drv_clearServerResult()
{
	if (!d)
		return;
	d->res = 0;
}

QString MySqlCursor::serverErrorMsg()
{
	return d->errmsg;
}
