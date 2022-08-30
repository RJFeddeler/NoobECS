//
//  Registry.hpp
//  BasaltEngine
//
//  Created by Robert Feddeler on 7/30/22.
//

#pragma once

#include "../BasaltCom.hpp"
#include "StorageSet.hpp"


namespace basalt::data {

using Entity = uint64_t;
using EntityID = uint32_t;
using EntityGeneration = uint32_t;
using Component = EntityID;

constexpr size_t generationBitCount = 32;
constexpr size_t identifierBitCount = (sizeof(Entity) * 8) - generationBitCount;

template <typename T>
using ComponentStorageSet = StorageSet<Entity, generationBitCount, T>;

struct indexGenerator {
  static Component getNextIndex() {
    static std::atomic<Component> index = 0;
    return index++;
  }
};

template <typename T>
struct uniqueIndex {
  static Component get() {
    static const auto index = indexGenerator::getNextIndex();
    
    return index;
  }
  
  constexpr operator Component() const { return get(); }
};



static constexpr EntityID entityIdentifier(Entity entity) {
  return entity & (maxValue<Entity>() >> generationBitCount);
}

static constexpr EntityGeneration entityGeneration(Entity entity) {
  return entity >> identifierBitCount;
}

static constexpr Entity entityCombine(EntityID id, EntityGeneration gen) {
  return (Entity)gen << identifierBitCount | id;
}

static constexpr Entity NullEntity = entityCombine(maxValue<EntityID>(), 0);


class Registry {
public:
  Entity createEntity(bool recycleIfAvailable = true);
  Entity recycleEntity();
  void deleteEntity(Entity entity);
  
  template <typename T>
  ComponentStorageSet<T>* createComponentStorage();
  
  template <typename T>
  ComponentStorageSet<T>* getComponentStorage();
  
  template <typename T>
  T* getComponent(Entity entity);
  
  template <typename T>
  void setComponent(Entity entity, const T& data);
  
  template <typename T>
  void addComponent(Entity entity);
  
  template <typename T>
  void removeComponent(Entity entity);
  
private:
  template <size_t k, typename T>
  size_t smallestComponent();
  
  template <size_t k, typename T, typename... Ts>
  size_t smallestComponent();
  
  template <typename T>
  size_t smallestComponent();
  
  template <typename T, typename... Ts>
  size_t smallestComponent();
  
  template <typename T>
  void getEntities(Component index, std::vector<Entity>& list);
  
  template <typename T, typename... Ts>
  std::enable_if_t<(sizeof...(Ts) > 0), void>
  getEntities(Component index, std::vector<Entity>& list);
  
  template <typename T, typename... Ts>
  std::vector<Entity> getEntities(Component index);
  
  template <typename T>
  void removeUncommonEntities(std::vector<Entity>& list);
  
  template <typename T, typename... Ts>
  std::enable_if_t<(sizeof...(Ts) > 0), void>
  removeUncommonEntities(std::vector<Entity>& list);
  
public:
  template <typename T, typename Functor>
  void forEach(Functor&& f);
  
  template <typename... Ts, typename Functor>
  std::enable_if_t<(sizeof...(Ts) > 1), void> forEach(Functor&& f);
  
  
  //Delete when done testing
  void debugCheckContents() {
    components_.debugCheckContents();
    
    std::for_each(components_.begin(),
                  components_.end(),
                  [](auto& component){
      
      component->debugCheckContents();
    });
  }
  
private:
  template <typename T>
  void componentList(std::vector<Component>& list);
  
  template <typename T, typename... Ts>
  std::enable_if_t<sizeof...(Ts), void>
  componentList(std::vector<Component>& list);
  
  template <typename T, typename... Ts>
  std::vector<Component> componentList();
  
  std::pair<Component, size_t>
  shortestComponent(const std::vector<Component>& keyList) {
    std::pair<Component, size_t> shortest = { 0, maxValue<size_t>() };
    
    std::for_each(keyList.begin(), keyList.end(), [&](auto key){
      auto ptr = components_.get(key);
      if (ptr) {
        auto count = ptr->validCount();
        if (count < shortest.second) {
          shortest.first = key;
          shortest.second = count;
        }
      }
    });
    
    return shortest;
  }
  
private:
  StorageSet<Component, 0,
      std::unique_ptr<BaseStorageSet<Entity, generationBitCount>>> components_;
  
  std::vector<Entity> entities_;
  EntityID            entityRecyclingHead_ = entityIdentifier(NullEntity);
  size_t              entityRecyclingCount_ = 0;
};

template <typename T>
ComponentStorageSet<T>* Registry::createComponentStorage() {
  components_.add(uniqueIndex<T>(), new ComponentStorageSet<T>());
  return getComponentStorage<T>();
}

template <typename T>
ComponentStorageSet<T>* Registry::getComponentStorage() {
  auto ptr = components_.get(uniqueIndex<T>());
  auto ret = storage_cast<Entity, generationBitCount, T>(ptr);
  return ret ? ret : nullptr;
}

template <typename T>
T* Registry::getComponent(Entity entity) {
  auto ptr = getComponentStorage<T>();
  return ptr ? ptr->get(entity) : nullptr;
}

template <typename T>
void Registry::setComponent(Entity entity, const T& data) {
  auto ptr = getComponentStorage<T>();
  
  if (!ptr)
    ptr = createComponentStorage<T>();
  
  if (ptr)
    ptr->set(entity, data);
}

template <typename T>
void Registry::addComponent(Entity entity) {
  auto ptr = getComponentStorage<T>();
  
  if (!ptr)
    ptr = createComponentStorage<T>();
  
  if (ptr)
    ptr->add(entity, {});
}

template <typename T>
void Registry::removeComponent(Entity entity) {
  auto ptr = getComponentStorage<T>();
  if (ptr)
    ptr->remove(entity);
}


template <typename T>
void Registry::componentList(std::vector<Component>& list) {
  list.push_back(uniqueIndex<T>());
}

template <typename T, typename... Ts>
std::enable_if_t<sizeof...(Ts), void>
Registry::componentList(std::vector<Component>& list) {
  list.push_back(uniqueIndex<T>());
  componentList<Ts...>(list);
}

template <typename T, typename... Ts>
std::vector<Component> Registry::componentList() {
  std::vector<Component> list;
  componentList<T, Ts...>(list);
  return list;
}

template <typename T>
void Registry::getEntities(Component index, std::vector<Entity>& list) {
  if (uniqueIndex<T>() == index) {
    auto ptr = components_.get(index);
    if (ptr) {
      std::for_each(ptr->keyBegin(), ptr->keyEnd(), [&list, ptr](auto& e) {
        if (ptr->contains(e))
          list.push_back(e);
      });
    }
  }
}

template <typename T, typename... Ts>
std::enable_if_t<(sizeof...(Ts) > 0), void>
Registry::getEntities(Component index, std::vector<Entity>& list) {
  if (uniqueIndex<T>() == index)
    getEntities<T>(index, list);
  else
    getEntities<Ts...>(index, list);
}

template <typename T, typename... Ts>
std::vector<Entity> Registry::getEntities(Component index) {
  std::vector<Entity> list;
  getEntities<T, Ts...>(index, list);
  
  return list;
}

template <typename T>
void Registry::removeUncommonEntities(std::vector<Entity>& list) {
  std::vector<Entity> common;
  auto ptr = getComponentStorage<T>();
  
  if (ptr && list.size() > 0) {
    std::for_each(list.begin(), list.end(), [&](Entity& e){
      if (ptr->contains(e))
        common.push_back(e);
    });
  }
  
  list = common;
}

template <typename T, typename... Ts>
std::enable_if_t<(sizeof...(Ts) > 0), void>
Registry::removeUncommonEntities(std::vector<Entity>& list) {
  removeUncommonEntities<T>(list);
  removeUncommonEntities<Ts...>(list);
}

template <typename T, typename Functor>
void Registry::forEach(Functor&& f) {
  auto ptr = getComponentStorage<T>();
  if (ptr)
    std::for_each(ptr->begin(), ptr->end(), f);
}

template <typename... Ts, typename Functor>
std::enable_if_t<(sizeof...(Ts) > 1), void> Registry::forEach(Functor&& f) {
  auto keyList = componentList<Ts...>();
  auto shortest = shortestComponent(keyList);
  
  if (shortest.second > 0 && shortest.second != maxValue<size_t>()) {
    auto entityList = getEntities<Ts...>(shortest.first);
    removeUncommonEntities<Ts...>(entityList);
    
    std::for_each(entityList.begin(), entityList.end(), [&](Entity& e){
            auto params = std::tuple<Ts&...>(*getComponent<Ts>(e)...);
            std::apply(f, params);
    });
  }
}

}
