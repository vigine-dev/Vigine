#include <gtest/gtest.h>

#include <vigine/taskflow.h>
#include <vigine/abstracttask.h>
#include "concepts.h"
#include "vigine/result.h"

#include <memory>
#include <concepts>

using namespace vigine;

// Test class for AbstractTask
class TestTask : public AbstractTask {
public:
    Result execute() override { return Result(); }
};

TEST(TaskFlowTest, method_destructor)
{
    EXPECT_TRUE((HasMethod_destructor<TaskFlow>))
        << "TaskFlow hasn't correct destructor";
}

TEST(TaskFlowTest, constructor_empty)
{
    std::unique_ptr<TaskFlow> taskFlow;
    ASSERT_NO_THROW(taskFlow = std::make_unique<TaskFlow>());
    ASSERT_NE(taskFlow, nullptr);
}

TEST(TaskFlowTest, operator_execution)
{
    auto taskFlow = std::make_unique<TaskFlow>();
    ASSERT_NO_THROW((*taskFlow)()) << "operator() should not throw";
}

TEST(TaskFlowTest, addTask_empty_AbstractTask_pointer)
{
    auto taskFlow = std::make_unique<TaskFlow>();
    auto task = std::make_unique<TestTask>();
    AbstractTask* taskPtr = task.get();
    ASSERT_NO_THROW(taskFlow->addTask(std::move(task))) << "addTask() should not throw";

    taskFlow->changeTaskTo(taskPtr);
    EXPECT_EQ(taskPtr, taskFlow->currentTask()) << "addTask() should set current task";
}

TEST(TaskFlowTest, removeTask_empty_void)
{
    auto taskFlow = std::make_unique<TaskFlow>();
    auto task = std::make_unique<TestTask>();
    AbstractTask* taskPtr = task.get();
    taskFlow->addTask(std::move(task));
    ASSERT_NO_THROW(taskFlow->removeTask(taskPtr)) << "removeTask() should not throw";
    EXPECT_EQ(nullptr, taskFlow->currentTask()) << "removeTask() should clear current task";
}

TEST(TaskFlowTest, addTransition_empty_void)
{
    auto taskFlow = std::make_unique<TaskFlow>();
    auto task1 = std::make_unique<TestTask>();
    auto task2 = std::make_unique<TestTask>();
    AbstractTask* task1Ptr = task1.get();
    AbstractTask* task2Ptr = task2.get();
    taskFlow->addTask(std::move(task1));
    taskFlow->addTask(std::move(task2));
    ASSERT_NO_THROW(taskFlow->addTransition(task1Ptr, task2Ptr, Result::Code::Success)) << "addTransition() should not throw";
}

TEST(TaskFlowTest, changeTaskTo_empty_void)
{
    auto taskFlow = std::make_unique<TaskFlow>();
    auto task = std::make_unique<TestTask>();
    AbstractTask* taskPtr = task.get();
    taskFlow->addTask(std::move(task));
    ASSERT_NO_THROW(taskFlow->changeTaskTo(taskPtr)) << "changeTaskTo() should not throw";
    EXPECT_EQ(taskPtr, taskFlow->currentTask()) << "changeTaskTo() should set current task";
}

TEST(TaskFlowTest, currentTask_empty_AbstractTask_pointer)
{
    auto taskFlow = std::make_unique<TaskFlow>();
    EXPECT_EQ(nullptr, taskFlow->currentTask()) << "currentTask() should return nullptr when no task is set";
    
    auto task = std::make_unique<TestTask>();
    AbstractTask* taskPtr = task.get();
    taskFlow->addTask(std::move(task));
    taskFlow->changeTaskTo(taskPtr);
    EXPECT_EQ(taskPtr, taskFlow->currentTask()) << "currentTask() should return the correct task";
}

TEST(TaskFlowTest, hasTasksToRun_empty_bool)
{
    auto taskFlow = std::make_unique<TaskFlow>();
    EXPECT_FALSE(taskFlow->hasTasksToRun()) << "hasTasksToRun() should return false when no tasks are set";
    
    auto task = std::make_unique<TestTask>();
    auto taskPtr = taskFlow->addTask(std::move(task));
    taskFlow->changeTaskTo(taskPtr);
    EXPECT_TRUE(taskFlow->hasTasksToRun()) << "hasTasksToRun() should return true when tasks are set";
}

TEST(TaskFlowTest, runCurrentTask_empty_void)
{
    auto taskFlow = std::make_unique<TaskFlow>();
    ASSERT_NO_THROW(taskFlow->runCurrentTask()) << "runCurrentTask() should not throw";
}

