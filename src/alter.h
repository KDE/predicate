/* This file is part of the KDE project
   Copyright (C) 2006 Jaroslaw Staniek <js@iidea.pl>

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

#ifndef KEXIDB_ALTER_H
#define KEXIDB_ALTER_H

#include "connection.h"

#include <q3valuelist.h>
#include <qasciidict.h>

#include <kdebug.h>

namespace KexiDB
{
class Connection;
class ConnectionData;

//! @short A tool for handling altering database table schema. 
/*! In relational (and other) databases, table schema altering is not an easy task.
 It may be considered as easy if there is no data that user wants to keep while 
 the table schema is altered. Otherwise, if the table is alredy filled with data,
 there could be no easy algorithm like:
 1. Drop existing table
 2. Create new one with altered schema. 

 Instead, more complex algorithm is needed. To perform the table schema alteration, 
 a list of well defined atomic operations is used as a "recipe".

 1. Look at the current data, and:
 1.1. analyze what values will be removed (in case of impossible conversion 
      or table field removal);
 1.2. analyze what values can be converted (e.g. from numeric types to text), and so on.
 2. Optimize the atomic actions knowing that sometimes a compilation of one action 
    and another that's opposite to the first means "do nothing". The optimization 
    is a simulating of actions' execution.
    For example, when both action A="change field name from 'city' to 'town'" 
    and action B="change field name from 'town' to 'city'" is specified, the compilation 
    of the actions means "change field name from 'city' to 'city'", what is a NULL action.
    On the other hand, we need to execute all the actions on the destination table 
    in proper order, and not just drop them. For the mentioned example, between actions
    A and B there can be an action like C="change the type of field 'city' to LongText".
    If A and B were simply removed, C would become invalid (there is no 'city' field).
 3. Ask user whether she agrees with the results of analysis mentioned in 1. 
 3.2. Additionally, it may be possible to get some hints from the user, as humans usually 
      know more about logic behind the altered table schema than any machine.
      If the user provided hints about the altering, apply them to the actions list.
 4. Create (empty) destination table schema with temporary name, using 
    the information collected so far. 
 5. Copy the data from the source to destionation table. Convert values, 
    move them between fields, using the information collected.
 6. Remove the source table.
 7. Rename the destination table to the name previously assigned for the source table.

 Notes:
 * The actions 4 to 7 should be performed within a database transaction.
 * [todo] We want to take care about database relationships as well. 
    For example, is a table field is removed, relationships related to this field should 
    be also removed (similar rules as in the Query Designer).
 * Especially, care about primary keys and uniquess (indices). Recreate them when needed.
   The problem could be if such analysis may require to fetch the entire table data 
   to the client side. Use "SELECT INTO" statments if possible to avoid such a treat.

 The AlterTableHandler is used in Kexi's Table Designer.
 Already opened Connection object is needed. 

 Use case:
 \code
  Connection *conn = ...

  // add some actions (in reality this is performed by tracking user's actions)
  // Actions 1, 2 will require physical table altering PhysicalAltering
  // Action 3 will only require changes in kexi__fields
  // Action 4 will only require changes in extended table schema written in kexi__objectdata
  AlterTable::ActionList list;

  // 1. rename the "city" field to "town"
  list << new ChangeFieldPropertyAction("city", "name", "town")

  // 2. change type of "town" field to "LongText"
    << new ChangeFieldPropertyAction("town", "type", "LongText")

  // 3. set caption of "town" field to "Town"
    << new ChangeFieldPropertyAction("town", "caption", "Town")

  // 4. set visible decimal places to 4 for "cost" field
    << new ChangeFieldPropertyAction("cost", "visibleDecimalPlaces", 4)

  AlterTableHandler::execute( *conn );

 \endcode

 Actions for Alter
*/
class KEXI_DB_EXPORT AlterTableHandler : public Object
{
	public:
		class ChangeFieldPropertyAction;
		class RemoveFieldAction;
		class InsertFieldAction;
		class MoveFieldPositionAction;

		//! Defines flags for possible altering requirements; can be combined.
		enum AlteringRequirements {
			/*! Physical table altering is required; e.g. ALTER TABLE ADD COLUMN. */
			PhysicalAlteringRequired = 1,

			/*! Data conversion is required; e.g. converting integer 
			 values to string after changing column type from integer to text. */
			DataConversionRequired = 2,

			/* Only changes to extended table schema (or kexi__fields) required,
			 this does not require physical changes for the table; 
			 e.g. changing value of the "visibleDecimalPlaces" property. */
			ExtendedSchemaAlteringRequired = 4
		};

		class ActionBase;
		typedef QAsciiDict<ActionBase> ActionDict; //!< for collecting actions related to a single field
		typedef QIntDict<ActionDict> ActionDictDict; //!< for collecting groups of actions by field UID
		typedef QAsciiDictIterator<ActionBase> ActionDictIterator;
		typedef QIntDictIterator<ActionDict> ActionDictDictIterator;
		typedef QPtrVector<ActionBase> ActionVector; //!< for collecting actions related to a single field

		//! Defines a type for action list.
		typedef QPtrList<ActionBase> ActionList;

		//! Defines a type for action list's iterator.
		typedef QPtrListIterator<ActionBase> ActionListIterator;

		//! Abstract base class used for implementing all the AlterTable actions.
		class KEXI_DB_EXPORT ActionBase {
			public:
				ActionBase(bool null = false);
				virtual ~ActionBase();

				ChangeFieldPropertyAction& toChangeFieldPropertyAction();
				RemoveFieldAction& toRemoveFieldAction();
				InsertFieldAction& toInsertFieldAction();
				MoveFieldPositionAction& toMoveFieldPositionAction();
				
				//! \return true if the action is NULL; used in the Table Designer 
				//! for temporarily collecting actions that have no effect at all.
				bool isNull() const { return m_null; }

				virtual QString debugString() { return "ActionBase"; }
				void debug() { KexiDBDbg << debugString() << " (req = " << alteringRequirements() << ")" << endl; }

			protected:
				//! Sets requirements for altering; used internally by AlterTableHandler object
				void setAlteringRequirements( int alteringRequirements )
					{ m_alteringRequirements = alteringRequirements; }
				
				int alteringRequirements() const { return m_alteringRequirements; }

				virtual void updateAlteringRequirements() {};

				virtual void simplifyActions(ActionDictDict &fieldActions);

			private:
				//! Performs physical execution of this action.
				virtual tristate execute(Connection &conn, TableSchema &table) { return true; }

				//! requirements for altering; used internally by AlterTableHandler object
				int m_alteringRequirements;
				
				//! @internal used for "simplify" algorithm
				int m_order;

				bool m_null : 1;

			friend class AlterTableHandler;
		};

		//! Abstract base class used for implementing table field-related actions.
		class KEXI_DB_EXPORT FieldActionBase : public ActionBase {
			public:
				FieldActionBase(const QString& fieldName, int uid);
				FieldActionBase(bool);
				virtual ~FieldActionBase();

				//! \return field name for this action
				QString fieldName() const { return m_fieldName; }

				/*! \return field's unique identifier
				 This id is needed because in the meantime there can be more than one
				 field sharing the same name, so we need to identify them unambiguously. 
				 After the (valid) altering is completed all the names will be unique. 
				
				 Example scenario when user exchanged the field names:
				 1. At the beginning: [field A], [field B]
				 2. Rename the 1st field to B: [field B], [field B]
				 3. Rename the 2nd field to A: [field B], [field A] */
				int uid() const { return m_fieldUID; }

				//! Sets field name for this action
				void setFieldName(const QString& fieldName) { m_fieldName = fieldName; }

			protected:

				//! field's unique identifier, @see uid()
				int m_fieldUID;
			private:
				QString m_fieldName;
		};

		/*! Defines an action for changing a single property value of a table field.
		 Supported properties are currently:
		 "name", "type", "caption", "description", "unsigned", "length", "precision", 
		 "width", "defaultValue", "primaryKey", "unique", "notNull", "allowEmpty",
		 "autoIncrement", "indexed", "visibleDecimalPlaces"

		 More to come.
		*/
		class KEXI_DB_EXPORT ChangeFieldPropertyAction : public FieldActionBase {
			public:
				ChangeFieldPropertyAction(const QString& fieldName, 
					const QString& propertyName, const QVariant& newValue, int uid);
				//! @internal, used for constructing null action
				ChangeFieldPropertyAction(bool null);
				virtual ~ChangeFieldPropertyAction();

				QString propertyName() const { return m_propertyName; }
				QVariant newValue() const { return m_newValue; }
				virtual QString debugString();

				virtual void simplifyActions(ActionDictDict &fieldActions);

			protected:
				virtual void updateAlteringRequirements();

				//! Performs physical execution of this action.
				virtual tristate execute(Connection &conn, TableSchema &table);

				QString m_propertyName;
				QVariant m_newValue;
		};

		//! Defines an action for removing a single table field.
		class KEXI_DB_EXPORT RemoveFieldAction : public FieldActionBase {
			public:
				RemoveFieldAction(const QString& fieldName, int uid);
				RemoveFieldAction(bool);
				virtual ~RemoveFieldAction();

				virtual QString debugString();

				virtual void simplifyActions(ActionDictDict &fieldActions);

			protected:
				virtual void updateAlteringRequirements();

				//! Performs physical execution of this action.
				virtual tristate execute(Connection &conn, TableSchema &table);
		};

		//! Defines an action for inserting a single table field.
		class KEXI_DB_EXPORT InsertFieldAction : public FieldActionBase {
			public:
				InsertFieldAction(int fieldIndex, KexiDB::Field *newField, int uid);
				//copy ctor
				InsertFieldAction::InsertFieldAction(const InsertFieldAction& action);
				InsertFieldAction(bool);
				virtual ~InsertFieldAction();

				int index() const { return m_index; }
				KexiDB::Field& field() const { return *m_field; }
				void setField(KexiDB::Field* field);
				virtual QString debugString();

				virtual void simplifyActions(ActionDictDict &fieldActions);

			protected:
				virtual void updateAlteringRequirements();

				//! Performs physical execution of this action.
				virtual tristate execute(Connection &conn, TableSchema &table);

				int m_index;

			private:
				KexiDB::Field *m_field;
		};

		/*! Defines an action for moving a single table field to a different 
		 position within table schema. */
		class KEXI_DB_EXPORT MoveFieldPositionAction : public FieldActionBase {
			public:
				MoveFieldPositionAction(int fieldIndex, const QString& fieldName, int uid);
				MoveFieldPositionAction(bool);
				virtual ~MoveFieldPositionAction();

				int index() const { return m_index; }
				virtual QString debugString();

				virtual void simplifyActions(ActionDictDict &fieldActions);

			protected:
				virtual void updateAlteringRequirements();

				//! Performs physical execution of this action.
				virtual tristate execute(Connection &conn, TableSchema &table);

				int m_index;
		};

		AlterTableHandler(Connection &conn);

		virtual ~AlterTableHandler();

		/*! Appends \a action for the alter table tool. */
		void addAction(ActionBase* action);

		/*! Provided for convenience, @see addAction(const ActionBase& action). */
		AlterTableHandler& operator<< ( ActionBase* action );

		/*! Removes an action from the alter table tool at index \a index. */
		void removeAction(int index);

		/*! Removes all actions from the alter table tool. */
		void clear();

		/*! Sets \a actions for the alter table tool. Previous actions are cleared. 
		 \a actions will be owned by the AlterTableHandler object. */
		void setActions(const ActionList& actions);

		/*! \return a list of actions for this AlterTable object. 
		 Use ActionBase::ListIterator to iterate over the list items. */
		const ActionList& actions() const;

		/*! Performs table alteration using predefined actions for table named \a tableName,
		 assuming it already exists. The Connection object passed to the constructor must exist,
		 must be connected and a database must be used. The connection must not be read-only.

		 If \a simulate is true, the execution is only simulated, i.e. al lactions are processed 
		 like for regular execution but no changes are performed physically. 
		 THis mode is used only for debugging purposes.

	@todo For some cases, table schema can completely change, so it will be needed 
		 to refresh all objects depending on it.
		 Implement this!

		 \return true on success, false on failure or when the above requirements are not met
		 (then, you can detailed get error message from KexiDB::Object). 
		 When the action has been cancelled (stopped), returns cancelled value. */
		tristate execute(const QString& tableName, bool simulate = false);

		void debug();

	protected:

		class Private;
		Private *d;
};
}

#endif