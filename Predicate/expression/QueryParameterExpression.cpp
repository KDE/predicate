/* This file is part of the KDE project
   Copyright (C) 2003-2011 Jarosław Staniek <staniek@kde.org>

   Based on nexp.cpp : Parser module of Python-like language
   (C) 2001 Jarosław Staniek, MIMUW (www.mimuw.edu.pl)

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

#include <Predicate/Expression>
#include <Predicate/Utils>
#include <Predicate/QuerySchema>
#include <Predicate/Tools/Static>
#include "parser/SqlParser.h"

#include <ctype.h>

using namespace Predicate;

QueryParameterExpressionData::QueryParameterExpressionData()
 : ConstExpressionData()
 , m_type(Field::InvalidType)
{
    ExpressionDebug << "QueryParameterExpressionData" << ref;
}

QueryParameterExpressionData::QueryParameterExpressionData(
    Field::Type type, const QVariant& value)
 : ConstExpressionData(value)
 , m_type(type)
{
   ExpressionDebug << "QueryParameterExpressionData" << ref;
}

QueryParameterExpressionData::~QueryParameterExpressionData()
{
    ExpressionDebug << "~QueryParameterExpressionData" << ref;
}

QueryParameterExpressionData* QueryParameterExpressionData::clone()
{
    ExpressionDebug << "QueryParameterExpressionData::clone" << *this;
    return new QueryParameterExpressionData(*this);
}

void QueryParameterExpressionData::debugInternal(QDebug dbg, CallStack* callStack) const
{
    Q_UNUSED(callStack);
    dbg.nospace() << QString::fromLatin1("QueryParExp([%1],type=%2)")
        .arg(value.toString()).arg(Driver::defaultSQLTypeName(type()));
}

EscapedString QueryParameterExpressionData::toStringInternal(QuerySchemaParameterValueListIterator* params,
                                                             CallStack* callStack) const
{
    Q_UNUSED(callStack);
    return params ? params->getPreviousValueAsString(type())
           : EscapedString("[%1]").arg(EscapedString(value.toString()));
}

void QueryParameterExpressionData::getQueryParameters(QuerySchemaParameterList& params)
{
    QuerySchemaParameter param;
    param.message = value.toString();
    param.type = type();
    params.append(param);
}

bool QueryParameterExpressionData::validateInternal(ParseInfo *parseInfo, CallStack* callStack)
{
    Q_UNUSED(parseInfo);
    return typeInternal(callStack) != Field::InvalidType;
}

Field::Type QueryParameterExpressionData::typeInternal(CallStack* callStack) const
{
    Q_UNUSED(callStack);
    return m_type;
}

//=========================================

QueryParameterExpression::QueryParameterExpression()
 : ConstExpression(new QueryParameterExpressionData)
{
    ExpressionDebug << "QueryParameterExpression() ctor" << *this;
}

QueryParameterExpression::QueryParameterExpression(const QString& message)
        : ConstExpression(new QueryParameterExpressionData(Field::Text, message),
              QueryParameterExpressionClass, QUERY_PARAMETER)
{
}

QueryParameterExpression::QueryParameterExpression(const QueryParameterExpression& expr)
        : ConstExpression(expr)
{
}

QueryParameterExpression::QueryParameterExpression(ExpressionData* data)
    : ConstExpression(data)
{
    ExpressionDebug << "QueryParameterExpression ctor (ExpressionData*)" << *this;
}

QueryParameterExpression::QueryParameterExpression(const ExplicitlySharedExpressionDataPointer &ptr)
    : ConstExpression(ptr)
{
}

QueryParameterExpression::~QueryParameterExpression()
{
}

void QueryParameterExpression::setType(Field::Type type)
{
    d->convert<QueryParameterExpressionData>()->m_type = type;
}
