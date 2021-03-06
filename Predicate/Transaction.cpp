/* This file is part of the KDE project
   Copyright (C) 2003 Jarosław Staniek <staniek@kde.org>

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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#include "Transaction.h"
#include "Connection.h"

#include <QtDebug>

#include <assert.h>

//remove debug
#undef PreDbg
#define PreDbg if (true); else qDebug()

using namespace Predicate;

//helper for debugging
PREDICATE_EXPORT int Transaction::globalcount = 0;
PREDICATE_EXPORT int Transaction::globalCount()
{
    return Transaction::globalcount;
}
PREDICATE_EXPORT int TransactionData::globalcount = 0;
PREDICATE_EXPORT int TransactionData::globalCount()
{
    return TransactionData::globalcount;
}

TransactionData::TransactionData(Connection *conn)
        : m_conn(conn)
        , m_active(true)
        , refcount(1)
{
    assert(conn);
    Transaction::globalcount++; //because refcount(1) init.
    TransactionData::globalcount++;
    PreDbg << "-- globalcount ==" << TransactionData::globalcount;
}

TransactionData::~TransactionData()
{
    TransactionData::globalcount--;
    PreDbg << "-- globalcount ==" << TransactionData::globalcount;
}

//---------------------------------------------------

Transaction::Transaction()
        : QObject(0)
        , m_data(0)
{
}

Transaction::Transaction(const Transaction& trans)
        : QObject(0)
        , m_data(trans.m_data)
{
    if (m_data) {
        m_data->refcount++;
        Transaction::globalcount++;
    }
}

Transaction::~Transaction()
{
    if (m_data) {
        m_data->refcount--;
        Transaction::globalcount--;
        PreDbg << "m_data->refcount==" << m_data->refcount;
        if (m_data->refcount == 0)
            delete m_data;
    } else {
        PreDbg << "null";
    }
    PreDbg << "-- globalcount == " << Transaction::globalcount;
}

Transaction& Transaction::operator=(const Transaction & trans)
{
    if (this != &trans) {
        if (m_data) {
            m_data->refcount--;
            Transaction::globalcount--;
            PreDbg << "m_data->refcount==" << m_data->refcount;
            if (m_data->refcount == 0)
                delete m_data;
        }
        m_data = trans.m_data;
        if (m_data) {
            m_data->refcount++;
            Transaction::globalcount++;
        }
    }
    return *this;
}

bool Transaction::operator==(const Transaction& trans) const
{
    return m_data == trans.m_data;
}

Connection* Transaction::connection() const
{
    return m_data ? m_data->m_conn : 0;
}

bool Transaction::active() const
{
    return m_data && m_data->m_active;
}

bool Transaction::isNull() const
{
    return m_data == 0;
}

//---------------------------------------------------

TransactionGuard::TransactionGuard(Connection *conn)
        : m_doNothing(false)
{
    Q_ASSERT(conn);
    m_trans = conn->beginTransaction();
}

TransactionGuard::TransactionGuard(const Transaction& trans)
        : m_trans(trans)
        , m_doNothing(false)
{
}

TransactionGuard::TransactionGuard()
        : m_doNothing(false)
{
}

TransactionGuard::~TransactionGuard()
{
    if (!m_doNothing && m_trans.active() && m_trans.connection())
        m_trans.connection()->rollbackTransaction(m_trans);
}

bool TransactionGuard::commit()
{
    if (m_trans.active() && m_trans.connection()) {
        return m_trans.connection()->commitTransaction(m_trans);
    }
    return false;
}

void TransactionGuard::doNothing()
{
    m_doNothing = true;
}

