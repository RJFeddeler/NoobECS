//
//  StorageSet.hpp
//  PebbleEngine
//
//  Created by Robert Feddeler on 7/30/22.
//

#pragma once

#include "../Core/PebbleCom.hpp"
#include "BaseStorageSet.hpp"


namespace pebble {

template <typename Key, size_t keyPrefixBitCount, typename Type>
class StorageSet : public BaseStorageSet<Key, keyPrefixBitCount> {
public:
  StorageSet(size_t pageSize = defaultPageSize,
             size_t pageCountMax  = defaultPageCountMax)
      : BaseStorageSet<Key, keyPrefixBitCount>(pageSize, pageCountMax),
        storageType_{[]() -> const std::type_info& { return typeid(Type); }}
        {}

  virtual ~StorageSet() = default;

  virtual const std::type_info& storageType() override {
    return storageType_();
  }

  virtual void clear() override {
    storage_.clear();
  }

  // returns Type* to element in storage (deref from smart ptr)
  template <typename T = Type>
  typename std::enable_if_t<is_smart_ptr<T>::value, typename std::add_pointer_t<
  typename std::remove_reference_t<T>::element_type>>
  get(Key key);

  // returns Type* to element in storage
  template <typename T = Type>
  typename std::enable_if_t<!is_smart_ptr<T>::value,
  typename std::add_pointer_t<T>>
  get(Key key);

  template <typename T = Type>
  void set(Key key, typename std::enable_if_t<is_smart_ptr<T>::value,
           typename std::add_pointer_t<
           typename std::remove_reference_t<T>::element_type>> data);

  template <typename T = Type>
  void set(Key key, typename std::enable_if_t<!is_smart_ptr<T>::value,
           typename std::add_lvalue_reference_t<typename std::add_const_t<
           typename std::remove_reference_t<T>>>> data);

  template <typename T = Type>
  void add(Key key, typename std::enable_if_t<is_smart_ptr<T>::value,
           typename std::add_pointer_t<
           typename std::remove_reference_t<T>::element_type>>
           data);

  template <typename T = Type>
  void add(Key key, typename std::enable_if_t<!is_smart_ptr<T>::value,
           typename std::add_lvalue_reference_t<
           typename std::add_const_t<
           typename std::remove_reference_t<T>>>>
           data);

  template <typename T = Type>
  void add(Key key, typename std::enable_if_t<!is_smart_ptr<T>::value,
           typename std::add_rvalue_reference_t<
           typename std::remove_reference_t<T>>>
           data);

private:
  size_t add(Key key);

public:
  auto begin()  { return storage_.begin();  }
  auto cbegin() { return storage_.cbegin(); }
  auto end()    { return storage_.end();    }
  auto cend()   { return storage_.cend();   }

private:
  const std::type_info& (*storageType_)();
  std::vector<Type> storage_;
};


// returns const Type& reference to element in storage (deref from smart ptr)
template <typename Key, size_t keyPrefixBitCount, typename Type>
template <typename T>
typename std::enable_if_t<is_smart_ptr<T>::value,
typename std::add_pointer_t<
typename std::remove_reference_t<T>::element_type>>
StorageSet<Key, keyPrefixBitCount, Type>::
get(Key key) {
  return this->contains(key) ?
      storage_[this->densePosFromKey(key)].get() : nullptr;
}

// returns const Type& reference to element in storage
template <typename Key, size_t keyPrefixBitCount, typename Type>
template <typename T>
typename std::enable_if_t<!is_smart_ptr<T>::value,
typename std::add_pointer_t<T>>
StorageSet<Key, keyPrefixBitCount, Type>::
get(Key key) {
  return this->contains(key) ? &storage_[this->densePosFromKey(key)] : nullptr;
}

template <typename Key, size_t keyPrefixBitCount, typename Type>
template <typename T>
void StorageSet<Key, keyPrefixBitCount, Type>::
set(Key key,
    typename std::enable_if_t<is_smart_ptr<T>::value,
        typename std::add_pointer_t<
        typename std::remove_reference_t<T>::element_type>> data) {

  if (contains(key))
    storage_[densePosFromKey(key)] = T(data);
  else
    add(key, data);
}

template <typename Key, size_t keyPrefixBitCount, typename Type>
template <typename T>
void StorageSet<Key, keyPrefixBitCount, Type>::
set(Key key,
    typename std::enable_if_t<!is_smart_ptr<T>::value,
        typename std::add_lvalue_reference_t<typename std::add_const_t<
        typename std::remove_reference_t<T>>>> data) {

  if (this->contains(key))
    storage_[this->densePosFromKey(key)] = data;
  else
    add(key, data);
}

template <typename Key, size_t keyPrefixBitCount, typename Type>
template <typename T>
void StorageSet<Key, keyPrefixBitCount, Type>::
add(Key key,
    typename std::enable_if_t<is_smart_ptr<T>::value,
        typename std::add_pointer_t<
        typename std::remove_reference_t<T>::element_type>> data) {

  auto pos = add(key);

  if (pos != maxValue<decltype(pos)>()) {
    assert(pos <= storage_.size() &&
      "Add reported invalid index!");

    if (pos == storage_.size())
      storage_.push_back(T(data));
    else
      storage_[pos] = T(data);
  }
}

template <typename Key, size_t keyPrefixBitCount, typename Type>
template <typename T>
void StorageSet<Key, keyPrefixBitCount, Type>::
add(Key key,
    typename std::enable_if_t<!is_smart_ptr<T>::value,
        typename std::add_lvalue_reference_t<typename std::add_const_t<
        typename std::remove_reference_t<T>>>> data) {

  auto pos = add(key);

  if (pos != maxValue<decltype(pos)>()) {
    assert(pos <= storage_.size() &&
      "Add reported invalid index!");

    if (pos == storage_.size())
      storage_.push_back(data);
    else
      storage_[pos] = data;
  }
}

template <typename Key, size_t keyPrefixBitCount, typename Type>
template <typename T>
void StorageSet<Key, keyPrefixBitCount, Type>::
add(Key key,
    typename std::enable_if_t<!is_smart_ptr<T>::value,
        typename std::add_rvalue_reference_t<
        typename std::remove_reference_t<T>>> data) {

  auto pos = add(key);

  if (pos != maxValue<decltype(pos)>()) {
    assert(pos <= storage_.size() &&
      "Add reported invalid index!");

    if (pos == storage_.size())
      storage_.push_back(std::move(data));
    else
      storage_[pos] = std::move(data);
  }
}

template <typename Key, size_t keyPrefixBitCount, typename Type>
size_t StorageSet<Key, keyPrefixBitCount, Type>::
add(Key key) {
  auto position = maxValue<size_t>();

  if (!this->contains(key)) {
    auto [page, offset] = this->pageAndOffsetFromKey(key);
    this->resizeContainersForKey(page, offset);

    if (this->recyclingCount_ == 0) {
      assert(this->dense_.size() < densePageSizeMax &&
             "Cannot add item, dense vector is full!");

      position = this->dense_.size();
      (*this->sparse_[page])[offset] =
          static_cast<typename BaseStorageSet<Key, keyPrefixBitCount>::BaseKey>(
              this->dense_.size());
      this->dense_.push_back(key);
    }
    else {
      auto dPos = this->recyclingHead_;
      this->recyclingHead_ =
          static_cast<typename BaseStorageSet<Key, keyPrefixBitCount>::BaseKey>(
              this->dense_[this->recyclingHead_]);
      --this->recyclingCount_;

      position = dPos;
      (*this->sparse_[page])[offset] = dPos;
      this->dense_[dPos] = key;
    }
  }

  return position;
}

template <typename Key, size_t keyPrefixBitCount, typename Type>
StorageSet<Key, keyPrefixBitCount, Type>*
storage_cast(BaseStorageSet<Key, keyPrefixBitCount>* basePtr) {

  if (basePtr && typeid(Type) == basePtr->storageType() &&
      typeid(Key) == basePtr->keyType()) {

    return static_cast<StorageSet<Key, keyPrefixBitCount, Type>*>(basePtr);
  }
  else
    return nullptr;
}

}
