/* This file is part of the KDE project
   Copyright (C) 2004 Jaroslaw Staniek <js@iidea.pl>

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

#ifndef KEXIDB_MYSQLCONN_P_H
#define KEXIDB_MYSQLCONN_P_H

typedef struct st_mysql MYSQL;

namespace KexiDB
{

/*! Internal MySQL connection data. Also used inside MySqlCursor. */
class MySqlConnectionInternal
{
	public:
		MySqlConnectionInternal();
		~MySqlConnectionInternal();

		//! stores last result's message
		void storeResult();

		MYSQL *mysql;
		QString errmsg; //<! server-specific message of last operation
		int res; //<! result code of last operation on server

		QCString temp_st;
};

}

#endif