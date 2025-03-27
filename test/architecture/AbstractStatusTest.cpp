#include <gtest/gtest.h>

#include <vigine/vigine.h>
#include "concepts.h"

#include <memory>

using namespace vigine;

TEST(AbstractStatusTest, check_isAbstract) {
  EXPECT_TRUE((IsAbstract<AbstractStatus>))
      << "AbstractStatus isn't an abstract";
}

TEST(AbstractStatusTest, method_destructor) {
  EXPECT_TRUE((HasMethod_destructor<AbstractStatus>))
      << "AbstractStatus hasn't correct destructor";
}

TEST(AbstractStatusTest, method_operatorEqual) {
    EXPECT_TRUE((HasMethod_operatorEqual<AbstractStatus, bool, const AbstractStatus&>))
        << "AbstractStatus hasn't correct operator== method";
}

TEST(AbstractStatusTest, method_operatorNotEqual) {
    EXPECT_TRUE((HasMethod_operatorNotEqual<AbstractStatus, bool, const AbstractStatus&>))
        << "AbstractStatus hasn't correct operator!= method";
}
