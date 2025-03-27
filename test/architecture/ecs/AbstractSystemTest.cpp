#include <gtest/gtest.h>

#include <vigine/ecs/abstractsystem.h>
#include <vigine/vigine.h>
#include "concepts.h"

#include <memory>

using namespace vigine;

TEST(AbstractSystemTest, check_isAbstract) {
  EXPECT_TRUE((IsAbstract<AbstractSystem>))
      << "AbstractSystem isn't an abstract";
}

TEST(AbstractSystemTest, method_destructor) {
  EXPECT_TRUE((HasMethod_destructor<AbstractSystem>))
      << "AbstractSystem hasn't correct destructor";
}
