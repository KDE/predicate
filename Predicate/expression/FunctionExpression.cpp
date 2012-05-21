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
#include "Expression_p.h"

#include <ctype.h>

using namespace Predicate;

class BuiltInAggregates : public QSet<QString>
{
public:
    BuiltInAggregates()
    {
        insert(QLatin1String("SUM"));
        insert(QLatin1String("MIN"));
        insert(QLatin1String("MAX"));
        insert(QLatin1String("AVG"));
        insert(QLatin1String("COUNT"));
        insert(QLatin1String("STD"));
        insert(QLatin1String("STDDEV"));
        insert(QLatin1String("VARIANCE"));
    }
};

PREDICATE_GLOBAL_STATIC(BuiltInAggregates, _builtInAggregates)

//=========================================

FunctionExpressionData::FunctionExpressionData()
 : ExpressionData()
{
    qDebug() << "FunctionExpressionData" << ref;
}

FunctionExpressionData::FunctionExpressionData(const QString& aName)
        : ExpressionData()
        , name(aName)
{
    qDebug() << "FunctionExpressionData" << ref;
/*    if (aArgs) {
        args = *aArgs;
        args->setParent(this);
    }*/
}

FunctionExpressionData::~FunctionExpressionData()
{
    qDebug() << "~FunctionExpressionData" << ref;
}

FunctionExpressionData* FunctionExpressionData::clone()
{
    qDebug() << "FunctionExpressionData::clone" << *this;
    return new FunctionExpressionData(*this);
}

QDebug FunctionExpressionData::debug(QDebug dbg) const
{
    dbg.nospace() << "FunctionExp(" << name;
    if (args.data()) {
        dbg.nospace() << ',';
        args.data()->debug(dbg);
    }
    dbg.nospace() << QString::fromLatin1(",type=%1)").arg(Driver::defaultSQLTypeName(type()));
    return dbg.space();
}

EscapedString FunctionExpressionData::toString(
    QuerySchemaParameterValueListIterator* params) const
{
    return EscapedString(name + QLatin1Char('('))
           + (args.data() ? args.data()->toString(params) : EscapedString())
           + EscapedString(')');
}

void FunctionExpressionData::getQueryParameters(QuerySchemaParameterList& params)
{
    args->getQueryParameters(params);
}

Field::Type FunctionExpressionData::type() const
{
//! @todo
    return Field::InvalidType;
}

bool FunctionExpressionData::validate(ParseInfo& parseInfo)
{
    if (!ExpressionData::validate(parseInfo))
        return false;

    return args.data() ? args.data()->validate(parseInfo) : true;
}

//=========================================

inline ExpressionClass classForFunctionName(const QString& name)
{
    if (FunctionExpression::isBuiltInAggregate(name))
        return AggregationExpressionClass;
    else
        return FunctionExpressionClass;
}

FunctionExpression::FunctionExpression()
 : Expression(new FunctionExpressionData)
{
    qDebug() << "FunctionExpression() ctor" << *this;
}

FunctionExpression::FunctionExpression(const QString& name)
        : Expression(new FunctionExpressionData(name),
              classForFunctionName(name), 0/*undefined*/)
{
}

FunctionExpression::FunctionExpression(const QString& name, NArgExpression& args)
        : Expression(new FunctionExpressionData(name),//, args.data()),
              classForFunctionName(name), 0/*undefined*/)
{
    if (!args.isNull()) {
        d->convert<FunctionExpressionData>()->args = args.d;
        args.setParent(*this);
    }
}

FunctionExpression::FunctionExpression(const FunctionExpression& expr)
        : Expression(expr)
{
}

FunctionExpression::FunctionExpression(ExpressionData* data)
    : Expression(data)
{
    qDebug() << "FunctionExpression ctor (ExpressionData*)" << *this;
}

FunctionExpression::~FunctionExpression()
{
}

// static
bool FunctionExpression::isBuiltInAggregate(const QString& function)
{
    return _builtInAggregates->contains(function.toUpper());
}

// static
QStringList FunctionExpression::builtInAggregates()
{
    return _builtInAggregates->toList();
}

QString FunctionExpression::name() const
{
    return d->convert<FunctionExpressionData>()->name;
}

NArgExpression FunctionExpression::arguments() const
{
    return NArgExpression(d->convert<FunctionExpressionData>()->args.data());
}
