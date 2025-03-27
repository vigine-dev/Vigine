#include <gtest/gtest.h>

#include <vigine/abstracttaskflow.h>
#include <vigine/abstracttask.h>
#include "concepts.h"

#include <memory>
#include <vector>

using namespace vigine;

class TestTask : public AbstractTask {
public:
    void execute() override {}
};

class TestTaskFlow : public AbstractTaskFlow {
public:
    void operator()() override {}
    void addTask(AbstractTask* task, const RelationContainer& relations) override {}
    void removeTask(AbstractTask* task) override {}
    void update() override {}
};

// Concept for checking addTask method
template <typename T, typename R, typename Task, typename Relations>
concept HasMethod_addTask = requires(T t, Task* task, const Relations& relations)
{
    { t.addTask(task, relations) } -> std::same_as<R>;
};

// Concept for checking removeTask method
template <typename T, typename R, typename Task>
concept HasMethod_removeTask = requires(T t, Task task)
{
    { t.removeTask(task) } -> std::same_as<R>;
};

TEST(AbstractTaskFlowTest, check_isAbstract) {
    EXPECT_TRUE((IsAbstract<AbstractTaskFlow>))
        << "AbstractTaskFlow isn't an abstract";
}

TEST(AbstractTaskFlowTest, method_destructor) {
    EXPECT_TRUE((HasMethod_destructor<AbstractTaskFlow>))
        << "AbstractTaskFlow hasn't correct destructor";
}

TEST(AbstractTaskFlowTest, method_operator) {
    EXPECT_TRUE((HasMethod_operator<AbstractTaskFlow, void>))
        << "AbstractTaskFlow hasn't correct operator() method";
}

TEST(AbstractTaskFlowTest, method_addTask) {
    auto taskFlow = std::make_unique<TestTaskFlow>();
    auto task = std::make_unique<TestTask>();
    
    // Create a relation using pointers
    Relation relation = std::make_pair(Result::Code::Success, task.get());
    RelationContainer relations;
    relations.push_back(relation);
    
    ASSERT_NO_THROW(taskFlow->addTask(task.get(), relations));
}

TEST(AbstractTaskFlowTest, method_removeTask) {
    EXPECT_TRUE((HasMethod_removeTask<AbstractTaskFlow, void, AbstractTask*>))
        << "AbstractTaskFlow hasn't correct removeTask method";
}

TEST(AbstractTaskFlowTest, method_update) {
    EXPECT_TRUE((HasMethod_update<AbstractTaskFlow, void>))
        << "AbstractTaskFlow hasn't correct update method";
}
