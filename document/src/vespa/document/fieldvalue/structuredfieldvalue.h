// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
/**
 * \class document::StructuredFieldValue
 * \ingroup fieldvalue
 *
 * \brief Base class for Document and Struct field values.
 *
 * This class contains the common functionality between Document and Struct
 * parts. This includes most their functionality.
 */
#pragma once

#include <vespa/document/base/field.h>
#include <vespa/document/fieldvalue/fieldvalue.h>

namespace document {

class ArrayFieldValue;
class ByteFieldValue;
class DoubleFieldValue;
class FloatFieldValue;
class IntFieldValue;
class LongFieldValue;
class RawFieldValue;
class StringFieldValue;
class StructFieldValue;
class MapFieldValue;
class WeightedSetFieldValue;

class StructuredFieldValue : public FieldValue
{
    const DataType *_type;

    UP onGetNestedFieldValue(FieldPath::const_iterator start, FieldPath::const_iterator end) const override;

protected:
    StructuredFieldValue(const DataType &type);
    StructuredFieldValue(const StructuredFieldValue&);
    StructuredFieldValue& operator=(const StructuredFieldValue&);
    void swap(StructuredFieldValue & rhs) { std::swap(_type, rhs._type); }

    /** Called from Document when deserializing alters type. */
    virtual void setType(const DataType& type);
    const DataType &getType() const { return *_type; }

    struct StructuredIterator {
        typedef std::unique_ptr<StructuredIterator> UP;
        virtual ~StructuredIterator() {}

        virtual const Field* getNextField() = 0;
    };
    class Iterator {
        const StructuredFieldValue* _owner;
        vespalib::LinkedPtr<StructuredIterator> _iterator;
        const Field* _field;

    public:
        Iterator(); // Generate end iterator

        // Generate begin iterator
        Iterator(const StructuredFieldValue& owner, const Field* first);

        const Field &field() const { return *_field; }
        const Field &operator*() const { return field(); }
        Iterator& operator++() {
            _field = _iterator->getNextField();
            return *this;
        }
        Iterator operator++(int); // postfix
        bool operator==(const Iterator& other) const {
            if (_field == 0 && other._field == 0)
               // both at end()
               return true;
            if (_field == 0 || other._field == 0)
                // one at end()
                return false;
            return (*_field == *other._field);
        }
        bool operator!=(const Iterator& other) const
            { return !(operator==(other)); }
    };

        // Used to implement iterator
    virtual StructuredIterator::UP getIterator(const Field* toFind) const = 0;

        // As overloading doesn't work with polymorphy, have protected functions
        // doing the functionality, such that we can make utility functions here
    virtual bool hasFieldValue(const Field&) const = 0;
    virtual void removeFieldValue(const Field&) = 0;
    virtual FieldValue::UP getFieldValue(const Field&) const = 0;
    virtual bool getFieldValue(const Field& field, FieldValue& value) const = 0;
    virtual void setFieldValue(const Field&, FieldValue::UP value) = 0;
    void setFieldValue(const Field & field, const FieldValue & value);

    IteratorHandler::ModificationStatus
    onIterateNested(FieldPath::const_iterator start, FieldPath::const_iterator end, IteratorHandler & handler) const override;

public:
    DECLARE_IDENTIFIABLE_ABSTRACT(StructuredFieldValue);


    virtual StructuredFieldValue* clone() const = 0;
    const DataType *getDataType() const override { return _type; }

    /** Wrapper for DataType's hasField() function. */
    virtual bool hasField(const vespalib::stringref & name) const = 0;
    /**
     * Wrapper for DataType's getField() function.
     * @throws FieldNotFoundException If no field with given name exist.
     */
    virtual const Field& getField(const vespalib::stringref & name) const = 0;

    /**
     * Retrieve value of given field and assign it to given field.
     *
     * @return True if field is set and stored in value, false if unset.
     * @throws vespalib::IllegalArgumentException If value given has wrong type
     */
    inline bool getValue(const Field& field, FieldValue& value) const
        { return getFieldValue(field, value); }
    /** @return Retrieve value of given field. Null pointer if not set. */
    inline FieldValue::UP getValue(const Field& field) const
        { return getFieldValue(field); }
    /** @return Retrieve value of given field. Null pointer if not set. */
    inline FieldValue::UP getValue(const vespalib::stringref & name) const
        { return getFieldValue(getField(name)); }
    /** @return True if value is set. */
    inline bool hasValue(const Field& field) const
        { return hasFieldValue(field); }

    /**
     * Set the given field to contain given value.
     *
     * @throws vespalib::IllegalArgumentException If value given has wrong type
     */
    inline void setValue(const Field& field, const FieldValue& value)
        { setFieldValue(field, value); }
    /** Remove the value of given field if it is set. */
    inline void remove(const Field& field)
        { removeFieldValue(field); }

    virtual void clear() = 0;

        // Utility functions for easy but less efficient access
    bool hasValue(const vespalib::stringref & fieldName) const
        { return hasFieldValue(getField(fieldName)); }
    void remove(const vespalib::stringref & fieldName)
        { removeFieldValue(getField(fieldName)); }
    void setValue(const vespalib::stringref & fieldName, const FieldValue& value)
        { setFieldValue(getField(fieldName), value); }
    template<typename PrimitiveType>
    void set(const Field& field, PrimitiveType value);
    template<typename PrimitiveType>
    void set(const vespalib::stringref & fieldName, PrimitiveType value);

    size_t getSetFieldCount() const {
        size_t count = 0;
        for (const_iterator it(begin()), mt(end()); it != mt; ++it, ++count) {}
        return count;
    }
    virtual bool empty() const = 0;

    typedef Iterator const_iterator;
    const_iterator begin() const { return const_iterator(*this, NULL); }
    const_iterator end() const { return const_iterator(); }

    /**
     * return an iterator starting at field, or end() if field was not found
     **/
    const_iterator find(const Field& field) const {
        return const_iterator(*this, &field);
    }

    template <typename T>
    std::unique_ptr<T> getAs(const Field &field) const;
};

} // document

