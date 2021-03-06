/* This file is part of the KDE project
   Copyright (C) 2005 Jarosław Staniek <staniek@kde.org>

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

#ifndef PREDICATE_CONNECTION_P_H
#define PREDICATE_CONNECTION_P_H

#include <Predicate/Connection.h>

namespace Predicate
{

//! Interface for connection's internals, implemented within drivers
class PREDICATE_EXPORT ConnectionInternal
{
public:
    explicit ConnectionInternal(Connection *conn);
    virtual ~ConnectionInternal();
    virtual void storeResult() = 0;

    inline void setServerMessage(const QString& serverMessage) {
        connection->setServerMessage(serverMessage);
    }
    inline void setServerResultCode(int code) {
        connection->setServerResultCode(code);
    }
    inline void setResult(const Result& result) {
        connection->setResult(result);
    }

    Connection* const connection;
};

} //namespace Predicate

#endif
