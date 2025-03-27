// #include <gtest/gtest.h>
// #include <vigine/component/componentmanager.h>
// #include <vigine/entity.h>

// #include <string>

// class Component1Test {
// public:
//     Component1Test(const std::string &name) : _name{name} {}
//     std::string name() { return _name; }

// private:
//     std::string _name;
// };

// class  Component2Test
// {
// public:
//     Component2Test(const std::string &name) : _name{name} {}
//     std::string name() { return _name; }

// private:
//     std::string _name;
// };

// class  Component3Test
// {
// public:
//     Component3Test(const std::string &name) : _name{name} {}
//     std::string name() { return _name; }

// private:
//     std::string _name;
// };

// TEST(EntityTest, entity_addComponent)
// {
//     ComponentManager compManager;
//     Entity ent;

//     using TestedType1 = Component1Test;
//     using TestedType2 = Component2Test;
//     using TestedType3 = Component3Test;

//     std::string name1 = "Component1Test";
//     std::string name2 = "Component2Test";
//     std::string name3 = "Component3Test";

//     auto comp1 = compManager.createComponent<TestedType1>(name1);
//     auto comp2 = compManager.createComponent<TestedType2>(name2);
//     auto comp3 = compManager.createComponent<TestedType3>(name3);

//     ent.addComponent(comp1);
//     ent.addComponent(comp2);
//     ent.addComponent(comp3);

//     auto receivedComponent1 = ent.getComponent<TestedType1>();
//     auto receivedComponent2 = ent.getComponent<TestedType2>();
//     auto receivedComponent3 = ent.getComponent<TestedType3>();


//     // Component1Test
//     {
//         EXPECT_TRUE((std::is_same_v<TestedType1, std::remove_reference_t<decltype(*comp1)>>));
//         EXPECT_TRUE((std::is_same_v<TestedType1, std::remove_reference_t<decltype(*receivedComponent1)>>));

//         EXPECT_EQ(typeid(TestedType1).name(), typeid(decltype(*comp1)).name());
//         EXPECT_EQ(typeid(TestedType1).name(), typeid(decltype(*receivedComponent1)).name());
//         EXPECT_EQ(typeid(comp1), typeid(receivedComponent1));
//         EXPECT_EQ(name1, comp1->name());
//         EXPECT_EQ(name1, receivedComponent1->name());
//         EXPECT_EQ(comp1->name(), receivedComponent1->name());
//     }

//     // Component2Test
//     {
//         EXPECT_TRUE((std::is_same_v<TestedType2, std::remove_reference_t<decltype(*comp2)>>));
//         EXPECT_TRUE((std::is_same_v<TestedType2, std::remove_reference_t<decltype(*receivedComponent2)>>));

//         EXPECT_EQ(typeid(TestedType2).name(), typeid(decltype(*comp2)).name());
//         EXPECT_EQ(typeid(TestedType2).name(), typeid(decltype(*receivedComponent2)).name());
//         EXPECT_EQ(typeid(comp2), typeid(receivedComponent2));
//         EXPECT_EQ(name2, comp2->name());
//         EXPECT_EQ(name2, receivedComponent2->name());
//         EXPECT_EQ(comp2->name(), receivedComponent2->name());
//     }

//     // Component3Test
//     {
//         EXPECT_TRUE((std::is_same_v<TestedType3, std::remove_reference_t<decltype(*comp3)>>));
//         EXPECT_TRUE((std::is_same_v<TestedType3, std::remove_reference_t<decltype(*receivedComponent3)>>));

//         EXPECT_EQ(typeid(TestedType3).name(), typeid(decltype(*comp3)).name());
//         EXPECT_EQ(typeid(TestedType3).name(), typeid(decltype(*receivedComponent3)).name());
//         EXPECT_EQ(typeid(comp3), typeid(receivedComponent3));
//         EXPECT_EQ(name3, comp3->name());
//         EXPECT_EQ(name3, receivedComponent3->name());
//         EXPECT_EQ(comp3->name(), receivedComponent3->name());
//     }
// }
