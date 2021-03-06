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
#include "parser/Parser_p.h"

#include <ctype.h>

using namespace Predicate;

NArgExpressionData::NArgExpressionData()
 : ExpressionData()
{
    ExpressionDebug << "NArgExpressionData" << ref;
}

NArgExpressionData::~NArgExpressionData()
{
    ExpressionDebug << "~NArgExpressionData" << ref;
}

NArgExpressionData* NArgExpressionData::clone()
{
    ExpressionDebug << "NArgExpressionData::clone" << *this;
    return new NArgExpressionData(*this);
}

bool NArgExpressionData::validateInternal(ParseInfo *parseInfo, CallStack* callStack)
{
    foreach(ExplicitlySharedExpressionDataPointer data, children) {
        if (!data->validate(parseInfo, callStack))
            return false;
    }
    return true;
}

void NArgExpressionData::debugInternal(QDebug dbg, CallStack* callStack) const
{
    dbg.nospace() << "NArgExp(class="
        << expressionClassName(expressionClass);
    foreach(ExplicitlySharedExpressionDataPointer data, children) {
        dbg.nospace() << ", ";
        data->debug(dbg, callStack);
    }
    dbg.nospace() << ")";
}

EscapedString NArgExpressionData::toStringInternal(QuerySchemaParameterValueListIterator* params,
                                                   CallStack* callStack) const
{
    EscapedString s;
    s.reserve(256);
    foreach(ExplicitlySharedExpressionDataPointer data, children) {
        if (!s.isEmpty())
            s += ", ";
        s += data->toString(params, callStack);
    }
    return s;
}

void NArgExpressionData::getQueryParameters(QuerySchemaParameterList& params)
{
    foreach(ExplicitlySharedExpressionDataPointer data, children) {
        data->getQueryParameters(params);
    }
}

//=========================================

NArgExpression::NArgExpression()
 : Expression(new NArgExpressionData)
{
    ExpressionDebug << "NArgExpression() ctor" << *this;
}

NArgExpression::NArgExpression(ExpressionData* data)
 : Expression(data)
{
    ExpressionDebug << "NArgExpression(ExpressionData*) ctor" << *this;
}

NArgExpression::NArgExpression(ExpressionClass aClass, int token)
        : Expression(new NArgExpressionData, aClass, token)
{
    ExpressionDebug << "NArgExpression(ExpressionClass, int) ctor" << *this;
}

NArgExpression::NArgExpression(const NArgExpression& expr)
        : Expression(expr)
{
}

NArgExpression::NArgExpression(const ExplicitlySharedExpressionDataPointer &ptr)
    : Expression(ptr)
{
}

NArgExpression::~NArgExpression()
{
}

void NArgExpression::append(const Expression& expr)
{
    appendChild(expr);
}

void NArgExpression::prepend(const Expression& expr)
{
    prependChild(expr);
}

Expression NArgExpression::arg(int n) const
{
    return Expression(d->children.value(n));
}

void NArgExpression::insert(int i, const Expression& expr)
{
    insertChild(i, expr);
}

bool NArgExpression::remove(const Expression& expr)
{
    return removeChild(expr);
}

void NArgExpression::removeAt(int i)
{
    removeChild(i);
}

Expression NArgExpression::takeAt(int i)
{
    return takeChild(i);
}

int NArgExpression::indexOf(const Expression& expr, int from) const
{
    return indexOfChild(expr, from);
}

int NArgExpression::lastIndexOf(const Expression& expr, int from) const
{
    return lastIndexOfChild(expr, from);
}

int NArgExpression::argCount() const
{
    return d->children.count();
}

bool NArgExpression::isEmpty() const
{
    return d->children.isEmpty();
}
