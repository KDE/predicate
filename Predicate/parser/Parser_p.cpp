/* This file is part of the KDE project
   Copyright (C) 2004-2012 Jarosław Staniek <staniek@kde.org>

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

#include "Parser_p.h"
#include "SqlParser.h"

#include <QRegExp>
#include <QMutableListIterator>

#include <assert.h>

using namespace Predicate;

Parser *parser = 0;
Field *field = 0;
QList<Field*> fieldList;
int current = 0;
QByteArray ctoken;

extern int yylex_destroy(void);

//-------------------------------------

Parser::Private::Private()
        : initialized(false)
{
    clear();
    table = 0;
    select = 0;
    db = 0;
}

Parser::Private::~Private()
{
    delete select;
    delete table;
}

void Parser::Private::clear()
{
    operation = Parser::OP_None;
    error = ParserError();
}

//-------------------------------------

ParseInfoInternal::ParseInfoInternal(QuerySchema *query)
: ParseInfo(query)
{
}

ParseInfoInternal::~ParseInfoInternal()
{
}

void ParseInfoInternal::appendPositionForTableOrAliasName(const QString &tableOrAliasName, int pos)
{
    QList<int> *list = d->repeatedTablesAndAliases.value(tableOrAliasName);
    if (!list) {
        list = new QList<int>();
        d->repeatedTablesAndAliases.insert(tableOrAliasName, list);
    }
    list->append(pos);
}

void ParseInfoInternal::setErrorMessage(const QString &message)
{
    d->errorMessage = message;
}

void ParseInfoInternal::setErrorDescription(const QString &description)
{
    d->errorDescription = description;
}

//-------------------------------------

extern int yyparse();
extern void tokenize(const char *data);

void yyerror(const char *str)
{
    PreDbg << "error: " << str;
    PreDbg << "at character " << current << " near tooken " << ctoken;
    parser->setOperation(Parser::OP_Error);

    const bool otherError = (qstrnicmp(str, "other error", 11) == 0);

    if ((   parser->error().type().isEmpty()
         && (str == 0 || strlen(str) == 0 || qstrnicmp(str, "syntax error", 12) == 0 || qstrnicmp(str, "parse error", 11) == 0)
        )
        || otherError
       )
    {
        PreDbg << parser->statement();
        QString ptrline(current, QLatin1Char(' '));

        ptrline += QLatin1String("^");

        PreDbg << ptrline;

        //lexer may add error messages
        QString lexerErr = parser->error().message();

        QString errtypestr = QLatin1String(str);
        if (lexerErr.isEmpty()) {
            if (errtypestr.startsWith(QString::fromLatin1("parse error, expecting `IDENTIFIER'"))) {
                lexerErr = QObject::tr("identifier was expected");
            }
        }

        if (!otherError) {
            if (!lexerErr.isEmpty()) {
                lexerErr.prepend(QLatin1String(": "));
            }

            if (Predicate::isPredicateSQLKeyword(ctoken))
                parser->setError(ParserError(QObject::tr("Syntax Error"),
                                             QObject::tr("\"%1\" is a reserved keyword.")
                                                .arg(QLatin1String(ctoken)) + lexerErr,
                                             ctoken, current));
            else
                parser->setError(ParserError(QObject::tr("Syntax Error"),
                                             QObject::tr("Syntax Error near \"%1\".")
                                                .arg(QLatin1String(ctoken)) + lexerErr,
                                             ctoken, current));
        }
    }
}

void setError(const QString& errName, const QString& errDesc)
{
    parser->setError(ParserError(errName, errDesc, ctoken, current));
    yyerror(errName.toLatin1());
}

void setError(const QString& errDesc)
{
    setError(QObject::tr("Other error"), errDesc);
}

/* this is better than assert() */
#define IMPL_ERROR(errmsg) setError(QObject::tr("Implementation error"), QLatin1String(errmsg))

bool parseData(Parser *p, const char *data)
{
    /* todo: make this REENTRANT */
    parser = p;
    parser->clear();
    field = 0;
    fieldList.clear();

    if (!data) {
        ParserError err(QObject::tr("Error"), QObject::tr("No query statement specified."), ctoken, current);
        parser->setError(err);
        yyerror("");
        parser = 0;
        return false;
    }

    tokenize(data);
    if (!parser->error().type().isEmpty()) {
        parser = 0;
        return false;
    }
    yyparse();

    bool ok = true;
    if (parser->operation() == Parser::OP_Select) {
        PreDbg << "parseData(): ok";
//   PreDbg << "parseData(): " << tableDict.count() << " loaded tables";
        /*   TableSchema *ts;
              for(QDictIterator<TableSchema> it(tableDict); TableSchema *s = tableList.first(); s; s = tableList.next())
              {
                PreDbg << " " << s->name();
              }*/
    } else {
        ok = false;
    }
    yylex_destroy();
    parser = 0;
    return ok;
}


/* Adds @a column to @a querySchema. @a column can be in a form of
 table.field, tableAlias.field or field
*/
bool addColumn(ParseInfo *parseInfo, Expression *columnExpr)
{
    if (!columnExpr->validate(parseInfo)) {
        setError(parseInfo->errorMessage(), parseInfo->errorDescription());
        return false;
    }

    VariableExpression v_e(columnExpr->toVariable());
    if (columnExpr->expressionClass() == VariableExpressionClass && !v_e.isNull()) {
        //it's a variable:
        if (v_e.name() == QLatin1String("*")) {//all tables asterisk
            if (parseInfo->querySchema()->tables()->isEmpty()) {
                setError(QObject::tr("\"*\" could not be used if no tables are specified."));
                return false;
            }
            parseInfo->querySchema()->addAsterisk(new QueryAsterisk(parseInfo->querySchema()));
        } else if (v_e.tableForQueryAsterisk()) {//one-table asterisk
            parseInfo->querySchema()->addAsterisk(
                new QueryAsterisk(parseInfo->querySchema(), v_e.tableForQueryAsterisk()));
        } else if (v_e.field()) {//"table.field" or "field" (bound to a table or not)
            parseInfo->querySchema()->addField(v_e.field(), v_e.tablePositionForField());
        } else {
            IMPL_ERROR("addColumn(): unknown case!");
            return false;
        }
        return true;
    }

    //it's complex expression
    parseInfo->querySchema()->addExpression(*columnExpr);
    return true;
}

//! clean up no longer needed temporary objects
#define CLEANUP \
    /*delete colViews;*/ \
    /*delete tablesList;*/ \
    delete options

QuerySchema* buildSelectQuery(
    QuerySchema* querySchema, NArgExpression* _colViews,
    NArgExpression* _tablesList, SelectOptionsInternal* options)
{
    ParseInfoInternal parseInfo(querySchema);

    // remove from heap (using heap was requered because parser uses union)
    NArgExpression colViews;
    if (_colViews) {
        colViews = *_colViews;
        delete _colViews;
    }
    NArgExpression tablesList;
    if (_tablesList) {
        tablesList = *_tablesList;
        delete _tablesList;
    }

    //-------tables list
    uint columnNum = 0;
    /*! @todo use this later if there are columns that use database fields,
              e.g. "SELECT 1 from table1 t, table2 t") is ok however. */
    //used to collect information about first repeated table name or alias:
    if (!tablesList.isEmpty()) {
        for (int i = 0; i < tablesList.argCount(); i++, columnNum++) {
            Expression e(tablesList.arg(i));
            VariableExpression t_e;
            QString aliasString;
            if (e.expressionClass() == SpecialBinaryExpressionClass) {
                BinaryExpression t_with_alias = e.toBinary();
                assert(e.isBinary());
                assert(t_with_alias.left().expressionClass() == VariableExpressionClass);
                assert(t_with_alias.right().expressionClass() == VariableExpressionClass
                       && (t_with_alias.token() == AS || t_with_alias.token() == AS_EMPTY));
                t_e = t_with_alias.left().toVariable();
                aliasString = t_with_alias.right().toVariable().name();
            } else {
                t_e = e.toVariable();
            }
            assert(t_e.isVariable());
            QString tname = t_e.name();
            TableSchema *s = parser->db()->tableSchema(tname);
            if (!s) {
                setError(
                    QObject::tr("Table \"%1\" does not exist.").arg(tname));
                CLEANUP;
                return 0;
            }
            QString tableOrAliasName = Predicate::iifNotEmpty(aliasString, tname);
            if (!aliasString.isEmpty()) {
//    PreDbg << "- add alias for table: " << aliasString;
            }
            // 1. collect information about first repeated table name or alias
            //    (potential ambiguity)
            parseInfo.appendPositionForTableOrAliasName(tableOrAliasName, i);
//   PreDbg << "addTable: " << tname;
            querySchema->addTable(s, aliasString);
        }
    }

    /* set parent table if there's only one */
    if (querySchema->tables()->count() == 1)
        querySchema->setMasterTable(querySchema->tables()->first());

    //-------add fields
    if (!colViews.isEmpty()) {
        columnNum = 0;
        bool containsAsteriskColumn = false; // used to check duplicated asterisks (disallowed)
        for (int i = 0; i < colViews.argCount(); i++, columnNum++) {
            Expression e(colViews.arg(i));
            Expression columnExpr(e);
            VariableExpression aliasVariable;
            if (e.expressionClass() == SpecialBinaryExpressionClass && e.isBinary()
                    && (e.token() == AS || e.token() == AS_EMPTY)) {
                //SpecialBinaryExpressionClass: with alias
                columnExpr = e.toBinary().left();
                aliasVariable = e.toBinary().right().toVariable();
                if (aliasVariable.isNull()) {
                    setError(QObject::tr("Invalid alias definition for column \"%1\"")
                                  .arg(columnExpr.toString().toString())); //ok?
                    break;
                }
            }

            const int c = columnExpr.expressionClass();
            const bool isExpressionField =
                c == ConstExpressionClass
                || c == UnaryExpressionClass
                || c == ArithmeticExpressionClass
                || c == LogicalExpressionClass
                || c == RelationalExpressionClass
                || c == FunctionExpressionClass
                || c == AggregationExpressionClass;

            if (c == VariableExpressionClass) {
                if (columnExpr.toVariable().name() == QLatin1String("*")) {
                    if (containsAsteriskColumn) {
                        setError(QObject::tr("More than one asterisk (*) is not allowed"));
                        CLEANUP;
                        return 0;
                    }
                    else {
                        containsAsteriskColumn = true;
                    }
                }
                // addColumn() will handle this
            }
            else if (isExpressionField) {
                //expression object will be reused, take, will be owned, do not destroy
//  PreDbg << colViews->list.count() << " " << it.current()->debugString();
#ifdef __GNUC__
#warning ok? //predicate: it.remove();
#else
#pragma WARNING(ok?)
#endif
            } else if (aliasVariable.isNull()) {
                setError(QObject::tr("Invalid \"%1\" column definition")
                         .arg(e.toString().toString())); //ok?
                break;
            }
            else {
                //take first (left) argument of the special binary expr, will be owned, do not destroy
                e.toBinary().setLeft(Expression());
            }

            if (!addColumn(&parseInfo, &columnExpr)) {
                break;
            }

            if (!aliasVariable.isNull()) {
//    PreDbg << "ALIAS \"" << aliasVariable->name << "\" set for column "
//     << columnNum;
                querySchema->setColumnAlias(columnNum, aliasVariable.name());
            }
        } // for
        if (!parser->error().message().isEmpty()) { // we could not return earlier (inside the loop)
            // because we want run CLEANUP what could crash QMutableListIterator.
            CLEANUP;
            return 0;
        }
    }
    //----- SELECT options
    if (options) {
        //----- WHERE expr.
        if (!options->whereExpr.isNull()) {
            if (!options->whereExpr.validate(&parseInfo)) {
                setError(parseInfo.errorMessage(), parseInfo.errorDescription());
                CLEANUP;
                return 0;
            }
            querySchema->setWhereExpression(options->whereExpr);
        }
        //----- ORDER BY
        if (options->orderByColumns) {
            OrderByColumnList *orderByColumnList = querySchema->orderByColumnList();
            uint count = options->orderByColumns->count();
            OrderByColumnInternal::ListConstIterator it(options->orderByColumns->constEnd());
            --it;
            for (;count > 0; --it, --count)
                /*opposite direction due to parser specifics*/
            {
                //first, try to find a column name or alias (outside of asterisks)
                QueryColumnInfo *columnInfo = querySchema->columnInfo((*it).aliasOrName, false/*outside of asterisks*/);
                if (columnInfo) {
                    orderByColumnList->appendColumn(*columnInfo, (*it).ascending);
                } else {
                    //failed, try to find a field name within all the tables
                    if ((*it).columnNumber != -1) {
                        if (!orderByColumnList->appendColumn(*querySchema,
                                                            (*it).ascending, (*it).columnNumber - 1)) {
                            setError(QObject::tr("Could not define sorting - no column at position %1")
                                          .arg((*it).columnNumber));
                            CLEANUP;
                            return 0;
                        }
                    } else {
                        Field * f = querySchema->findTableField((*it).aliasOrName);
                        if (!f) {
                            setError(QObject::tr("Could not define sorting - "
                                          "column name or alias \"%1\" does not exist").arg((*it).aliasOrName));
                            CLEANUP;
                            return 0;
                        }
                        orderByColumnList->appendField(*f, (*it).ascending);
                    }
                }
            }
        }
    }

// PreDbg << "Select ColViews=" << (colViews ? colViews->debugString() : QString())
//  << " Tables=" << (tablesList ? tablesList->debugString() : QString()s);

    CLEANUP;
    return querySchema;
}

#undef CLEANUP

