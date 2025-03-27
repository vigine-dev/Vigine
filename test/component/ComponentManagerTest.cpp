#include <gtest/gtest.h>

#include "vigine/component/componentmanager.h"

#include <glm/glm.hpp>

#include <type_traits>

using namespace vigine;

class Component1Test {
public:
  Component1Test(const std::string &name) : _name{name} {}
  std::string name() { return _name; }

private:
  std::string _name;
};

class  Component2Test
{
public:
    Component2Test(const std::string &name) : _name{name} {}
    std::string name() { return _name; }

private:
    std::string _name;
};

class  Component3Test
{
public:
    Component3Test(const std::string &name) : _name{name} {}
    std::string name() { return _name; }

private:
    std::string _name;
};

TEST(ComponentManagerTest, addNewComponent_createComponent)
{
    ComponentManager compManager;

    // Component1Test
    {
        using TestedType = Component1Test;
        std::string name = "Component1Test";
        auto comp1 = compManager.createComponent<TestedType>(name);
        auto it = compManager.cbegin<TestedType>();

        EXPECT_TRUE((std::is_same_v<TestedType, std::remove_reference_t<decltype(*comp1)>>));
        EXPECT_TRUE((std::is_same_v<TestedType, std::remove_reference_t<decltype(*(*it))>>));

        EXPECT_EQ(typeid(TestedType).name(), typeid(decltype(*comp1)).name());
        EXPECT_EQ(typeid(TestedType).name(), typeid(decltype(*(*it))).name());
        EXPECT_EQ(typeid(comp1), typeid(*it));
        EXPECT_EQ(name, comp1->name());
        EXPECT_EQ(name, (*it)->name());
        EXPECT_EQ(comp1->name(), (*it)->name());
    }

    // Component2Test
    {
        using TestedType = Component2Test;
        std::string name = "Component2Test";
        auto comp2 = compManager.createComponent<TestedType>(name);
        auto it2 = compManager.cbegin<TestedType>();

        EXPECT_TRUE((std::is_same_v<TestedType, std::remove_reference_t<decltype(*comp2)>>));
        EXPECT_TRUE((std::is_same_v<TestedType, std::remove_reference_t<decltype(*(*it2))>>));

        EXPECT_EQ(typeid(TestedType).name(), typeid(decltype(*comp2)).name());
        EXPECT_EQ(typeid(TestedType).name(), typeid(decltype(*(*it2))).name());
        EXPECT_EQ(typeid(comp2), typeid(*it2));
        EXPECT_EQ(name, comp2->name());
        EXPECT_EQ(name, (*it2)->name());
        EXPECT_EQ(comp2->name(), (*it2)->name());
    }

    // Component3Test
    {
        using TestedType = Component3Test;
        std::string name = "Component3Test";
        auto comp3 = compManager.createComponent<TestedType>(name);
        auto it3 = compManager.cbegin<TestedType>();

        EXPECT_TRUE((std::is_same_v<TestedType, std::remove_reference_t<decltype(*comp3)>>));
        EXPECT_TRUE((std::is_same_v<TestedType, std::remove_reference_t<decltype(*(*it3))>>));

        EXPECT_EQ(typeid(TestedType).name(), typeid(decltype(*comp3)).name());
        EXPECT_EQ(typeid(TestedType).name(), typeid(decltype(*(*it3))).name());
        EXPECT_EQ(typeid(comp3), typeid(*it3));
        EXPECT_EQ(name, comp3->name());
        EXPECT_EQ(name, (*it3)->name());
        EXPECT_EQ(comp3->name(), (*it3)->name());
    }
}
