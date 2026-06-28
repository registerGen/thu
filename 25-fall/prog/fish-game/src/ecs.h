// An implementation of an Entity-Component-System (ECS) framework in C++.
// Also includes an Event Bus for handling events between systems.
// Also includes a Context Manager for storing global state.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

using Entity = std::uint32_t;
using SubscriptionId = std::uint32_t;

class Component {
public:
  virtual ~Component() = default;
};

class System {
protected:
  bool enabled = true;

public:
  virtual ~System() = default;
  virtual void update(float dt) = 0;

  void enable() { enabled = true; }
  void disable() { enabled = false; }
  bool is_enabled() const { return enabled; }
};

class Event {
public:
  virtual ~Event() = default;
};

template <typename T>
concept ComponentType = std::derived_from<T, Component>;

template <typename T>
concept SystemType = std::derived_from<T, System>;

template <typename T>
concept EventType = std::derived_from<T, Event>;

class ComponentManager {
private:
  std::unordered_map<std::type_index, std::unordered_map<Entity, std::shared_ptr<Component>>>
    components;

public:
  template <ComponentType T, typename... Args>
  void add(Entity entity, Args&&... args) {
    components[typeid(T)][entity] = std::make_shared<T>(std::forward<Args>(args)...);
  }

  template <ComponentType Base, ComponentType Derived, typename... Args>
  void add_as(Entity entity, Args&&... args) {
    static_assert(std::is_base_of_v<Base, Derived>);
    components[typeid(Base)][entity] = std::make_shared<Derived>(std::forward<Args>(args)...);
  }

  template <ComponentType T>
  void remove(Entity entity) {
    auto& entity_to_component = components[typeid(T)];
    entity_to_component.erase(entity);
  }

  void destroy(Entity entity) {
    for (auto& [type, entity_to_component] : components) entity_to_component.erase(entity);
  }

  template <ComponentType T, ComponentType... Args>
  bool has(Entity entity) {
    auto& entity_to_component = components[typeid(T)];
    return entity_to_component.contains(entity) &&
           (sizeof...(Args) == 0 || (has<Args>(entity) && ...));
  }

  template <ComponentType T>
  std::shared_ptr<T> get(Entity entity) {
    auto& entity_to_component = components[typeid(T)];
    auto it = entity_to_component.find(entity);
    return it == entity_to_component.end() ? nullptr : std::static_pointer_cast<T>(it->second);
  }

  template <ComponentType T, ComponentType... Args>
  auto get() {
    using ResultType =
      std::vector<std::tuple<Entity, std::shared_ptr<T>, std::shared_ptr<Args>...>>;

    ResultType result;
    auto& entity_to_component = components[typeid(T)];
    result.reserve(entity_to_component.size());

    for (const auto& [entity, component] : entity_to_component)
      if (((components[typeid(Args)].contains(entity)) && ...))
        result.emplace_back(
          entity,
          std::static_pointer_cast<T>(component),
          std::static_pointer_cast<Args>(components[typeid(Args)][entity])...
        );

    return result;
  }

  template <ComponentType T, ComponentType... Args, typename Fn>
  void for_each(Fn&& fn) {
    auto& entity_to_component = components[typeid(T)];
    for (const auto& [entity, component] : entity_to_component)
      if (((components[typeid(Args)].contains(entity)) && ...))
        std::invoke(
          std::forward<Fn>(fn),
          entity,
          std::static_pointer_cast<T>(component),
          std::static_pointer_cast<Args>(components[typeid(Args)][entity])...
        );
  }
};

class EntityManager {
private:
  Entity next_id = 1;

public:
  Entity make_entity() { return next_id++; }

  void destroy(Entity entity, ComponentManager& cm) { cm.destroy(entity); }
};

class EventBus {
private:
  struct Subscription {
    SubscriptionId id;
    std::function<bool(const Event&)> handler;
    Subscription(SubscriptionId id, const std::function<bool(const Event&)>& handler)
        : id(id), handler(handler) { }
  };

  std::unordered_map<std::type_index, std::vector<Subscription>> subscriptions;
  SubscriptionId subscription_id = 1;
  std::queue<std::shared_ptr<Event>> events;

  void invoke_subscribers(const std::type_index& type, const Event& event) {
    auto it = subscriptions.find(type);
    if (it == subscriptions.end()) return;

    auto& subs_of_T = it->second;
    std::vector<SubscriptionId> done;

    for (const auto& subscription : subs_of_T) {
      bool handled = std::invoke(subscription.handler, event);
      if (handled) done.emplace_back(subscription.id);
    }

    if (!done.empty()) {
      std::erase_if(subs_of_T, [&](auto& subscription) {
        return std::ranges::find(done, subscription.id) != done.end();
      });
    }
  }

public:
  template <EventType T>
  SubscriptionId sub(std::function<void(const T&)> handler) {
    auto wrapper = [handler](const Event& event) {
      std::invoke(handler, static_cast<const T&>(event));
      return false;
    };
    SubscriptionId id = subscription_id++;
    subscriptions[typeid(T)].emplace_back(id, std::move(wrapper));
    return id;
  }

  template <EventType T>
  SubscriptionId sub_once(std::function<bool(const T&)> handler) {
    auto wrapper = [handler](const Event& event) {
      return std::invoke(handler, static_cast<const T&>(event));
    };
    SubscriptionId id = subscription_id++;
    subscriptions[typeid(T)].emplace_back(id, std::move(wrapper));
    return id;
  }

  template <EventType T>
  void unsub(SubscriptionId id) {
    auto& subs_of_T = subscriptions[typeid(T)];
    std::erase_if(subs_of_T, [id](Subscription& subscription) { return subscription.id == id; });
  }

  template <EventType T>
  void emit_now(const T& event) {
    invoke_subscribers(typeid(T), event);
  }

  template <EventType T>
  void emit(const T& event) {
    events.push(std::make_shared<T>(event));
  }

  void dispatch() {
    while (!events.empty()) {
      auto event = events.front();
      events.pop();
      const Event& event_ref = *event;
      invoke_subscribers(typeid(event_ref), event_ref);
    }
  }
};

class ContextManager {
private:
  std::unordered_map<std::type_index, std::shared_ptr<void>> context;

public:
  template <typename T, typename... Args>
  void add(Args&&... args) {
    context[typeid(T)] = std::make_shared<T>(std::forward<Args>(args)...);
  }

  template <typename T>
  std::shared_ptr<T> get() {
    auto it = context.find(typeid(T));
    return it == context.end() ? nullptr : std::static_pointer_cast<T>(it->second);
  }

  template <typename T>
  bool has() const {
    auto it = context.find(typeid(T));
    return it != context.end() && it->second;
  }

  template <typename T>
  void remove() {
    context.erase(typeid(T));
  }
};

class World {
private:
  std::shared_ptr<EntityManager> em;
  std::shared_ptr<ComponentManager> cm;
  std::shared_ptr<ContextManager> ctx;
  std::unordered_map<std::type_index, std::shared_ptr<System>> systems;
  std::vector<std::type_index> sys_order;
  EventBus bus;

public:
  World()
      : em(std::make_shared<EntityManager>()),
        cm(std::make_shared<ComponentManager>()),
        ctx(std::make_shared<ContextManager>()),
        systems(),
        sys_order(),
        bus() { }

  // Add an Entity.
  Entity make_entity() { return em->make_entity(); }

  // Remove an Entity.
  void destroy(Entity ent) { em->destroy(ent, *cm); }

  // Add Component of type T to Entity.
  template <ComponentType T, typename... Args>
  void add(Entity ent, Args&&... args) {
    if (!ent) return;
    cm->add<T, Args...>(ent, std::forward<Args>(args)...);
  }

  template <typename Base, typename Derived, typename... Args>
  void add_as(Entity ent, Args&&... args) {
    if (!ent) return;
    cm->add_as<Base, Derived, Args...>(ent, std::forward<Args>(args)...);
  }

  // Remove Component of type T from Entity.
  template <ComponentType T>
  void remove(Entity ent) {
    cm->remove<T>(ent);
  }

  // Check whether Entity has Components of all types in Args.
  template <ComponentType T, ComponentType... Args>
  bool has(Entity ent) const {
    return cm->has<T, Args...>(ent);
  }

  // Get Component of type T from Entity.
  template <ComponentType T>
  std::shared_ptr<T> get(Entity ent) const {
    return cm->get<T>(ent);
  }

  // Get all Entities paired with their Components of all types in Args.
  template <ComponentType T, ComponentType... Args>
  auto get() const {
    return cm->get<T, Args...>();
  }

  // Call fn for each Entity with Components of all types in Args.
  template <ComponentType T, ComponentType... Args, typename Fn>
  void for_each(Fn&& fn) {
    return cm->for_each<T, Args...>(std::forward<Fn>(fn));
  }

  // Register a System.
  template <SystemType T>
  void reg(std::shared_ptr<T> sys) {
    auto type = std::type_index(typeid(T));
    if (systems.find(type) == systems.end()) sys_order.emplace_back(type);
    systems[type] = std::move(sys);
  }

  // Get the System of type T
  template <SystemType T>
  const std::shared_ptr<T> sys() const {
    auto it = systems.find(typeid(T));
    return it == systems.end() ? nullptr : std::static_pointer_cast<T>(it->second);
  }

  // Subscribe an Event.
  template <EventType T>
  SubscriptionId sub(std::function<void(const T&)> handler) {
    return bus.sub<T>(handler);
  }

  // Subscribe an one-time Event.
  template <EventType T>
  SubscriptionId sub_once(std::function<bool(const T&)> handler) {
    return bus.sub_once<T>(handler);
  }

  // Unsubscribe an Event.
  template <EventType T>
  void unsub(SubscriptionId id) {
    if (!id) return;
    return bus.unsub<T>(id);
  }

  // Emit an urgent Event. May interrupt the current flow, be careful!
  template <EventType T>
  void emit_now(const T& ev) {
    bus.emit_now<T>(ev);
  }

  // Emit a lazy Event.
  template <EventType T>
  void emit(const T& ev) {
    bus.emit<T>(ev);
  }

  // Dispatch all queued Events.
  void dispatch() { bus.dispatch(); }

  // Add a Context of type T.
  template <typename T, typename... Args>
  void add_ctx(Args&&... args) {
    ctx->add<T>(std::forward<Args>(args)...);
  }

  // Get current Context of type T.
  template <typename T>
  std::shared_ptr<T> get_ctx() {
    return ctx->get<T>();
  }

  // Check whether Context of type T exists.
  template <typename T>
  bool has_ctx() const {
    return ctx->has<T>();
  }

  // Remove Context of type T.
  template <typename T>
  void remove_ctx() {
    ctx->remove<T>();
  }

  // Update all Systems.
  void update(float dt) {
    for (const auto& type : sys_order) {
      auto it = systems.find(type);
      if (it == systems.end()) continue;
      const auto& sys = it->second;
      if (sys && sys->is_enabled()) sys->update(dt);  // The unit of dt is second.
    }
    dispatch();
  }
};
