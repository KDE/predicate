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

#include <kexidb/cursor.h>

#include <kexidb/driver.h>
#include <kexidb/driver_p.h>
#include <kexidb/error.h>
#include <kexidb/roweditbuffer.h>

#include <kdebug.h>
#include <klocale.h>

#include <assert.h>
#include <stdlib.h>

using namespace KexiDB;


Cursor::Cursor(Connection* conn, const QString& statement, uint options)
	: m_conn(conn)
	, m_query(0)
	, m_rawStatement(statement)
	, m_options(options)
{
	init();
#ifndef Q_WS_WIN
#warning TODO
#endif
//TODO(js) if the statement is not empty update m_fieldCount
// (change this when KexiDB::Query will be used here)
}

Cursor::Cursor(Connection* conn, QuerySchema& query, uint options )
	: m_conn(conn)
	, m_query(&query)
	, m_options(options)
{
	init();
}

void Cursor::init()
{
	assert(m_conn);
	m_conn->m_cursors.insert(this,this);
	m_opened = false;
//	, m_atFirst(false)
//	, m_atLast(false)
//	, m_beforeFirst(false)
	m_atLast = false;
	m_afterLast = false;
	m_readAhead = false;
	m_at = 0;
//js:todo:	if (m_query)
//		m_fieldCount = m_query->fieldsCount();
//	m_fieldCount = m_query ? m_query->fieldCount() : 0; //do not know
	//<members related to buffering>
//	m_cols_pointers_mem_size = 0;
	m_records_in_buf = 0;
	m_buffering_completed = false;
	m_at_buffer = false;
	m_result = -1;

	if (m_query) {
		//get list of all fields
		m_fieldsExpanded = new QueryColumnInfo::Vector();
		*m_fieldsExpanded = m_query->fieldsExpanded();//&m_detailedVisibility);
		m_fieldCount = m_fieldsExpanded->count();
	} else {
		m_fieldsExpanded = 0;
		m_fieldCount = 0;
	}
}

Cursor::~Cursor()
{
/*	if (!m_query)
		KexiDBDbg << "Cursor::~Cursor() '" << m_rawStatement.latin1() << "'" << endl;
	else
		KexiDBDbg << "Cursor::~Cursor() " << endl;*/

	//take me if delete was 
	if (!m_conn->m_destructor_started)
		m_conn->m_cursors.take(this);
	else {
		KexiDBDbg << "Cursor::~Cursor() can be destroyed with Conenction::deleteCursor(), not with delete operator !"<< endl;
		exit(1);
	}
	delete m_fieldsExpanded;
}

bool Cursor::open()
{
	if (m_opened) {
		if (!close())
			return false;
	}
	if (!m_rawStatement.isEmpty())
		m_conn->m_sql = m_rawStatement;
	else {
		if (!m_query) {
			KexiDBDbg << "Cursor::open(): no query statement (or schema) defined!" << endl;
			setError(ERR_SQL_EXECUTION_ERROR, i18n("No query statement or schema defined."));
			return false;
		}
		m_conn->m_sql = m_conn->selectStatement( *m_query );
		if (m_conn->m_sql.isEmpty()) {
			KexiDBDbg << "Cursor::open(): empty statement!" << endl;
			setError(ERR_SQL_EXECUTION_ERROR, i18n("Query statement is empty."));
			return false;
		}
	}
	m_opened = drv_open(m_conn->m_sql);
//	m_beforeFirst = true;
	m_afterLast = false; //we are not @ the end
	m_at = 0; //we are before 1st rec
	if (!m_opened) {
		setError(ERR_SQL_EXECUTION_ERROR, i18n("Error opening database cursor."));
		return false;
	}
	m_validRecord = false;

//luci:	WHAT_EXACTLY_SHOULD_THAT_BE?
//	if (!m_readAhead) // jowenn: to ensure before first state, without cluttering implementation code
	if (m_conn->driver()->beh->_1ST_ROW_READ_AHEAD_REQUIRED_TO_KNOW_IF_THE_RESULT_IS_EMPTY) {
//		KexiDBDbg << "READ AHEAD:" << endl;
		m_readAhead = getNextRecord(); //true if any record in this query
//		KexiDBDbg << "READ AHEAD = " << m_readAhead << endl;
	}
	m_at = 0; //we are still before 1st rec
	return !error();
}

bool Cursor::close()
{
	if (!m_opened)
		return true;
	bool ret = drv_close();

	clearBuffer();

	m_opened = false;
//	m_beforeFirst = false;
	m_afterLast = false;
	m_readAhead = false;
	m_fieldCount = 0;
	m_at = -1;

//	KexiDBDbg<<"Cursor::close() == "<<ret<<endl;
	return ret;
}

bool Cursor::reopen()
{
	if (!m_opened)
		return open();
	return close() && open();
}

bool Cursor::moveFirst()
{
	if (!m_opened)
		return false;
//	if (!m_beforeFirst) { //cursor isn't @ first record now: reopen
	if (!m_readAhead) {
		if (m_options & Buffered) {
			if (m_records_in_buf==0 && m_buffering_completed) {
				//eof and bof should now return true:
				m_afterLast = true;
				m_at = 0;
				return false; //buffering completed and there is no records!
			}
			if (m_records_in_buf>0) {
				//set state as we would be before first rec:
				m_at_buffer = false;
				m_at = 0;
				//..and move to next, ie. 1st record
//				m_afterLast = m_afterLast = !getNextRecord();
				m_afterLast = !getNextRecord();
				return !m_afterLast;
			}
		}
		if (m_afterLast && m_at==0) //failure if already no records
			return false;
		if (!reopen()) //try reopen
			return false;
		if (m_afterLast) //eof
			return false;
	}
	else {
		//we have a record already read-ahead: we now point @ that:
		m_at = 1;
	}
//	if (!m_atFirst) { //cursor isn't @ first record now: reopen
//		reopen();
//	}
//	if (m_validRecord) {
//		return true; //there is already valid record retrieved
//	}
	//get first record
//	if (drv_moveFirst() && drv_getRecord()) {
//		m_beforeFirst = false;
		m_afterLast = false;
		m_readAhead = false; //1st record had been read
//	}
	return m_validRecord;
}

bool Cursor::moveLast()
{
	if (!m_opened)
		return false;
	if (m_afterLast || m_atLast) {
		return m_validRecord; //we already have valid last record retrieved
	}
	if (!getNextRecord()) { //at least next record must be retrieved
//		m_beforeFirst = false;
		m_afterLast = true;
		m_validRecord = false;
		m_atLast = false;
		return false; //no records
	}
	while (getNextRecord()) //move after last rec.
		;
//	m_beforeFirst = false;
	m_afterLast = false;
	//cursor shows last record data
	m_atLast = true; 
//	m_validRecord = true;

/*
	//we are before or @ last record:
//	if (m_atLast && m_validRecord) //we're already @ last rec.
//		return true;
	if (m_validRecord) {
		if (drv_getRecord())
	}
	if (!m_validRecord) {
		if (drv_getRecord() && m_validRecord)
			return true;
		reopen();
	}
	*/
	return true;
}

bool Cursor::moveNext()
{
	if (!m_opened || m_afterLast)
		return false;
	if (getNextRecord()) {
//		m_validRecord = true;
		return true;
	}
	return false;
}

bool Cursor::movePrev()
{
	if (!m_opened /*|| m_beforeFirst*/ || !(m_options & Buffered))
		return false;

	//we're after last record and there are records in the buffer
	//--let's move to last record
	if (m_afterLast && (m_records_in_buf>0)) {
		drv_bufferMovePointerTo(m_records_in_buf-1);
		m_at=m_records_in_buf;
		m_at_buffer = true; //now current record is stored in the buffer
		m_validRecord=true;
		m_afterLast=false;
		return true;
	}
	//we're at first record: go BOF
	if ((m_at <= 1) || (m_records_in_buf <= 1/*sanity*/)) {
		m_at=0;
		m_at_buffer = false;
		m_validRecord=false;
		return false;
	}

	m_at--;
	if (m_at_buffer) {//we already have got a pointer to buffer
		drv_bufferMovePointerPrev(); //just move to prev record in the buffer
	} else {//we have no pointer
		//compute a place in the buffer that contain next record's data
		drv_bufferMovePointerTo(m_at-1);
		m_at_buffer = true; //now current record is stored in the buffer
	}
	m_validRecord=true;
	m_afterLast=false;
	return true;
}

bool Cursor::eof() const
{
	return m_afterLast;
}

bool Cursor::bof() const
{
	return m_at==0;
}

Q_LLONG Cursor::at() const
{
	if (m_readAhead)
		return 0;
	return m_at - 1;
}

bool Cursor::isBuffered() const
{
	return m_options & Buffered;
}

void Cursor::setBuffered(bool buffered)
{
	if (!m_opened)
		return;
	if (isBuffered()==buffered)
		return;
	m_options ^= Buffered;
}

void Cursor::clearBuffer()
{
	if ( !isBuffered() || !m_fieldCount)
		return;
	
	drv_clearBuffer();
	
	m_records_in_buf=0;
	m_at_buffer=false;
}

bool Cursor::getNextRecord()
{
	m_result = -1; //by default: invalid result of row fetching

	if ((m_options & Buffered)) {//this cursor is buffered:
//		KexiDBDbg << "m_at < m_records_in_buf :: " << (long)m_at << " < " << m_records_in_buf << endl;
//js		if (m_at==-1) m_at=0;
		if (m_at < m_records_in_buf) {//we have next record already buffered:
///		if (m_at < (m_records_in_buf-1)) {//we have next record already buffered:
//js			if (m_at_buffer && (m_at!=0)) {//we already have got a pointer to buffer
			if (m_at_buffer) {//we already have got a pointer to buffer
				drv_bufferMovePointerNext(); //just move to next record in the buffer
			} else {//we have no pointer
				//compute a place in the buffer that contain next record's data
				drv_bufferMovePointerTo(m_at-1+1);
//				drv_bufferMovePointerTo(m_at+1);
				m_at_buffer = true; //now current record is stored in the buffer
			}
		}
		else {//we are after last retrieved record: we need to physically fetch next record:
			if (!m_readAhead) {//we have no record that was read ahead
				if (!m_buffering_completed) {
					//retrieve record only if we are not after 
					//the last buffer's item (i.e. when buffer is not fully filled):
//					KexiDBDrvDbg<<"==== buffering: drv_getNextRecord() ===="<<endl;
					drv_getNextRecord();
				}
				if (m_result != FetchOK) {//there is no record
					m_buffering_completed = true; //no more records for buffer
//					KexiDBDrvDbg<<"m_result != FetchOK ********"<<endl;
					m_validRecord = false;
					m_afterLast = true;
//js					m_at = m_records_in_buf;
					m_at = -1; //position is invalid now and will not be used
					if (m_result == FetchEnd) {
						return false;
					}
					setError(ERR_CURSOR_RECORD_FETCHING, i18n("Cannot fetch next record."));
					return false;
				}
				//we have a record: store this record's values in the buffer
				drv_appendCurrentRecordToBuffer();
				m_records_in_buf++;
			}
			else //we have a record that was read ahead: eat this
				m_readAhead = false;
		}
	}
	else {//we are after last retrieved record: we need to physically fetch next record:
		if (!m_readAhead) {//we have no record that was read ahead
//			KexiDBDrvDbg<<"==== no prefetched record ===="<<endl;
			drv_getNextRecord();
			if (m_result != FetchOK) {//there is no record
//				KexiDBDrvDbg<<"m_result != FetchOK ********"<<endl;
				m_validRecord = false;
				m_afterLast = true;
				m_at = -1;
				if (m_result == FetchEnd) {
					return false;
				}
				setError(ERR_CURSOR_RECORD_FETCHING, i18n("Cannot fetch next record."));
				return false;
			}
		}
		else //we have a record that was read ahead: eat this
			m_readAhead = false;
	}

	m_at++;
	
//	if (m_data->curr_colname && m_data->curr_coldata)
//		for (int i=0;i<m_data->curr_cols;i++) {
//			KexiDBDrvDbg<<i<<": "<< m_data->curr_colname[i]<<" == "<< m_data->curr_coldata[i]<<endl;
//		}
//	KexiDBDrvDbg<<"m_at == "<<(long)m_at<<endl;

	m_validRecord = true;
	return true;
}

bool Cursor::updateRow(RowData& data, RowEditBuffer& buf)
{
//TODO: doesn't update cursor's buffer YET!
	clearError();
	if (!m_query)
		return false;
	return m_conn->updateRow(*m_query, data, buf);
}

bool Cursor::insertRow(RowData& data, RowEditBuffer& buf)
{
//TODO: doesn't update cursor's buffer YET!
	clearError();
	if (!m_query)
		return false;
	return m_conn->insertRow(*m_query, data, buf);
}

bool Cursor::deleteRow(RowData& data)
{
//TODO: doesn't update cursor's buffer YET!
	clearError();
	if (!m_query)
		return false;
	return m_conn->deleteRow(*m_query, data);
}

QString Cursor::debugString() const
{
	QString dbg = "CURSOR( ";
	if (!m_query) {
		dbg += "RAW STATEMENT: '";
		dbg += m_rawStatement;
		dbg += "'\n";
	}
	else {
		dbg += "QuerySchema: '";
		dbg += m_conn->selectStatement( *m_query );
		dbg += "'\n";
	}
	if (isOpened())
		dbg += " OPENED";
	else
		dbg += " NOT_OPENED";
	if (isBuffered())
		dbg += " BUFFERED";
	else
		dbg += " NOT_BUFFERED";
	dbg += " AT=";
	dbg += QString::number((unsigned long)at());
	dbg += " )";
	return dbg;
}

void Cursor::debug() const
{
	KexiDBDbg << debugString() << endl;
}

