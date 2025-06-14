#include <gtest/gtest.h>

#include <vigine/abstracttask.h>
#include <vigine/vigine.h>
#include "concepts.h"

#include <memory>

using namespace vigine;

// Concept for checking execute method
template <typename T>
concept MethodCheck_Execute = requires(T t)
{
    t.execute();
    { t.execute() } -> std::same_as<Result>;
};

// Test class for AbstractTask
class TestTask : public AbstractTask {
public:
    Result execute() override { return Result(); }
};

TEST(AbstractTaskTest, check_isAbstract)
{
    EXPECT_TRUE((IsAbstract<AbstractTask>))
        << "AbstractTask isn't an abstract";
}

TEST(AbstractTaskTest, method_destructor)
{
    EXPECT_TRUE((HasMethod_destructor<AbstractTask>))
        << "AbstractTask hasn't correct destructor";
}

TEST(AbstractTaskTest, method_execute)
{
    EXPECT_TRUE((HasMethod_execute<AbstractTask, Result>))
        << "AbstractTask hasn't correct execute method";
}

TEST(AbstractTaskTest, constructor_empty)
{
    std::unique_ptr<AbstractTask> task;
    ASSERT_NO_THROW(task = std::make_unique<TestTask>());
    ASSERT_NE(task, nullptr);
}

TEST(AbstractTaskTest, execute_returns_result)
{
    EXPECT_TRUE(MethodCheck_Execute<AbstractTask>) << "AbstractTask hasn't expected Result execute() method";
    
    auto task = std::make_unique<TestTask>();
    auto result = task->execute();
    ASSERT_EQ(result.code(), Result::Code::Success);
}
