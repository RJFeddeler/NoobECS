//
//  Registry.cpp
//  BasaltEngine
//
//  Created by Robert Feddeler on 7/30/22.
//

#include "Registry.hpp"


namespace basalt::data {

Entity Registry::createEntity(bool recycleIfAvailable) {
  auto e = recycleIfAvailable ? recycleEntity() : NullEntity;
  
  if (e == NullEntity) {
    auto id = static_cast<EntityID>(entities_.size());
    e = entityCombine(id, 0);
    entities_.push_back(e);
  }
  
  return e;
}

Entity Registry::recycleEntity() {
  if (entityRecyclingCount_ > 0) {
    auto e = entityCombine(entityRecyclingHead_,
                           entityGeneration(entities_[entityRecyclingHead_]));
    
    entityRecyclingHead_ = entityIdentifier(entities_[entityRecyclingHead_]);
    --entityRecyclingCount_;
    
    entities_[entityIdentifier(e)] = e;
    
    return e;
  }
  else
    return NullEntity;
}

void Registry::deleteEntity(Entity entity) {
  auto id = entityIdentifier(entity);
  if (id < entities_.size() && entity == entities_[id]) {
    entities_[id] = entityCombine(entityRecyclingHead_,
                                  entityGeneration(entities_[id]) + 1);
    entityRecyclingHead_ = id;
    ++entityRecyclingCount_;
    
    std::for_each(components_.begin(),
                  components_.end(),
                  [entity](auto& component){
      
      component->remove(entity);
    });
  }
}

}
