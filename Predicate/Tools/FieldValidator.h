/* This file is part of the KDE project
   Copyright (C) 2006 Jarosław Staniek <staniek@kde.org>

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

#ifndef PREDICATE_TOOLS_FIELDVALIDATOR_H
#define PREDICATE_TOOLS_FIELDVALIDATOR_H

#include <Predicate/predicate_export.h>
#include <Predicate/Tools/Validator>

class QWidget;

namespace Predicate
{
class Field;

namespace Utils
{

//! @short A validator for Predicate data types
/*! This can be used by QLineEdit or subclass to provide validated
  text entry. Curently is supports all integer types, floating point types and booleans.
  Internal validators like KIntValidator or Utils::LongLongValidator are used.
  'unsigned' and 'scale' parameters are taken into account when setting up internal validators.
  @todo date/time support for types
  @todo add validation of the maximum length and other field's properties
*/
class PREDICATE_EXPORT FieldValidator : public Utils::MultiValidator
{
public:
    //! Setups the validator for @a field. Does not keep a pointer to @a field.
    explicit FieldValidator(const Field &field, QWidget * parent = 0);
    ~FieldValidator();
};

}
}

#endif
