/* This file is part of the KDE project
   Copyright (C) 2002   Lucijan Busch <lucijan@gmx.at>
   Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2003 Jaroslaw Staniek <js@iidea.pl>

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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
 */

#ifndef KEXIDB_FIELD_H
#define KEXIDB_FIELD_H

#include <qvariant.h>
#include <qstring.h>
#include <qpair.h>
#include <qvaluevector.h>
#include <qptrvector.h>

namespace KexiDB {

class TableSchema;
class FieldList;
class Expression;

/*! KexiDB::Field provides information about single database field.
*/
class KEXI_DB_EXPORT Field
{
	public:
		typedef QPtrList<Field> List; //!< list of fields 
		typedef QPtrVector<Field> Vector; //!< vector of fields 
		typedef QPtrListIterator<Field> ListIterator; //!< iterator for list of fields 
		typedef QPair<Field*,Field*> Pair; //!< fields pair
		typedef QPtrList<Pair> PairList; //!< list of fields pair

		/*! Unified (most common used) types of fields. */
		enum Type
		{
			InvalidType = 0,   /*! type that cannot be supported or is not implemented */
			Byte = 1,          /*! 1 byte, signed or unsigned */
			ShortInteger = 2,  /*! 2 bytes, signed or unsigned */
			Integer = 3,       /*! 4 bytes, signed or unsigned */
			BigInteger = 4,    /*! 8 bytes, signed or unsigned */
//			AutoIncrement, /* 4 bytes, like LongInteger, used for keys */
			Boolean = 5,       /*! 0 or 1 */
			Date = 6,          /*! */
			DateTime = 7,      /*! */
			Time = 8,          /*! */
			Float = 9,         /*! 4 bytes */
			Double = 10,       /*! 8 bytes */
			Text = 11,         /*! other name: Varchar; no more than 200 bytes, for efficiency */
			LongText = 12,     /*! other name: Memo. More than 200 bytes*/
			BLOB = 13,         /*! large binary object */

			LastType = 13,     /*! This line should be at the end of the list of types! */
			
			//! special, interanal types:
			Asterisk = 128,    /*! type used in QueryAsterisk subclass objects only,
			                       not used in table definitions,
			                       but only in query definitions */
			Enum = 129         /*! a integer internal with a string list of hints */
		};

		const int defaultTextLength() { return 200; }

		/*! Type groups for fields. */
		enum TypeGroup
		{
			InvalidGroup = 0,
			TextGroup = 1,
			IntegerGroup = 2,
			FloatGroup = 3,
			BooleanGroup = 4,
			DateTimeGroup = 5,
			BLOBGroup = 6, /* large binary object */

			LastTypeGroup = 6 // This line should be at the end of the enum!
		};

/*		enum ColumnType
		{
			SQLInvalid = 0,
			SQLBigInt,
			SQLBinary,
			SQLBoolean,
			SQLDate,
			SQLDecimal,
			SQLDouble,
			SQLFloat,
			SQLInteger,
			SQLInterval,
			SQLLongVarBinary,
			SQLLongVarchar,
			SQLNumeric,
			SQLSmallInt,
			SQLTime,
			SQLTimeStamp,
			SQLTinyInt,
			SQLVarBinary,
			SQLVarchar,
			SQLLastType	// This line should be at the end of the enum!
		};
*/
		enum Constraints
		{
			NoConstraints = 0,
			AutoInc = 1,
			Unique = 2,
			PrimaryKey = 4,
			ForeignKey = 8,
			NotNull = 16,
			NotEmpty = 32, //!< only legal for string-like and blob fields
			Indexed = 64
		};

		enum Options
		{
			NoOptions = 0,
			Unsigned = 1
		};

		Field(TableSchema *tableSchema);
		Field();

		Field(const QString& name, Type ctype,
			uint cconst=NoConstraints,
			uint options = NoOptions,
			uint length=0, uint precision=0,
			QVariant defaultValue=QVariant(),
			const QString& caption = QString::null,
			const QString& description = QString::null,
			uint width = 0);

		/*! Copy constructor. */
		Field(const Field& f);

		virtual ~Field();

		//! Converts field \a type to QVariant equivalent as accurate as possible
		static QVariant::Type variantType(uint type);

		/*! \return a i18n'd type name for \a type (\a type has to be an element from Field::Type, 
		 not greater than Field::LastType) */
		static QString typeName(uint type);

		/*! \return type string for \a type, e.g. "Integer" for Integer type 
		 (not-i18n'd, \a type has to be an element from Field::Type, 
		 not greater than Field::LastType) */
		static QString typeString(uint type);

		/*! \return type for a given \a typeString */
		static Type typeForString(const QString typeString);

		/*! \return type group for a given \a typeGroupString */
		static TypeGroup typeGroupForString(const QString typeGroupString);

		/*! \return group for \a type */
		static TypeGroup typeGroup(uint type);

		/*! \return a i18n'd group name for \a typeGroup 
		 (\a typeGroup has to be an element from Field::TypeGroup) */
		static QString typeGroupName(uint typeGroup);

		/*! \return type group string for \a typeGroup, e.g. "IntegerGroup" for IntegerGroup type 
		 (not-i18n'd, \a type has to be an element from Field::Type, 
		 not greater than Field::LastType) */
		static QString typeGroupString(uint typeGroup);

		//! \return the name of this field
		inline QString name() const { return m_name; }
		
		/*! \return table schema of table that owns this field. */
		virtual TableSchema* table() const;

		/*! \return true if the field is autoincrement (e.g. integer/numeric) */
		inline bool isAutoIncrement() const { return constraints() & AutoInc; }

		/*! \return true if the field is member of single-field primary key */
		inline bool isPrimaryKey() const { return constraints() & PrimaryKey; }

		/*! \return true if the field is member of single-field unique key */
		inline bool isUniqueKey() const { return constraints() & Unique; }

		/*! \return true if the field is member of single-field foreign key */
		inline bool isForeignKey() const { return constraints() & ForeignKey; }

		/*! \return true if the field is not allowed to be null */
		inline bool isNotNull() const { return constraints() & NotNull; }

		/*! \return true if the field is not allowed to be null */
		inline bool isNotEmpty() const { return constraints() & NotEmpty; }

		/*! \return true if the field is indexed using single-field database index. */
		inline bool isIndexed() const { return constraints() & Indexed; }

		/*! \return true if the field is of any numeric type (integer or floating point) */
		inline bool isNumericType() const { return Field::isNumericType(m_type); }
		
		/*! static version of isNumericType() method
		 *! \return true if the field is of any numeric type (integer or floating point)*/
		static bool isNumericType(uint type);

		/*! \return true if the field is of any integer type */
		inline bool isIntegerType() const { return Field::isIntegerType(m_type); }
		
		/*! static version of isIntegerType() method
		 *! \return true if the field is of any integer type */
		static bool isIntegerType(uint type);
		
		/*! \return true if the field is of any floating point numeric type */
		inline bool isFPNumericType() const { return Field::isFPNumericType(m_type); }
		
		/*! static version of isFPNumericType() method 
		 *! \return true if the field is of any floating point numeric type */
		static bool isFPNumericType(uint type);

		/*! \return true if the field is of any date or time related type */
		inline bool isDateTimeType() const { return Field::isDateTimeType(m_type); }
		
		/*! static version of isDateTimeType() method
		 *! \return true if the field is of any date or time related type */
		static bool isDateTimeType(uint type);

		/*! @return true if the field is of any text type */
		inline bool isTextType() const { return Field::isTextType(m_type); }
		
		/*! static version of isTextType() method
		 *! \return true if the field is of any text type */
		static bool isTextType(uint type);
                        
//js: we have m_table for this		/*!
//		 *	@return the table.column that this field references or QString::null if !foreign_key()
//		 */
//		virtual QString		references() const;

		uint options() const { return m_options; }

		void setOptions(uint options) { m_options = options; }

		inline QVariant::Type variantType() const { return variantType(m_type); }

		inline Type type() const { return m_type; }

		inline QString typeName() const { return Field::typeName(m_type); }

		inline TypeGroup typeGroup() const { return Field::typeGroup(m_type); }

		inline QString typeGroupName() const { return Field::typeGroupName(m_type); }

		inline QString typeString() const { return Field::typeString(m_type); }

		inline QString typeGroupString() const { return Field::typeGroupString(m_type); }

		inline QVariant defaultValue() const { return m_defaultValue; }
		
		//! \return length of text is the field type is text
		inline uint length() const { return m_length; }

		/*! \return precision for numeric and other fields that have both length 
		 and precision (floating point types) */
		inline uint precision() const { return m_precision; } 

		//! \return the constraints defined for this field
		inline uint constraints() const { return m_constraints; }

		//! \return order of this field in containing table (counting starts from 0)
		//! (-1 if unspecified)
		inline int order() const { return m_order; }

		//! \return caption of this field
		inline QString caption() const { return m_caption; }

		//! \return caption of this field or - if empty - return its name
		inline QString captionOrName() const { return m_caption.isEmpty() ? m_name : m_caption; }

		//! \return description text for this field
		inline QString description() const { return m_desc; }

		//! \return width of this field (usually in pixels or points)
		//! 0 (the default) means there is no hint for the width
		inline uint width() const { return m_width; }
		
		//! if the type has the unsigned attribute
		inline bool isUnsigned() const { return m_options & Unsigned; }

//		virtual bool isBinary() const;

		//! \return true if this field has EMPTY property (i.e. it is of type string or is a BLOB)
		inline bool hasEmptyProperty() const { return Field::hasEmptyProperty(m_type); }

		/*! static version of hasEmptyProperty() method
		 *! \return true if this field has EMPTY property (i.e. it is of type string or is a BLOB) */
		static bool hasEmptyProperty(uint type);

		void setType(Type t);

		virtual void setTable(TableSchema *table);

		void setName(const QString& n);

		/*! Sets constraints to \a c. If PrimaryKey is set in \a c, also 
		 constraits implied by being primary key are enforced (see setPrimaryKey()).
		 If Indexed is not set in \a c, constraits implied by not being are 
		 enforced as well (see setIndexed()). */
		void setConstraints(uint c);

		void setLength(uint l);

		void setPrecision(uint p);

		void setUnsigned(bool u);

//		void setBinary(bool b);

		void setDefaultValue(const QVariant& def);

		/*! Sets default value decoded from QCString. 
			Decoding errors are detected (value is strictly checked against field type) 
			- if one is encountered, default value is cleared (defaultValue()==QVariant()). 
			\return true if given value was valid for field type. */
		bool setDefaultValue(const QCString& def);

		void setAutoIncrement(bool a);

		/*! Specifies whether the field is single-field primary key or not 
		 (KexiDB::PrimeryKey item). 
		 Use this with caution. Setting this to true implies setting:
		 - setUniqueKey(true)
		 - setNotNull(true)
		 - setNotEmpty(true)
		 - setIndexed(true) */
		void setPrimaryKey(bool p);

		/*! Specifies whether the field has single-field unique constraint or not 
		 (KexiDB::Unique item). Setting this to true implies setting Indexed flag
		 to true (setIndexed(true)), because index is required it control unique constraint. */
		void setUniqueKey(bool u);

		void setForeignKey(bool f);

		/*! Specifies whether the field has single-field unique constraint or not 
		 (KexiDB::NotNull item). Setting this to true implies setting Indexed flag
		 to true (setIndexed(true)), because index is required it control 
		 not null constraint. */
		void setNotNull(bool n);

		/*! Specifies whether the field has single-field unique constraint or not 
		 (KexiDB::NotEmpty item). Setting this to true implies setting Indexed flag
		 to true (setIndexed(true)), because index is required it control 
		 not empty constraint. */
		void setNotEmpty(bool n);

		/*! Specifies whether the field is indexed (KexiDB::Indexed item)
		 (by single-field implicit index) or not. 
		 Use this with caution. Since index is used to control unique, 
		 not null/empty constratins, setting this to false implies setting:
		 - setPrimaryKey(false)
		 - setUniqueKey(false)
		 - setNotNull(false)
		 - setNotEmpty(false)
		 because above flags need index to be present. 
		 Similarly, setting one of the above flags to true, will automatically 
		 do setIndexed(true) for the same reason. */
		void setIndexed(bool s);

		void setCaption(const QString& caption) { m_caption=caption; }

		void setDescription(const QString& description) { m_desc=description; }
		
		void setWidth(uint w) { m_width=w; }

		/*! There can be added asterisks (QueryAsterisk objects) 
		 to query schemas' field list. QueryAsterisk subclasses Field class,
		 and to check if the given object (pointed by Field*) 
		 is asterisk or just ordinary field definition,
		 you can call this method. This is just effective version of QObject::isA().
		 Every QueryAsterisk object returns true here,
		 and every Field object returns false.
		*/
		inline bool isQueryAsterisk() const { return m_type == Asterisk; }
		
		//! \return string for debugging purposes.
		virtual QString debugString() const;

		/*! \return KexiDB::Expression object if the field value is a result expression. 
		 Unless the expression is set with setExpresion(), it is null.
		*/
		inline KexiDB::Expression *expression() { return m_expr; }

		/*! Sets expression data \a expr. If \a expr there was 
		 already expression set, it is destroyed before new assignment.
		 this Field object becames owner of passed \a expr object
		 - you do not have to worry about deleting of \a expr.
		 \a expr can be null - then current field's expression is cleared.
		*/
		void setExpression(KexiDB::Expression *expr);
//<TMP>
		/*! Returns the hints for enum fields */
		QValueVector<QString> enumHints() const { return m_hints; }
		QString enumHint(uint num) { return (num < m_hints.size()) ? m_hints.at(num) : QString::null; }
//</TMP>

		/*! sets the hint for enum fields */
		void setEnumHints(const QValueVector<QString> &l) { m_hints = l; }

	protected:
		FieldList *m_parent; //!< In most cases this points to a TableSchema 
		                     //!< object that field is assigned.
		QString m_name;
//		QString m_reference;
		Type m_type;
		uint m_constraints;
		uint m_length;
		uint m_precision;
		uint m_options;
		QVariant m_defaultValue;
		int m_order;
		QString m_caption;
		QString m_desc;
		uint m_width;
		QValueVector<QString> m_hints;

		Expression *m_expr;

		class KEXI_DB_EXPORT FieldTypeNames : public QValueVector<QString> {
			public:
				FieldTypeNames();
				void init();
			QMap<QString,Type> str2num;
			protected:
				bool m_initialized : 1;
		};
		class KEXI_DB_EXPORT FieldTypeGroupNames : public QValueVector<QString> {
			public: 
				FieldTypeGroupNames();
				void init();
			QMap<QString,TypeGroup> str2num;
			protected:
				bool m_initialized : 1;
		};

		//! real i18n'd type names (and not-i18n'd type name strings)
		static FieldTypeNames m_typeNames;

		//! real i18n'd type group names (and not-i18n'd group name strings)
		static FieldTypeGroupNames m_typeGroupNames;


	friend class Connection;
	friend class FieldList;
	friend class TableSchema;
	friend class QuerySchema;
};




} //namespace KexiDB

#endif
