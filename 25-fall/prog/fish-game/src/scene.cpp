#include "scene.h"

#include "ui.h"

void SceneSystem::show(Scene scene) {
  for (auto& ent : ents[std::to_underlying(scene)]) world->add<Shown>(ent);
}

void SceneSystem::hide(Scene scene) {
  for (auto& ent : ents[std::to_underlying(scene)]) world->remove<Shown>(ent);
}

void SceneSystem::change(Scene scene) {
  if (scene == Scene::Invalid || scene == cur_scene) return;
  if (cur_scene != Scene::Invalid) hide(cur_scene);
  cur_scene = scene;
  show(scene);
}

void SceneSystem::reg(Scene scene, Entity ent) {
  ents[std::to_underlying(scene)].emplace_back(ent);
}

void SceneSystem::unreg(Scene scene, Entity ent) {
  world->remove<Shown>(ent);
  std::erase(ents[std::to_underlying(scene)], ent);
}

void SceneSystem::clear(Scene scene) {
  hide(scene);
  ents[std::to_underlying(scene)].clear();
}
