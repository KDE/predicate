/* This file is part of the KDE project
   Copyright (C) 2003-2012 Jarosław Staniek <staniek@kde.org>

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

ConstExpressionData::ConstExpressionData(const QVariant& aValue)
 : ExpressionData()
 , value(aValue)
{
    ExpressionDebug << "ConstExpressionData" << ref;
}

ConstExpressionData::~ConstExpressionData()
{
    ExpressionDebug << "~ConstExpressionData" << ref;
}

ConstExpressionData* ConstExpressionData::clone()
{
    ExpressionDebug << "ConstExpressionData::clone" << *this;
    return new ConstExpressionData(*this);
}

Field::Type ConstExpressionData::typeInternal(CallStack* callStack) const
{
    Q_UNUSED(callStack);
    switch (token) {
    case SQL_NULL:
        return Field::Null;
    case INTEGER_CONST:
//! @todo ok?
//! @todo add sign info?
        if (value.type() == QVariant::Int || value.type() == QVariant::UInt) {
            qint64 v = value.toInt();
            if (v <= 0xff && v > -0x80)
                return Field::Byte;
            if (v <= 0xffff && v > -0x8000)
                return Field::ShortInteger;
            return Field::Integer;
        }
        return Field::BigInteger;
    case CHARACTER_STRING_LITERAL:
        if (Field::defaultMaxLength() > 0
            && uint(value.toString().length()) > Field::defaultMaxLength())
        {
            return Field::LongText;
        }
        else {
            return Field::Text;
        }
    case SQL_TRUE:
    case SQL_FALSE:
        return Field::Boolean;
    case REAL_CONST:
        return Field::Double;
    case DATE_CONST:
        return Field::Date;
    case DATETIME_CONST:
        return Field::DateTime;
    case TIME_CONST:
        return Field::Time;
    }
    return Field::InvalidType;
}

void ConstExpressionData::debugInternal(QDebug dbg, CallStack* callStack) const
{
    Q_UNUSED(callStack);
    const QString res = QLatin1String("ConstExp(")
        + Expression::tokenToDebugString(token)
        + QLatin1String(",") + toString().toString()
        + QString::fromLatin1(",type=%1)").arg(Driver::defaultSQLTypeName(type()));
    dbg.nospace() << res.toLocal8Bit().constData();
}

EscapedString ConstExpressionData::toStringInternal(QuerySchemaParameterValueListIterator* params,
                                                    CallStack* callStack) const
{
    Q_UNUSED(params);
    Q_UNUSED(callStack);
    switch (token) {
    case SQL_NULL:
        return EscapedString("NULL");
    case CHARACTER_STRING_LITERAL:
//! @todo better escaping!
        return EscapedString('\'') + value.toString() + '\'';
    case SQL_TRUE:
        return EscapedString("TRUE");
    case SQL_FALSE:
        return EscapedString("FALSE");
    case REAL_CONST:
        return EscapedString::number(value.toPoint().x()) + '.'
                + EscapedString::number(value.toPoint().y());
    case DATE_CONST:
        return EscapedString('\'') + value.toDate().toString(Qt::ISODate) + '\'';
    case DATETIME_CONST:
        return EscapedString('\'')
                + EscapedString(value.toDateTime().date().toString(Qt::ISODate))
                + ' ' + value.toDateTime().time().toString(Qt::ISODate) + '\'';
    case TIME_CONST:
        return EscapedString('\'') + value.toTime().toString(Qt::ISODate) + '\'';
    case INTEGER_CONST:
    default:
        break;
    }
    return EscapedString(value.toString());
}

void ConstExpressionData::getQueryParameters(QuerySchemaParameterList& params)
{
    Q_UNUSED(params);
}

bool ConstExpressionData::validateInternal(ParseInfo *parseInfo, CallStack* callStack)
{
    Q_UNUSED(parseInfo);
    return typeInternal(callStack) != Field::InvalidType;
}

//=========================================

ConstExpression::ConstExpression()
 : Expression(new ConstExpressionData(QVariant()))
{
    ExpressionDebug << "ConstExpression() ctor" << *this;
}

ConstExpression::ConstExpression(int token, const QVariant& value)
        : Expression(new ConstExpressionData(value), ConstExpressionClass, token)
{
}

ConstExpression::ConstExpression(ExpressionData* data, ExpressionClass aClass,
                                 int token)
        : Expression(data, aClass, token)
{
}

ConstExpression::ConstExpression(ExpressionData* data)
    : Expression(data)
{
}

ConstExpression::ConstExpression(const ExplicitlySharedExpressionDataPointer &ptr)
    : Expression(ptr)
{
}

ConstExpression::ConstExpression(const ConstExpression& expr)
        : Expression(expr)
{
}

ConstExpression::~ConstExpression()
{
}

QVariant ConstExpression::value() const
{
    return d->convert<const ConstExpressionData>()->value;
}

void ConstExpression::setValue(const QVariant& value)
{
    d->convert<ConstExpressionData>()->value = value;
}
