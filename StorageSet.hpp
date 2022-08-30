//
//  StorageSet.hpp
//  BasaltEngine
//
//  Created by Robert Feddeler on 7/30/22.
//

#pragma once

#include "../BasaltCom.hpp"


namespace basalt::data {

using Entity = uint64_t; // YUCK, FIGURE THIS OUT...

static constexpr size_t minPageSize         = 8;
static constexpr size_t densePageSizeMax    = maxValue<uint16_t>();
static constexpr size_t defaultPageSize     = 4096;
static constexpr size_t defaultPageCountMax = 16;

template<typename T>
struct is_unique_ptr : std::false_type {};

template<typename T>
struct is_unique_ptr<std::unique_ptr<T>> : std::true_type {};

template<typename T>
struct is_shared_ptr : std::false_type {};

template<typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

template<typename T>
struct is_smart_ptr : std::conditional_t<
  !is_unique_ptr<T>::value && !is_shared_ptr<T>::value,
  std::false_type,
  std::true_type> {};



template <typename Key, size_t keyPrefixBitCount, typename Type>
class StorageSet;

template <typename Key, size_t keyPrefixBitCount>
class BaseStorageSet {
protected:
  using BaseKey = typename std::conditional<
    (sizeof(Key) * 8 - keyPrefixBitCount) <= 16,
    uint16_t,
    typename std::conditional<
      (sizeof(Key) * 8 - keyPrefixBitCount) <= 32, uint32_t, uint64_t>::type
  >::type;
  
  static constexpr BaseKey NullKey = static_cast<BaseKey>(
      maxValue<Key>() >> keyPrefixBitCount);
  
  static constexpr BaseKey baseIdentifier(Key key) {
    return static_cast<BaseKey>(
        key & (maxValue<Key>() >> keyPrefixBitCount));
  }
  
public:
  BaseStorageSet(size_t pageSize = defaultPageSize,
                 size_t pageCountMax  = defaultPageCountMax)
    : keyType_{[]() -> const std::type_info& { return typeid(Key); }},
      pageSize_(std::max(nextPow2(pageSize), minPageSize)),
      pageCountMax_(pageCountMax),
      sparse_(1) {}
  
  virtual ~BaseStorageSet() = default;
  
  const std::type_info& keyType() { return keyType_(); }
  virtual const std::type_info& storageType() = 0;
  
  BaseKey densePosFromKey(size_t page, size_t offset) {
    if (page < pageCount_ && sparse_[page] && offset < sparse_[page]->size())
      return (*sparse_[page])[offset];
    else
      return NullKey;
  }
  
  BaseKey densePosFromKey(Key key) {
    auto [page, offset] = pageAndOffsetFromKey(key);
    return densePosFromKey(page, offset);
  }
  
  Key validCount() { return dense_.size() - recyclingCount_; }
  Key totalCount() { return dense_.size(); }
  
  auto keyBegin()  { return dense_.cbegin();  }
  auto keyEnd()    { return dense_.cend();    }
  
  constexpr bool contains(Key key);
  virtual void remove(Key key);
  
  virtual void debugCheckContents() = 0; //Delete when done testing
  
protected:
  void resizeContainersForKey(size_t page, size_t offset);
  
  size_t pageFromKey(Key key) {
    return baseIdentifier(key) / pageSize_;
  }
  
  size_t pageOffsetFromKey(Key key) {
    return modPow2(baseIdentifier(key), pageSize_);
  }
  
  std::pair<size_t, size_t> pageAndOffsetFromKey(Key key) {
    return std::make_pair(pageFromKey(key), pageOffsetFromKey(key));
  }
  
protected:
  const std::type_info& (*keyType_)();
  
  const size_t pageSize_;
  const size_t pageCountMax_;
  
  size_t pageCount_ = 1;
  
  BaseKey recyclingHead_ = NullKey;
  size_t recyclingCount_ = 0;
  
  std::vector<std::unique_ptr<std::vector<BaseKey>>> sparse_;
  std::vector<Key>  dense_;
};



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
  
  // returns const Type& reference to element in storage (deref from smart ptr)
  template <typename T = Type>
  typename std::enable_if_t<is_smart_ptr<T>::value, typename std::add_pointer_t<
  typename std::remove_reference_t<T>::element_type>>
  get(Key key);
  
  // returns const Type& reference to element in storage
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
  
  //Delete when done testing
  virtual void debugCheckContents() override {
    /*
    const auto& dStorageType = std::string(storageType().name());
    
    auto recyclingCount = recyclingCount_;
    auto recyclingHead = recyclingHead_;
    
    const auto& dense = dense_;
    const auto& storage = storage_;
    
    std::vector<std::vector<BaseKey>*> vecSparse;
    for (int i = 0; i < sparse_.size(); ++i)
      if (sparse_[i])
        vecSparse.push_back(sparse_[i].get());
    
    int pauseHere = 0;
    */
  }
  
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



template <typename Key, size_t keyPrefixBitCount>
constexpr bool BaseStorageSet<Key, keyPrefixBitCount>::
contains(Key key) {
  auto dPos = densePosFromKey(key);
  
  if (dPos != NullKey && dPos < dense_.size() && dense_[dPos] == key)
    return true;
  else
    return false;
}

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
    assert (pos <= storage_.size() && "Add reported invalid index!");
    
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
    assert (pos <= storage_.size() && "Add reported invalid index!");
    
    if (pos == storage_.size())
      storage_.push_back(data);
    else
      storage_[pos] = data;
  }
}

template <typename Key, size_t keyPrefixBitCount>
void BaseStorageSet<Key, keyPrefixBitCount>::
remove(Key key) {
  if (contains(key)) {
    auto [page, offset] = pageAndOffsetFromKey(key);
    auto dPos = densePosFromKey(page, offset);
    
    dense_[dPos] = recyclingHead_;
    recyclingHead_ = dPos;
    ++recyclingCount_;
    
    (*sparse_[page])[offset] = NullKey;
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

template <typename Key, size_t keyPrefixBitCount>
void BaseStorageSet<Key, keyPrefixBitCount>::
resizeContainersForKey(size_t page, size_t offset) {
  if (page >= sparse_.capacity()) {
    assert(page < pageCountMax_ &&
           "Cannot reserve page(s), index out of range!");
    
    sparse_.reserve(nextPow2(std::max(page, size_t(1))));
  }
  
  if (page >= sparse_.size()) {
    assert(page < pageCountMax_ &&
           "Cannot create page(s), index out of range!");
    
    sparse_.resize(std::max(page, size_t(1)));
    pageCount_ = sparse_.size();
  }
  
  if (!sparse_[page]) {
    sparse_[page] =
        std::make_unique<std::vector<BaseKey>>(minPageSize, NullKey);
  }
  
  if (offset >= sparse_[static_cast<uint32_t>(page)]->capacity()) {
    assert(offset < pageSize_ &&
           "Cannot reserve page size, index out of range!");
    
    sparse_[page]->reserve(nextPow2(offset));
  }
  
  if (offset >= sparse_[page]->size()) {
    assert(offset < pageSize_ &&
           "Cannot set page size, index out of range!");
    
    sparse_[page]->resize(offset, NullKey);
  }
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
