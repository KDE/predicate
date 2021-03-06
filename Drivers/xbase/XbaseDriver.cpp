/* This file is part of the KDE project
   Copyright (C) 2008 Sharan Rao <sharanrao@gmail.com>

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

#include "XbaseDriver.h"

#include <Predicate/Connection>
#include <Predicate/DriverManager>
#include <Predicate/Private/Driver>
#include <Predicate/Utils>

#include <QtDebug>

#include "XbaseConnection.h"

using namespace Predicate;

EXPORT_PREDICATE_DRIVER(xBaseDriver, xbase)

class Predicate::xBaseDriverPrivate {

public:
  xBaseDriverPrivate()
    : internalDriver(0)
  {
  }

  Predicate::Driver* internalDriver;

};

xBaseDriver::xBaseDriver()
  : Driver()
  ,dp( new xBaseDriverPrivate() )
{
  Predicate::DriverManager manager;
  dp->internalDriver = manager.driver(Predicate::defaultFileBasedDriverName());

//  d->isFileDriver = true ;
  d->isDBOpenedAfterCreate = true;
  d->features = SingleTransactions | CursorForward;

  // Everything below is for the SQLite (default file based) driver

  //special method for autoincrement definition
  beh->SPECIAL_AUTO_INCREMENT_DEF = true;
  beh->AUTO_INCREMENT_FIELD_OPTION = ""; //not available
  beh->AUTO_INCREMENT_TYPE = "INTEGER";
  beh->AUTO_INCREMENT_PK_FIELD_OPTION = "PRIMARY KEY";
  beh->AUTO_INCREMENT_REQUIRES_PK = true;
  beh->ROW_ID_FIELD_NAME = "OID";

  beh->QUOTATION_MARKS_FOR_IDENTIFIER='"';
  beh->SELECT_1_SUBQUERY_SUPPORTED = true;

  // As we provide a wrapper over SQLite, this aspect will be hidden by SQLite to us.
  beh->_1ST_ROW_READ_AHEAD_REQUIRED_TO_KNOW_IF_THE_RESULT_IS_EMPTY=false;

  initDriverSpecificKeywords(keywords);

  // Ditto like SQLite , as it won't matter
  d->typeNames[Field::Byte]="Byte";
  d->typeNames[Field::ShortInteger]="ShortInteger";
  d->typeNames[Field::Integer]="Integer";
  d->typeNames[Field::BigInteger]="BigInteger";
  d->typeNames[Field::Boolean]="Boolean";
  d->typeNames[Field::Date]="Date";
  d->typeNames[Field::DateTime]="DateTime";
  d->typeNames[Field::Time]="Time";
  d->typeNames[Field::Float]="Float";
  d->typeNames[Field::Double]="Double";
  d->typeNames[Field::Text]="Text";
  d->typeNames[Field::LongText]="CLOB";
  d->typeNames[Field::BLOB]="BLOB";
}

xBaseDriver::~xBaseDriver()
{
  delete dp;
}

Predicate::Connection* xBaseDriver::drv_createConnection(const ConnectionData& connData)
{
    if ( !dp->internalDriver ) {
        return 0;
    }
    return new xBaseConnection( this, dp->internalDriver, connData );
}

bool xBaseDriver::isSystemObjectName( const QString& n ) const
{
  if ( !dp->internalDriver ) {
    return false;
  }
  return Driver::isSystemObjectName(n) || dp->internalDriver->isSystemObjectName(n);
}

bool xBaseDriver::drv_isSystemFieldName( const QString& n ) const
{
  if ( !dp->internalDriver ) {
    return false;
  }
  return dp->internalDriver->isSystemFieldName(n);
}

EscapedString xBaseDriver::escapeString(const QString& str) const
{
  if ( !dp->internalDriver ) {
    return EscapedString("'") + str + '\'';
  }
  return dp->internalDriver->escapeString(str);
}

EscapedString xBaseDriver::escapeString(const QByteArray& str) const
{
  if ( !dp->internalDriver ) {
    return EscapedString("'") + str + '\'';
  }
  return dp->internalDriver->escapeString(str);
}

EscapedString xBaseDriver::escapeBLOB(const QByteArray& array) const
{
  if ( !dp->internalDriver ) {
    return array;
  }
  return dp->internalDriver->escapeBLOB(array);
}

QByteArray xBaseDriver::drv_escapeIdentifier( const QString& str) const
{
  if ( !dp->internalDriver ) {
    return str;
  }
  return dp->internalDriver->escapeIdentifier(str);
}

QByteArray xBaseDriver::drv_escapeIdentifier( const QByteArray& str) const
{
  if ( !dp->internalDriver ) {
    return str;
  }
  return dp->internalDriver->escapeIdentifier(str);
}
