int main(int argc, const char * argv[]) {
  pebble::data::Registry registry;
  
  const auto entityA = registry.createEntity();
  registry.addComponent<position>(entityA, { 1.1f, 1.1f });
  registry.addComponent<velocity>(entityA, { 0.1f, 0.1f });
  
  const auto entityB = registry.createEntity();
  registry.addComponent<position>(entityB, { 2.2f, 2.2f });
  registry.addComponent<velocity>(entityB, { 0.2f, 0.2f });
  
  const auto entityC = registry.createEntity();
  registry.addComponent<position>(entityC, { 3.3f, 3.3f });
  
  registry.deleteEntity(entityB);
  
  const auto entityD = registry.createEntity();
  registry.addComponent<position>(entityD, { 4.4f, 4.4f });
  registry.addComponent<velocity>(entityD, { 0.4f, 0.4f });
  
  registry.forEach<position>([](auto& pos) {
    pos.x += 10.f;
    pos.y += 10.f;
  });
  
  float dT = 1.f;
  registry.forEach<position, velocity>([dT](auto& pos, auto& vel) {
    pos.x += vel.dx * dT;
    pos.y += vel.dy * dT;
  });
}
