/* This file is part of the KDE project
   Copyright (C) 2006 Jarosław Staniek <staniek@kde.org>

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

#include "QuerySchemaParameter.h"
#include "Driver.h"

#include <QtDebug>
#include <qpointer.h>

using namespace Predicate;

QuerySchemaParameter::QuerySchemaParameter()
        : type(Field::InvalidType)
{
}

QuerySchemaParameter::~QuerySchemaParameter()
{
}

QString QuerySchemaParameter::debugString() const
{
    return QString("msg=\"%1\" type=\"%2\"").arg(Field::typeName(type)).arg(message);
}

void Predicate::debug(const QuerySchemaParameterList& list)
{
    PreDbg << QString("Query parameters (%1):").arg(list.count()) << endl;
    foreach(const QuerySchemaParameter& parameter, list)
    PreDbg << " - " << parameter.debugString() << endl;
}

//================================================

class QuerySchemaParameterValueListIterator::Private
{
public:
    Private(Driver& aDriver, const QList<QVariant>& aParams)
            : driver(&aDriver)
            , params(aParams) {
        //move to last item, as the order is reversed due to parser's internals
        paramsIt = params.constEnd(); //fromLast();
        --paramsIt;
        paramsItPosition = params.count();
    }
    QPointer<Driver> driver;
    const QList<QVariant> params;
    QList<QVariant>::ConstIterator paramsIt;
    uint paramsItPosition;
};

QuerySchemaParameterValueListIterator::QuerySchemaParameterValueListIterator(
    Driver& driver, const QList<QVariant>& params)
        : d(new Private(driver, params))
{
}

QuerySchemaParameterValueListIterator::~QuerySchemaParameterValueListIterator()
{
    delete d;
}

QVariant QuerySchemaParameterValueListIterator::getPreviousValue()
{
    if (d->paramsItPosition == 0) { //d->params.constEnd()) {
        PreWarn << "QuerySchemaParameterValues::getPreviousValue() no prev value" << endl;
        return QVariant();
    }
    QVariant res(*d->paramsIt);
    --d->paramsItPosition;
    --d->paramsIt;
// ++d->paramsIt;
    return res;
}

QString QuerySchemaParameterValueListIterator::getPreviousValueAsString(Field::Type type)
{
    if (d->paramsItPosition == 0) { //d->params.constEnd()) {
        PreWarn << "QuerySchemaParameterValues::getPreviousValueAsString() no prev value" << endl;
        return d->driver->valueToSQL(type, QVariant()); //"NULL"
    }
    QString res(d->driver->valueToSQL(type, *d->paramsIt));
    --d->paramsItPosition;
    --d->paramsIt;
// ++d->paramsIt;
    return res;
}