/* This file is part of the KDE project
   Copyright (C) 2002 Lucijan Busch <lucijan@gmx.at>
   Copyright (C) 2002 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2003-2012 Jarosław Staniek <staniek@kde.org>

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

#ifndef PREDICATE_FIELD_H
#define PREDICATE_FIELD_H

#include <QVariant>
#include <QString>
#include <QPair>
#include <QVector>
#include <QStringList>
#include <QHash>
#include <QtDebug>

#include <Predicate/Tools/Utils>
#include <Predicate/predicate_export.h>

namespace Predicate
{

class TableSchema;
class QuerySchema;
class FieldList;
class Expression;

//! Meta-data for a field
/*! Predicate::Field provides information about single database field.

 Field class has defined following members:
 - name
 - type
 - database constraints
 - additional options
 - maxLength (makes sense mostly for string types)
 - maxLengthStrategy (makes sense mostly for string types)
 - precision (for floating-point type)
 - defaultValue
 - caption (user readable name that can be e.g. translated)
 - description (user readable name additional text, can be useful for developers)
 - defaultWidth (a hint for displaying in tabular mode or as text box)

 Field can also have assigned expression (see Predicate::Expression class,
 and expression() method).

 Note that aliases for fields are defined within query, not in Field object,
 because the same field can be used in different queries with different alias.

 Notes for advanced use: Field obeject is designed to be owned by a parent object.
 Such a parent object can be Predicate::TableSchema, if the field defines single table column,
 or Predicate::QuerySchema, if the field defines an expression (Predicate::Expression class).

 Using expression class for fields allos to define expressions within queries like
 "SELECT AVG(price) FROM products"

 You can choose whether your field is owned by query or table,
 using appropriate constructor, or using parameterless constructor and
 calling setTable() or setQuery() later.

*/
class PREDICATE_EXPORT Field
{
    Q_GADGET
    Q_ENUMS(Type TypeGroup)
    Q_FLAGS(Constraints Constraint Options Option)
public:
    typedef Utils::AutodeletedList<Field*> List; //!< list of fields
    typedef QVector<Field*> Vector; //!< vector of fields
    typedef QList<Field*>::ConstIterator ListIterator; //!< iterator for list of fields
    typedef QPair<Field*, Field*> Pair; //!< fields pair
    typedef QList<Pair> PairList; //!< list of fields pair

    /*! Unified (most common used) types of fields. */
    enum Type {
        InvalidType = 0, /*!< Unsupported/Unimplemented type */
        Byte = 1,        /*!< 1 byte, signed or unsigned */
        FirstType = 1, /*! First type */
        ShortInteger = 2,/*!< 2 bytes, signed or unsigned */
        Integer = 3,     /*!< 4 bytes, signed or unsigned */
        BigInteger = 4,  /*!< 8 bytes, signed or unsigned */
        Boolean = 5,     /*!< 0 or 1 */
        Date = 6,        /*!< */
        DateTime = 7,    /*!< */
        Time = 8,        /*!< */
        Float = 9,       /*!< 4 bytes */
        Double = 10,     /*!< 8 bytes */
        Text = 11,       /*!< Other name: Varchar */
        LongText = 12,   /*!< Other name: Memo */
        BLOB = 13,       /*!< Large binary object */

        LastType = 13,   /*!< This line should be at the end of the list of types! */

        Null = 64,       /*!< Used for fields that are "NULL" expressions. */

        //! Special, internal types:
        Asterisk = 128,  /*!< Used in QueryAsterisk subclass objects only,
                             not used in table definitions,
                             but only in query definitions */
        Enum = 129,      /*!< An integer internal with a string list of hints */
        Map = 130        /*!< Mapping from string to string list (more generic than Enum */
    };

    /*! Type groups for fields. */
    enum TypeGroup {
        InvalidGroup = 0,
        TextGroup = 1,
        IntegerGroup = 2,
        FloatGroup = 3,
        BooleanGroup = 4,
        DateTimeGroup = 5,
        BLOBGroup = 6, /* large binary object */

        LastTypeGroup = 6 // This line should be at the end of the enum!
    };

    /*! Possible constraints defined for a field. */
    enum Constraint {
        NoConstraints = 0,
        AutoInc = 1,
        Unique = 2,
        PrimaryKey = 4,
        ForeignKey = 8,
        NotNull = 16,
        NotEmpty = 32, //!< only legal for string-like and blob fields
        Indexed = 64
    };
    Q_DECLARE_FLAGS(Constraints, Constraint)

    /*! Possible options defined for a field. */
    enum Option {
        NoOptions = 0,
        Unsigned = 1
    };
    Q_DECLARE_FLAGS(Options, Option)

    /*! Creates a database field as a child of @a tableSchema table.
     No other properties are set (even the name), so these should be set later. */
    explicit Field(TableSchema *tableSchema);

    /*! Creates a database field.
     maxLength property is set to 0 (unlimited length).
     No other properties are set (even the name), so these should be set later. */
    Field();

    /*! Creates a database field with specified properties.
     For meaning of @a maxLength argument please refer to setMaxLength(). */
    Field(const QString& name, Type type,
          Constraints constr = NoConstraints,
          Options options = NoOptions,
          uint maxLength = 0, uint precision = 0,
          QVariant defaultValue = QVariant(),
          const QString& caption = QString(),
          const QString& description = QString());

    /*! Copy constructor. */
    Field(const Field& f);

    virtual ~Field();

    //! Converts type @a type to QVariant equivalent as accurate as possible
    static QVariant::Type variantType(Type type);

    /*! @return a i18n-ed type name for @a type (@a type has to be an element from Field::Type,
     not greater than Field::LastType) */
    static QString typeName(Type type);

    /*! @return list of all available i18n-ed type names. */
    static QStringList typeNames();

    /*! @return type string for @a type, e.g. "Integer" for Integer type
     (not-i18n-ed, @a type has to be an element from Field::Type,
     not greater than Field::LastType) */
    static QString typeString(Type type);

    /*! @return type for a given @a typeString */
    static Type typeForString(const QString& typeString);

    /*! @return type group for a given @a typeGroupString */
    static TypeGroup typeGroupForString(const QString& typeGroupString);

    /*! @return group for @a type */
    static TypeGroup typeGroup(Type type);

    /*! @return a i18n-ed group name for @a typeGroup
     (@a typeGroup has to be an element from Field::TypeGroup) */
    static QString typeGroupName(TypeGroup typeGroup);

    /*! @return list of all available i18n-ed type group names. */
    static QStringList typeGroupNames();

    /*! @return type group string for @a typeGroup, e.g. "IntegerGroup" for IntegerGroup type
     (not-i18n-ed, @a type has to be an element from Field::Type,
     not greater than Field::LastType) */
    static QString typeGroupString(TypeGroup typeGroup);

    /*! @return the name of this field */
    inline QString name() const {
        return m_name;
    }

    /*! @return table schema of table that owns this field
     or null if it has no table assigned.
     @see query() */
    virtual TableSchema* table() const;

    /*! Sets @a table schema of table that owns this field.
     This does not adds the field to @a table object.
     You do not need to call this method by hand.
     Call TableSchema::addField(Field *field) instead.
     @see setQuery() */
    virtual void setTable(TableSchema *table);

    /*! For special use when the field defines expression.
     @return query schema of query that owns this field
     or null if it has no query assigned.
     @see table() */
    QuerySchema* query() const;

    /*! For special use when field defines expression.
     Sets @a query schema of query that owns this field.
     This does not adds the field to @a query object.
     You do not need to call this method by hand.
     Call QuerySchema::addField() instead.
     @see setQuery() */
    void setQuery(QuerySchema *query);

    /*! @return true if the field is autoincrement (e.g. integer/numeric) */
    inline bool isAutoIncrement() const {
        return constraints() & AutoInc;
    }

    /*! @return true if the field is member of single-field primary key */
    inline bool isPrimaryKey() const {
        return constraints() & PrimaryKey;
    }

    /*! @return true if the field is member of single-field unique key */
    inline bool isUniqueKey() const {
        return constraints() & Unique;
    }

    /*! @return true if the field is member of single-field foreign key */
    inline bool isForeignKey() const {
        return constraints() & ForeignKey;
    }

    /*! @return true if the field is not allowed to be null */
    inline bool isNotNull() const {
        return constraints() & NotNull;
    }

    /*! @return true if the field is not allowed to be null */
    inline bool isNotEmpty() const {
        return constraints() & NotEmpty;
    }

    /*! @return true if the field is indexed using single-field database index. */
    inline bool isIndexed() const {
        return constraints() & Indexed;
    }

    /*! @return true if the field is of any numeric type (integer or floating point) */
    inline bool isNumericType() const {
        return Field::isNumericType(type());
    }

    /*! static version of isNumericType() method
     *! @return true if the field is of any numeric type (integer or floating point)*/
    static bool isNumericType(Type type);

    /*! @return true if the field is of any integer type */
    inline bool isIntegerType() const {
        return Field::isIntegerType(type());
    }

    /*! static version of isIntegerType() method
     *! @return true if the field is of any integer type */
    static bool isIntegerType(Type type);

    /*! @return true if the field is of any floating point numeric type */
    inline bool isFPNumericType() const {
        return Field::isFPNumericType(type());
    }

    /*! static version of isFPNumericType() method
     *! @return true if the field is of any floating point numeric type */
    static bool isFPNumericType(Type type);

    /*! @return true if the field is of any date or time related type */
    inline bool isDateTimeType() const {
        return Field::isDateTimeType(type());
    }

    /*! static version of isDateTimeType() method
     *! @return true if the field is of any date or time related type */
    static bool isDateTimeType(Type type);

    /*! @return true if the field is of any text type */
    inline bool isTextType() const {
        return Field::isTextType(type());
    }

    /*! static version of isTextType() method
     *! @return true if the field is of any text type */
    static bool isTextType(Type type);

    Options options() const {
        return m_options;
    }

    void setOptions(Options options) {
        m_options = options;
    }

    //! Converts field's type to QVariant equivalent as accurate as possible
    inline QVariant::Type variantType() const {
        return variantType(type());
    }

    /*! @return a type for this field. If there's expression assigned,
     type of the expression (after evaluation) is returned instead. */
    Type type() const;

    //! @return a i18n-ed type name for this field
    inline QString typeName() const {
        return Field::typeName(type());
    }

    //! @return type group for this field
    inline TypeGroup typeGroup() const {
        return Field::typeGroup(type());
    }

    //! @return a i18n-ed type group name for this field
    inline QString typeGroupName() const {
        return Field::typeGroupName(typeGroup());
    }

    //! @return a type string for this field,
    //! for example "Integer" string for Field::Integer type.
    inline QString typeString() const {
        return Field::typeString(type());
    }

    //! @return a type group string for this field,
    //! for example "Integer" string for Field::IntegerGroup.
    inline QString typeGroupString() const {
        return Field::typeGroupString(typeGroup());
    }

    /*! @return (optional) subtype for this field.
     Subtype is a string providing additional hint for field's type.
     E.g. for BLOB type, it can be a MIME type or certain QVariant type name,
     for example: "QPixmap", "QColor" or "QFont" */
    inline QString subType() const {
        return m_subType;
    }

    /*! Sets (optional) subtype for this field.
     @see subType() */
    inline void setSubType(const QString& subType) {
        m_subType = subType;
    }

    //! @return default value for this field. Null value means there
    //! is no default value declared. The variant value is compatible with field's type.
    inline QVariant defaultValue() const {
        return m_defaultValue;
    }

    /*! @return default maximum length of text.
        Default is 0, i.e unlimited length (if the engine supports it). */
    static uint defaultMaxLength();

    /*! Sets default maximum length of text. 0 means unlimited length,
        greater than 0 means specific maximum length. */
    static void setDefaultMaxLength(uint maxLength);

    /*! Strategy for defining maximum length of text for this field.
      Only makes sense if the field type is of Text type.
      Default strategy is DefinedMaxLength.
     */
    enum MaxLengthStrategy {
        DefaultMaxLength,  //!< Default maximum text length defined globally by the application.
                           //!< @see defaultMaxLength()
        DefinedMaxLength   //!< Used if setMaxLength() was called to set specific maximum value
                           //!< or to unlimited (0).
    };

    /*! @return a hint that indicates if the maximum length of text for this field is based on default setting
      (defaultMaxLength()) or was explicitly set.
      Only makes sense if the field type is Text. */
    MaxLengthStrategy maxLengthStrategy() const;

    /*! Sets strategy for defining maximum length of text for this field.
      Only makes sense if the field type is Text.
      Default strategy is DefinedMaxLength.
      Changing this value does not affect maxLength property.

      Fields with DefaultMaxLength strategy does not follow changes made by calling setDefaultMaxLength()
      so to update the default maximum lengths in fields, the app has to iterate over all fields of type Text,
      and reset to the new default as explained in setMaxLength() documentation.
      See documentation for setMaxLength() for information how to reset maxLength to default value.

      @see maxLengthStrategy(), setMaxLength() */
    void setMaxLengthStrategy(MaxLengthStrategy strategy);

    /*! @return maximum length of text allowed for this field. Only meaningful if the type is Text.
      @see setMaxLength() */
    uint maxLength() const;

    /*! Sets maximum length for this field. Only works for Text type.
     It can be specific maximum value or 0 for unlimited length (which will work if engine supports).
     Resets maxLengthStrategy property to DefinedMaxLength.
     To reset to default maximum length, call setMaxLength(defaultMaxLength()) and then
     to indicate this is based on default setting, call setMaxLengthStrategy(DefaultMaxLength).
     @see maxLength(), maxLengthStrategy() */
    void setMaxLength(uint maxLength);

    /*! @return precision for numeric and other fields that have both length (scale)
     and precision (floating point types). */
    inline uint precision() const {
        return m_precision;
    }

    /*! @return scale for numeric and other fields that have both length (scale)
     and precision (floating point types).
     The scale of a numeric is the count of decimal digits in the fractional part,
     to the right of the decimal point. The precision of a numeric is the total count
     of significant digits in the whole number, that is, the number of digits
     to both sides of the decimal point. So the number 23.5141 has a precision
     of 6 and a scale of 4. Integers can be considered to have a scale of zero. */
    inline uint scale() const {
        return m_maxLength;
    }

//! @todo should we keep extended properties here or move them to a QVariant dictionary?
    /*! @return number of decimal places that should be visible to the user,
     e.g. within table view widget, form or printout.
     Only meaningful if the field type is floating point or (in the future: decimal or currency).

     - Any value less than 0 (-1 is the default) means that there should be displayed all digits
       of the fractional part, except the ending zeros. This is known as "auto" mode.
       For example, 12.345000 becomes 12.345.

     - Value of 0 means that all the fractional part should be hidden (as well as the dot or comma).
       For example, 12.345000 becomes 12.

     - Value N > 0 means that the fractional part should take exactly N digits.
       If the fractional part is shorter than N, additional zeros are appended.
       For example, "12.345" becomes "12.345000" if N=6.
    */
    inline int visibleDecimalPlaces() const {
        return m_visibleDecimalPlaces;
    }

    /*! @return the constraints defined for this field. */
    inline Constraints constraints() const {
        return m_constraints;
    }

    /*! @return order of this field in containing table (counting starts from 0)
    (-1 if unspecified). */
    inline int order() const {
        return m_order;
    }

    /*! @return caption of this field. */
    inline QString caption() const {
        return m_caption;
    }

    /*! @return caption of this field or - if empty - return its name. */
    inline QString captionOrName() const {
        return m_caption.isEmpty() ? m_name : m_caption;
    }

    /*! @return description text for this field. */
    inline QString description() const {
        return m_desc;
    }

    //! if the type has the unsigned attribute
    inline bool isUnsigned() const {
        return m_options & Unsigned;
    }

    /*! @return true if this Field.has EMPTY property (i.e. it is of type
    string or is a BLOB). */
    inline bool hasEmptyProperty() const {
        return Field::hasEmptyProperty(type());
    }

    /*! static version of hasEmptyProperty() method
     @return true if this field type has EMPTY property (i.e. it is string or BLOB type) */
    static bool hasEmptyProperty(Type type);

    /*! @return true if this field can be auto-incremented.
     Actually, returns true for integer field type. @see IntegerType, isAutoIncrement() */
    inline bool isAutoIncrementAllowed() const {
        return Field::isAutoIncrementAllowed(type());
    }

    /*! static version of isAutoIncrementAllowed() method
     @return true if this field type can be auto-incremented. */
    static bool isAutoIncrementAllowed(Type type);

    /*! Sets type @a t for this field.
     This does nothing if there's expression assigned.
     @see setExpression() */
    void setType(Type t);

    /*! Sets name @a name for this field. */
    void setName(const QString& name);

    /*! Sets constraints to @a c. If PrimaryKey is set in @a c, also
     constraits implied by being primary key are enforced (see setPrimaryKey()).
     If Indexed is not set in @a c, constraits implied by not being are
     enforced as well (see setIndexed()). */
    void setConstraints(Constraints c);

    /*! Sets scale for this field. Only works for floating-point types.
     @see scale() */
    void setScale(uint s);

    /*! Sets number of decimal places that should be visible to the user.
     @see visibleDecimalPlaces() */
    void setVisibleDecimalPlaces(int p);

    /*! Sets scale for this field. Only works for floating-point types. */
    void setPrecision(uint p);

    /*! Sets unsigned flag for this field. Only works for integer types. */
    void setUnsigned(bool u);

    /*! Sets default value for this field. Setting null value removes the default value.
     @see defaultValue() */
    void setDefaultValue(const QVariant& def);

    /*! Sets default value decoded from QByteArray.
      Decoding errors are detected (value is strictly checked against field type)
      - if one is encountered, default value is cleared (defaultValue()==QVariant()).
      @return true if given value was valid for field type. */
    bool setDefaultValue(const QByteArray& def);

    /*! Sets auto increment flag. Only available to set true,
     if isAutoIncrementAllowed() is true. */
    void setAutoIncrement(bool a);

    /*! Specifies whether the field is single-field primary key or not
     (Predicate::PrimeryKey item).
     Use this with caution. Setting this to true implies setting:
     - setUniqueKey(true)
     - setNotNull(true)
     - setNotEmpty(true)
     - setIndexed(true)

     Setting this to false implies setting setAutoIncrement(false). */
    void setPrimaryKey(bool p);

    /*! Specifies whether the Field.has single-field unique constraint or not
     (Predicate::Unique item). Setting this to true implies setting Indexed flag
     to true (setIndexed(true)), because index is required it control unique constraint. */
    void setUniqueKey(bool u);

    /*! Sets whether the Field.has to be declared with single-field foreign key.
     Used in IndexSchema::setForeigKey(). */
    void setForeignKey(bool f);

    /*! Specifies whether the Field.has single-field unique constraint or not
     (Predicate::NotNull item). Setting this to true implies setting Indexed flag
     to true (setIndexed(true)), because index is required it control
     not null constraint. */
    void setNotNull(bool n);

    /*! Specifies whether the Field.has single-field unique constraint or not
     (Predicate::NotEmpty item). Setting this to true implies setting Indexed flag
     to true (setIndexed(true)), because index is required it control
     not empty constraint. */
    void setNotEmpty(bool n);

    /*! Specifies whether the field is indexed (Predicate::Indexed item)
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

    /*! Sets caption for this field to @a caption. */
    void setCaption(const QString& caption) {
        m_caption = caption;
    }

    /*! Sets description for this field to @a description. */
    void setDescription(const QString& description) {
        m_desc = description;
    }

    /*! There can be added asterisks (QueryAsterisk objects)
     to query schemas' field list. QueryAsterisk subclasses Field class,
     and to check if the given object (pointed by Field*)
     is asterisk or just ordinary field definition,
     you can call this method. This is just effective version of QObject::isA().
     Every QueryAsterisk object returns true here,
     and every Field object returns false.
    */
    inline bool isQueryAsterisk() const {
        return m_type == Field::Asterisk;
    }

    /*! @return Predicate::Expression object if the field value is an
     expression.  Unless the expression is set with setExpression(), it is null.
    */
    Expression expression();

    const Expression expression() const;

    /*! Sets expression data @a expr. If there was
     already expression set, it is removed before new assignment.
     This Field object becames logical owner of @a expr object,
     so do not use the expression for other objects (you can call ExpressioN::clone()).
     Current field's expression is deleted, if exists.

     Because the field defines an expression, it should be assigned to a query,
     not to a table.
    */
    void setExpression(const Expression& expr);

    /*! @return true if there is expression defined for this field.
     This method is provided for better readibility
     - does the same as expression().isNull(). */
    bool isExpression() const;

//<TMP>
    /*! @return the hints for enum fields. */
    QVector<QString> enumHints() const {
        return m_hints;
    }
    QString enumHint(uint num) {
        return (num < (uint)m_hints.size()) ? m_hints.at(num) : QString();
    }
    /*! sets the hint for enum fields */
    void setEnumHints(const QVector<QString> &l) {
        m_hints = l;
    }
//</TMP>

    /*! @return custom property @a propertyName.
     If there is no such a property, @a defaultValue is returned. */
    QVariant customProperty(const QByteArray& propertyName,
                            const QVariant& defaultValue = QVariant()) const;

    //! Sets value @a value for custom property @a propertyName
    void setCustomProperty(const QByteArray& propertyName, const QVariant& value);

    //! A data type used for handling custom properties of a field
    typedef QHash<QByteArray, QVariant> CustomPropertiesMap;

    //! @return all custom properties
    inline CustomPropertiesMap customProperties() const {
        return m_customProperties ? *m_customProperties : CustomPropertiesMap();
    }

protected:
    /*! Creates a database field as a child of @a querySchema table
     Assigns @a expr expression to this field, if present.
     Used internally by query schemas, e.g. to declare asterisks or
     to add expression columns.
     No other properties are set, so these should be set later. */
    Field(QuerySchema *querySchema, const Expression& expr);

    /*! @overload Field(QuerySchema*, const Expression&) */
    explicit Field(QuerySchema *querySchema);

    /*! @internal Used by constructors. */
    void init();

    //! @return a deep copy of this object. Used in @ref FieldList(const FieldList& fl).
    virtual Field* copy() const;

    FieldList *m_parent; //!< In most cases this points to a TableSchema
    //!< object that field is assigned.
    QString m_name;
    QString m_subType;
    Constraints m_constraints;
    MaxLengthStrategy m_maxLengthStrategy;
    uint m_maxLength; //!< also used for storing scale for floating point types
    uint m_precision;
    int m_visibleDecimalPlaces; //!< used in visibleDecimalPlaces()
    Options m_options;
    QVariant m_defaultValue;
    int m_order;
    QString m_caption;
    QString m_desc;
    QVector<QString> m_hints;

    Expression* m_expr;
    CustomPropertiesMap* m_customProperties;

    //! @internal Used in m_typeNames member to handle i18n-ed type names
    class PREDICATE_EXPORT FieldTypeNames : public QVector<QString>
    {
    public:
        FieldTypeNames();
        void init();
        QHash<QString, Type> str2num;
        QStringList names;
    protected:
        bool m_initialized;
    };

    //! @internal Used in m_typeGroupNames member to handle i18n-ed type group names
    class PREDICATE_EXPORT FieldTypeGroupNames : public QVector<QString>
    {
    public:
        FieldTypeGroupNames();
        void init();
        QHash<QString, TypeGroup> str2num;
        QStringList names;
    protected:
        bool m_initialized;
    };

    //! real i18n-ed type names (and not-i18n-ed type name strings)
    static FieldTypeNames m_typeNames;

    //! real i18n-ed type group names (and not-i18n-ed group name strings)
    static FieldTypeGroupNames m_typeGroupNames;

private:
    Type m_type;

    friend class Connection;
    friend class FieldList;
    friend class TableSchema;
    friend class QuerySchema;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Field::Constraints)
Q_DECLARE_OPERATORS_FOR_FLAGS(Field::Options)

} //namespace Predicate

//! Sends information about field @a field to debug output @a dbg.
PREDICATE_EXPORT QDebug operator<<(QDebug dbg, const Predicate::Field& field);

//! Sends information about field type @a type to debug output @a dbg.
PREDICATE_EXPORT QDebug operator<<(QDebug dbg, Predicate::Field::Type type);

//! Sends information about field type group @a typeGroup to debug output @a dbg.
PREDICATE_EXPORT QDebug operator<<(QDebug dbg, Predicate::Field::TypeGroup typeGroup);

#endif
