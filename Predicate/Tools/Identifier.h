/* This file is part of the KDE project
   Copyright (C) 2003-2005 Jarosław Staniek <staniek@kde.org>
   Copyright (C) 2005 Martin Ellis <martin.ellis@kdemail.net>

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

#ifndef PREDICATE_TOOLS_IDENTIFIER_H
#define PREDICATE_TOOLS_IDENTIFIER_H

#include <Predicate/Tools/Validator>

namespace Predicate
{
namespace Utils
{

/*! @return true if @a s is a valid identifier, ie. starts with a letter or '_' character
 and contains only letters, numbers and '_' character. */
PREDICATE_EXPORT bool isIdentifier(const QString& s);

/*! @return valid identifier based on @a s.
 Non-alphanumeric characters (or spaces) are replaced with '_'.
 If a number is at the beginning, '_' is added at start.
 Empty strings are not changed. Case remains unchanged. */
PREDICATE_EXPORT QString stringToIdentifier(const QString &s);

/*! @return useful message "Value of "valueName" column must be an identifier.
  "v" is not a valid identifier.". It is also used by IdentifierValidator.  */
PREDICATE_EXPORT QString identifierExpectedMessage(const QString &valueName,
        const QVariant& v);

//! Validates input for identifier name.
class PREDICATE_EXPORT IdentifierValidator : public Validator
{
public:
    explicit IdentifierValidator(QObject * parent = 0);
    virtual ~IdentifierValidator();
    virtual State validate(QString & input, int & pos) const;

    //! @return true if lower case letters are forced.
    //! By default letters are not forced to lowercase.
    bool isLowerCaseForced() const;

    //! Sets or unsets lower case forcing.
    void setLowerCaseForced(bool set);

protected:
    virtual Validator::Result internalCheck(const QString &valueName, const QVariant& value,
                                            QString *message, QString *details);

    class Private;
    Private* const d;
};

}
}
#endif
