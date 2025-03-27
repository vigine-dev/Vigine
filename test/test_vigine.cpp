#include <gtest/gtest.h>

#include "vigine/vigine.h"

#include <glm/glm.hpp>
 
TEST(LibraryTest, FunctionTest) {

    //Vigine vigine;
    //vigine.run();
}

TEST(GLMTest, VectorTest) {

    glm::vec3 v(1.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
