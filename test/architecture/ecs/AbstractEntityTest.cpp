#include <gtest/gtest.h>

#include <vigine/ecs/abstractentity.h>
#include <vigine/vigine.h>
#include "concepts.h"

#include <vector>

using namespace vigine;

// Entity-specific concepts
template <typename T, typename R, typename... Args>
concept HasMethod_addComponent = requires(T t, Args... args) {
  { t.addComponent(args...) } -> std::same_as<R>;
};

template <typename T, typename R, typename... Args>
concept HasMethod_removeComponent = requires(T t, Args... args) {
  { t.removeComponent(args...) } -> std::same_as<R>;
};

template <typename T, typename R, typename... Args>
concept HasMethod_getComponent = requires(T t, Args... args) {
  { t.getComponent(args...) } -> std::same_as<R>;
};

template <typename T, typename R, typename... Args>
concept HasMethod_hasComponent = requires(T t, Args... args) {
  { t.hasComponent(args...) } -> std::same_as<R>;
};

template <typename T, typename R>
concept HasMethod_getAllComponents = requires(T t) {
  { t.getAllComponents() } -> std::same_as<R>;
};

TEST(AbstractEntityTest, check_isAbstract) {
  EXPECT_TRUE((IsAbstract<AbstractEntity>))
      << "AbstractEntity isn't an abstract";
}

TEST(AbstractEntityTest, method_destructor) {
  EXPECT_TRUE((HasMethod_destructor<AbstractEntity>))
      << "AbstractEntity hasn't correct destructor";
}

TEST(AbstractEntityTest, method_addComponent) {
  EXPECT_TRUE((HasMethod_addComponent<AbstractEntity, void, AbstractComponent*>))
      << "AbstractEntity hasn't correct addComponent method";
}

TEST(AbstractEntityTest, method_removeComponent) {
  EXPECT_TRUE((HasMethod_removeComponent<AbstractEntity, void, AbstractComponent*>))
      << "AbstractEntity hasn't correct removeComponent method";
}

TEST(AbstractEntityTest, method_getComponent) {
  EXPECT_TRUE((HasMethod_getComponent<AbstractEntity, AbstractComponent*, const std::string&>))
      << "AbstractEntity hasn't correct getComponent method";
}

TEST(AbstractEntityTest, method_hasComponent) {
  EXPECT_TRUE((HasMethod_hasComponent<AbstractEntity, bool, const std::string&>))
      << "AbstractEntity hasn't correct hasComponent method";
}

TEST(AbstractEntityTest, method_getAllComponents) {
  EXPECT_TRUE((HasMethod_getAllComponents<AbstractEntity, std::vector<AbstractComponent*>>))
      << "AbstractEntity hasn't correct getAllComponents method";
}
