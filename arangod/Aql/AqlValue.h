////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQL_VALUE_H
#define ARANGOD_AQL_AQL_VALUE_H 1

#include "Basics/Common.h"
#include "Aql/Range.h"
#include "Aql/types.h"
#include "Basics/VelocyPackHelper.h"
#include "VocBase/document-collection.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <v8.h>

struct TRI_doc_mptr_t;

namespace arangodb {
class AqlTransaction;

namespace aql {
class AqlItemBlock;

struct AqlValue final {
 friend struct std::hash<arangodb::aql::AqlValue>;
 friend struct std::equal_to<arangodb::aql::AqlValue>;

 public:

  /// @brief AqlValueType, indicates what sort of value we have
  enum AqlValueType : uint8_t { 
    VPACK_SLICE_POINTER, // contains a pointer to a vpack document, memory is not managed!
    VPACK_INLINE, // contains vpack data, inline
    VPACK_MANAGED, // contains vpack, via pointer to a managed buffer
    DOCVEC, // a vector of blocks of results coming from a subquery, managed
    RANGE // a pointer to a range remembering lower and upper bound, managed
  };

  /// @brief Holds the actual data for this AqlValue
  /// The last byte of this union (_data.internal[15]) will be used to identify 
  /// the type of the contained data:
  ///
  /// VPACK_SLICE_POINTER: data may be referenced via a pointer to a VPack slice
  /// existing somewhere in memory. The AqlValue is not responsible for managing
  /// this memory.
  /// VPACK_INLINE: VPack values with a size less than 16 bytes can be stored 
  /// directly inside the data.internal structure. All data is stored inline, 
  /// so there is no need for memory management.
  /// VPACK_MANAGED: all values of a larger size will be stored in 
  /// _data.external via a managed VPackBuffer object. The Buffer is managed
  /// by the AqlValue.
  /// DOCVEC: a managed vector of AqlItemBlocks, for storing subquery results.
  /// The vector and ItemBlocks are managed by the AqlValue
  /// RANGE: a managed range object. The memory is managed by the AqlValue
 private:
  union {
    uint8_t internal[16];
    uint8_t const* pointer;
    arangodb::velocypack::Buffer<uint8_t>* buffer;
    std::vector<AqlItemBlock*>* docvec;
    Range const* range;
  } _data;

 public:
  // construct an empty AqlValue
  // note: this is the default constructor and should be as cheap as possible
  AqlValue() noexcept {
    // construct a slice of type None
    _data.internal[0] = '\x00';
    setType(AqlValueType::VPACK_INLINE);
  }

  // construct from document
  explicit AqlValue(TRI_doc_mptr_t const* mptr) {
    _data.pointer = mptr->vpack();
    setType(AqlValueType::VPACK_SLICE_POINTER);
    TRI_ASSERT(VPackSlice(_data.pointer).isObject());
    TRI_ASSERT(!VPackSlice(_data.pointer).isExternal());
  }
  
  // construct from pointer
  explicit AqlValue(uint8_t const* pointer) {
    // we must get rid of Externals first here, because all
    // methods that use VPACK_SLICE_POINTER expect its contents
    // to be non-Externals
    if (*pointer == '\x1d') {
      // an external
      _data.pointer = VPackSlice(pointer).resolveExternals().begin();
    } else {
      _data.pointer = pointer;
    }
    setType(AqlValueType::VPACK_SLICE_POINTER);
    TRI_ASSERT(!VPackSlice(_data.pointer).isExternal());
  }
  
  // construct from docvec, taking over its ownership
  explicit AqlValue(std::vector<AqlItemBlock*>* docvec) {
    _data.docvec = docvec;
    setType(AqlValueType::DOCVEC);
  }

  // construct boolean value type
  explicit AqlValue(bool value) {
    VPackSlice slice(value ? arangodb::basics::VelocyPackHelper::TrueValue() : arangodb::basics::VelocyPackHelper::FalseValue());
    memcpy(_data.internal, slice.begin(), static_cast<size_t>(slice.byteSize()));
    setType(AqlValueType::VPACK_INLINE);
  }
  
  // construct from char* and length
  AqlValue(char const* value, size_t length) {
    if (length == 0) {
      // empty string
      _data.internal[0] = 0x40;
      setType(AqlValueType::VPACK_INLINE);
      return;
    }
    if (length < sizeof(_data.internal) - 1) {
      // short string... can store it inline
      _data.internal[0] = static_cast<uint8_t>(0x40 + length);
      memcpy(_data.internal + 1, value, length);
      setType(AqlValueType::VPACK_INLINE);
    } else if (length <= 126) {
      // short string... cannot store inline, but we don't need to
      // create a full-featured Builder object here
      _data.buffer = new arangodb::velocypack::Buffer<uint8_t>(length + 1);
      _data.buffer->push_back(static_cast<char>(0x40 + length));
      _data.buffer->append(value, length);
      setType(AqlValueType::VPACK_MANAGED);
    } else {
      // long string
      // create a big enough Buffer object
      auto buffer = std::make_unique<VPackBuffer<uint8_t>>(8 + length);
      // add string to Builder
      VPackBuilder builder(*buffer.get());
      builder.add(VPackValuePair(value, length, VPackValueType::String));
      // steal Buffer. now we have ownership
      _data.buffer = buffer.release();
      setType(AqlValueType::VPACK_MANAGED);
    }
  }
  
  // construct from std::string
  explicit AqlValue(std::string const& value) : AqlValue(value.c_str(), value.size()) {}
  
  // construct from Buffer, taking over its ownership
  explicit AqlValue(arangodb::velocypack::Buffer<uint8_t>* buffer) {
    _data.buffer = buffer;
    setType(AqlValueType::VPACK_MANAGED);
  }
  
  // construct from Builder
  explicit AqlValue(arangodb::velocypack::Builder const& builder) {
    TRI_ASSERT(builder.isClosed());
    initFromSlice(builder.slice());
  }
  
  explicit AqlValue(arangodb::velocypack::Builder const* builder) {
    TRI_ASSERT(builder->isClosed());
    initFromSlice(builder->slice());
  }

  // construct from Slice
  explicit AqlValue(arangodb::velocypack::Slice const& slice) {
    initFromSlice(slice);
  }
  
  // construct range type
  AqlValue(int64_t low, int64_t high) {
    _data.range = new Range(low, high);
    setType(AqlValueType::RANGE);
  }
 
  /// @brief AqlValues can be copied and moved as required
  /// memory management is not performed via AqlValue destructor but via 
  /// explicit calls to destroy()
  AqlValue(AqlValue const&) noexcept = default; 
  AqlValue& operator=(AqlValue const&) noexcept = default; 
  AqlValue(AqlValue&&) noexcept = default; 
  AqlValue& operator=(AqlValue&&) noexcept = default; 

  ~AqlValue() = default;
  
  /// @brief whether or not the value must be destroyed
  inline bool requiresDestruction() const noexcept {
    AqlValueType t = type();
    return (t == VPACK_MANAGED || t == DOCVEC || t == RANGE);
  }

  /// @brief whether or not the value is empty / none
  inline bool isEmpty() const noexcept { 
    if (type() != VPACK_INLINE) {
      return false;
    }
    return arangodb::velocypack::Slice(_data.internal).isNone();
  }
  
  /// @brief whether or not the value is a pointer
  inline bool isPointer() const noexcept {
    return type() == VPACK_SLICE_POINTER;
  }
  
  /// @brief whether or not the value is a range
  inline bool isRange() const noexcept {
    return type() == RANGE;
  }
  
  /// @brief whether or not the value is a docvec
  inline bool isDocvec() const noexcept {
    return type() == DOCVEC;
  }

  /// @brief hashes the value
  uint64_t hash(arangodb::AqlTransaction*, uint64_t seed = 0xdeadbeef) const;

  /// @brief whether or not the value contains a none value
  bool isNone() const;
  
  /// @brief whether or not the value contains a null value
  bool isNull(bool emptyIsNull) const;

  /// @brief whether or not the value contains a boolean value
  bool isBoolean() const;

  /// @brief whether or not the value is a number
  bool isNumber() const;
  
  /// @brief whether or not the value is a string
  bool isString() const;
  
  /// @brief whether or not the value is an object
  bool isObject() const;
  
  /// @brief whether or not the value is an array (note: this treats ranges
  /// as arrays, too!)
  bool isArray() const;
  
  /// @brief get the (array) length (note: this treats ranges as arrays, too!)
  size_t length() const;
  
  /// @brief get the (array) element at position 
  AqlValue at(int64_t position, bool& mustDestroy, bool copy) const;
  
  /// @brief get the _key attribute from an object/document
  AqlValue getKeyAttribute(arangodb::AqlTransaction* trx,
                           bool& mustDestroy, bool copy) const;
  /// @brief get the _id attribute from an object/document
  AqlValue getIdAttribute(arangodb::AqlTransaction* trx,
                          bool& mustDestroy, bool copy) const;
  /// @brief get the _from attribute from an object/document
  AqlValue getFromAttribute(arangodb::AqlTransaction* trx,
                            bool& mustDestroy, bool copy) const;
  /// @brief get the _to attribute from an object/document
  AqlValue getToAttribute(arangodb::AqlTransaction* trx,
                          bool& mustDestroy, bool copy) const;
  
  /// @brief get the (object) element by name(s)
  AqlValue get(arangodb::AqlTransaction* trx,
               std::string const& name, bool& mustDestroy, bool copy) const;
  AqlValue get(arangodb::AqlTransaction* trx,
               std::vector<std::string> const& names, bool& mustDestroy,
               bool copy) const;
  bool hasKey(arangodb::AqlTransaction* trx, std::string const& name) const;

  /// @brief get the numeric value of an AqlValue
  double toDouble() const;
  double toDouble(bool& failed) const;
  int64_t toInt64() const;
  
  /// @brief whether or not an AqlValue evaluates to true/false
  bool toBoolean() const;
  
  /// @brief return the range value
  Range const* range() const {
    TRI_ASSERT(isRange());
    return _data.range;
  }
  
  /// @brief return the total size of the docvecs
  size_t docvecSize() const;
  
  /// @brief return the item block at position
  AqlItemBlock* docvecAt(size_t position) const {
    TRI_ASSERT(isDocvec());
    return _data.docvec->at(position);
  }
  
  /// @brief construct a V8 value as input for the expression execution in V8
  /// only construct those attributes that are needed in the expression
  v8::Handle<v8::Value> toV8Partial(v8::Isolate* isolate,
                                    arangodb::AqlTransaction*,
                                    std::unordered_set<std::string> const&) const;
  
  /// @brief construct a V8 value as input for the expression execution in V8
  v8::Handle<v8::Value> toV8(v8::Isolate* isolate, arangodb::AqlTransaction*) const;

  /// @brief materializes a value into the builder
  void toVelocyPack(arangodb::AqlTransaction*,
                    arangodb::velocypack::Builder& builder,
                    bool resolveExternals) const;

  /// @brief materialize a value into a new one. this expands docvecs and 
  /// ranges
  AqlValue materialize(arangodb::AqlTransaction*, bool& hasCopied,
                       bool resolveExternals) const;

  /// @brief return the slice for the value
  /// this will throw if the value type is not VPACK_SLICE_POINTER, VPACK_INLINE or
  /// VPACK_MANAGED
  arangodb::velocypack::Slice slice() const;
  
  /// @brief clone a value
  AqlValue clone() const;
  
  /// @brief invalidates/resets a value to None, not freeing any memory
  void erase() noexcept {
    _data.internal[0] = '\x00';
    setType(AqlValueType::VPACK_INLINE);
  }
  
  /// @brief destroy, explicit destruction, only when needed
  void destroy();

  /// @brief create an AqlValue from a vector of AqlItemBlock*s
  static AqlValue CreateFromBlocks(arangodb::AqlTransaction*,
                                    std::vector<AqlItemBlock*> const&,
                                    std::vector<std::string> const&);

  /// @brief create an AqlValue from a vector of AqlItemBlock*s
  static AqlValue CreateFromBlocks(arangodb::AqlTransaction*,
                                    std::vector<AqlItemBlock*> const&,
                                    arangodb::aql::RegisterId);
  
  /// @brief compare function for two values
  static int Compare(arangodb::AqlTransaction*, 
                     AqlValue const& left, AqlValue const& right, bool useUtf8);

 private:
  
  /// @brief Returns the type of this value. If true it uses an external pointer
  /// if false it uses the internal data structure
  inline AqlValueType type() const noexcept {
    return static_cast<AqlValueType>(_data.internal[sizeof(_data.internal) - 1]);
  }
  
  /// @brief initializes value from a slice
  void initFromSlice(arangodb::velocypack::Slice const& slice) {
    // intentionally do not resolve externals here
    // if (slice.isExternal()) {
    //   // recursively resolve externals
    //   slice = slice.resolveExternals();
    // }
    arangodb::velocypack::ValueLength length = slice.byteSize();
    if (length < sizeof(_data.internal)) {
      // Use inline value
      memcpy(_data.internal, slice.begin(), static_cast<size_t>(length));
      setType(AqlValueType::VPACK_INLINE);
    } else {
      // Use managed buffer
      _data.buffer = new arangodb::velocypack::Buffer<uint8_t>(length);
      _data.buffer->append(reinterpret_cast<char const*>(slice.begin()), length);
      setType(AqlValueType::VPACK_MANAGED);
    }
  }

  /// @brief sets the value type
  inline void setType(AqlValueType type) noexcept {
    _data.internal[sizeof(_data.internal) - 1] = type;
  }
};

class AqlValueGuard {
 public:
  AqlValueGuard() = delete;
  AqlValueGuard(AqlValueGuard const&) = delete;
  AqlValueGuard& operator=(AqlValueGuard const&) = delete;

  AqlValueGuard(AqlValue& value, bool destroy) : value(value), destroy(destroy) {}
  ~AqlValueGuard() { 
    if (destroy) { 
      value.destroy();
    } 
  }
  void steal() { destroy = false; }
 private:
  AqlValue& value;
  bool destroy;
};

struct AqlValueMaterializer {
  explicit AqlValueMaterializer(arangodb::AqlTransaction* trx) 
      : trx(trx), materialized(), hasCopied(false) {}

  AqlValueMaterializer(AqlValueMaterializer const& other) 
      : trx(other.trx), materialized(other.materialized), hasCopied(other.hasCopied) {
    if (other.hasCopied) {
      // copy other's slice
      materialized = other.materialized.clone();
    }
  }
  
  AqlValueMaterializer& operator=(AqlValueMaterializer const& other) {
    if (this != &other) {
      TRI_ASSERT(trx == other.trx); // must be from same transaction
      if (hasCopied) {
        // destroy our own slice
        materialized.destroy();
        hasCopied = false;
      }
      // copy other's slice
      materialized = other.materialized.clone();
      hasCopied = other.hasCopied;
    }
    return *this;
  }
  
  AqlValueMaterializer(AqlValueMaterializer&& other) noexcept 
      : trx(other.trx), materialized(other.materialized), hasCopied(other.hasCopied) {
    // reset other
    other.hasCopied = false;
    other.materialized = AqlValue();
  }
  
  AqlValueMaterializer& operator=(AqlValueMaterializer&& other) noexcept {
    if (this != &other) {
      TRI_ASSERT(trx == other.trx); // must be from same transaction
      if (hasCopied) {
        // destroy our own slice
        materialized.destroy();
      }
      // reset other
      materialized = other.materialized;
      hasCopied = other.hasCopied;
      other.materialized = AqlValue();
    }
    return *this;
  }

  ~AqlValueMaterializer() { 
    if (hasCopied) { 
      materialized.destroy(); 
    } 
  }

  arangodb::velocypack::Slice slice(AqlValue const& value,
                                    bool resolveExternals) {
    materialized = value.materialize(trx, hasCopied, resolveExternals);
    return materialized.slice();
  }

  arangodb::AqlTransaction* trx;
  AqlValue materialized;
  bool hasCopied;
};

static_assert(sizeof(AqlValue) == 16, "invalid AqlValue size");

}  // closes namespace arangodb::aql
}  // closes namespace arangodb

/// @brief hash function for AqlValue objects
namespace std {

template <>
struct hash<arangodb::aql::AqlValue> {
  size_t operator()(arangodb::aql::AqlValue const& x) const {
    std::hash<uint8_t> intHash;
    std::hash<void const*> ptrHash;
    arangodb::aql::AqlValue::AqlValueType type = x.type();
    size_t res = intHash(type);
    switch (type) {
      case arangodb::aql::AqlValue::VPACK_SLICE_POINTER: { 
        return res ^ ptrHash(x._data.pointer);
      }
      case arangodb::aql::AqlValue::VPACK_INLINE: {
        return res ^ static_cast<size_t>(arangodb::velocypack::Slice(&x._data.internal[0]).hash());
      }
      case arangodb::aql::AqlValue::VPACK_MANAGED: {
        return res ^ ptrHash(x._data.buffer);
      }
      case arangodb::aql::AqlValue::DOCVEC: {
        return res ^ ptrHash(x._data.docvec);
      }
      case arangodb::aql::AqlValue::RANGE: {
        return res ^ ptrHash(x._data.range);
      }
    }

    TRI_ASSERT(false);
    return 0;
  }
};

template <>
struct equal_to<arangodb::aql::AqlValue> {
  bool operator()(arangodb::aql::AqlValue const& a,
                  arangodb::aql::AqlValue const& b) const {
    arangodb::aql::AqlValue::AqlValueType type = a.type();
    if (type != b.type()) {
      return false;
    }
    switch (type) {
      case arangodb::aql::AqlValue::VPACK_SLICE_POINTER: {
        return a._data.pointer == b._data.pointer;
      }
      case arangodb::aql::AqlValue::VPACK_INLINE: {
        return arangodb::velocypack::Slice(&a._data.internal[0]).equals(arangodb::velocypack::Slice(&b._data.internal[0]));
      }
      case arangodb::aql::AqlValue::VPACK_MANAGED: {
        return a._data.buffer == b._data.buffer;
      }
      case arangodb::aql::AqlValue::DOCVEC: {
        return a._data.docvec == b._data.docvec;
      }
      case arangodb::aql::AqlValue::RANGE: {
        return a._data.range == b._data.range;
      }
    }
    
    TRI_ASSERT(false);
    return false;
  }
};

}  // closes namespace std

#endif
