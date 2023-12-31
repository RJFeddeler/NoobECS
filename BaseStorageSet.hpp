//
//  BaseStorageSet.hpp
//  PebbleEngine
//
//  Created by Robert Feddeler on 8/30/22.
//

#pragma once

#include "../Core/PebbleCom.hpp"


namespace pebble {

static constexpr size_t minPageSize         = 8;
static constexpr size_t densePageSizeMax    = maxValue<uint16_t>();
static constexpr size_t defaultPageSize     = 4096;
static constexpr size_t defaultPageCountMax = 16;

template <typename Key, size_t keyPrefixBitCount>
struct BaseKeyInfo;

template <typename Key, size_t keyPrefixBitCount, typename Type>
class StorageSet;



template <typename Key, size_t keyPrefixBitCount>
class BaseStorageSet {
protected:
  using MyBaseKeyInfo = BaseKeyInfo<Key, keyPrefixBitCount>;
  using BaseKey = typename MyBaseKeyInfo::type;
  static constexpr BaseKey NullKey = MyBaseKeyInfo::NullKey;
  static constexpr auto baseIdentifier = MyBaseKeyInfo::baseIdentifier;

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

  virtual void clear() = 0;

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
  Key totalCount() { return dense_.size();    }

  auto keyBegin()  { return dense_.cbegin();  }
  auto keyEnd()    { return dense_.cend();    }

  constexpr bool contains(Key key);
  virtual void remove(Key key);

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


template <typename Key, size_t keyPrefixBitCount>
constexpr bool BaseStorageSet<Key, keyPrefixBitCount>::
contains(Key key) {
  auto dPos = densePosFromKey(key);

  if (dPos != NullKey && dPos < dense_.size() && dense_[dPos] == key)
    return true;
  else
    return false;
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

template <typename Key, size_t keyPrefixBitCount>
void BaseStorageSet<Key, keyPrefixBitCount>::
resizeContainersForKey(size_t page, size_t offset) {
  if (page >= sparse_.capacity()) {
    assert(page < pageCountMax_ &&
           "Cannot reserve page(s), index out of range!");

    sparse_.reserve(nextPow2(std::max(page + 1, size_t(1))));
  }

  if (page >= sparse_.size()) {
    assert(page < pageCountMax_ &&
           "Cannot create page(s), index out of range!");

    sparse_.resize(std::max(page + 1, size_t(1)));
    pageCount_ = sparse_.size();
  }

  if (!sparse_[page]) {
    sparse_[page] =
      std::make_unique<std::vector<BaseKey>>(minPageSize, NullKey);
  }

  if (offset >= sparse_[static_cast<uint32_t>(page)]->capacity()) {
    assert(offset < pageSize_ &&
           "Cannot reserve page size, index out of range!");

    sparse_[page]->reserve(nextPow2(offset + 1));
  }

  if (offset >= sparse_[page]->size()) {
    assert(offset < pageSize_ &&
           "Cannot set page size, index out of range!");

    sparse_[page]->resize(offset + 1, NullKey);
  }
}



template <typename Key, size_t keyPrefixBitCount>
struct BaseKeyInfo {
  using type = typename std::conditional<
    (sizeof(Key) * 8 - keyPrefixBitCount) <= 16,
    uint16_t,
    typename std::conditional<
      (sizeof(Key) * 8 - keyPrefixBitCount) <= 32, uint32_t, uint64_t
    >::type
  >::type;

  static constexpr type NullKey = static_cast<type>(
      maxValue<Key>() >> keyPrefixBitCount);

  static constexpr type baseIdentifier(Key key) {
    return static_cast<type>(
        key & (maxValue<Key>() >> keyPrefixBitCount));
  }
};

}
