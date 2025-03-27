#include <gtest/gtest.h>

#include <vigine/ecs/abstractcomponent.h>
#include <vigine/vigine.h>
#include "concepts.h"

#include <memory>

TEST(AbstractComponentTest, check_isAbstract) {
  EXPECT_TRUE((IsAbstract<AbstractComponent>))
      << "AbstractComponent isn't an abstract";
}

TEST(AbstractComponentTest, method_destructor) {
  EXPECT_TRUE((HasMethod_destructor<AbstractComponent>))
      << "AbstractComponent hasn't correct destructor";
}
