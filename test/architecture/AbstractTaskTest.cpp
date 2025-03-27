#include <gtest/gtest.h>

#include <vigine/abstracttask.h>
#include <vigine/vigine.h>
#include "concepts.h"

#include <memory>

using namespace vigine;

// Concept for checking doWork method
template <typename T>
concept MethodCheck_Execute = requires(T t)
{
    t.execute();
    { t.execute() } -> std::same_as<void>;
};

// Test class for AbstractTask
class TestTask : public AbstractTask {
public:
    void execute() override {}
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
    EXPECT_TRUE((HasMethod_execute<AbstractTask, void>))
        << "AbstractTask hasn't correct execute method";
}

TEST(AbstractTaskTest, constructor_empty)
{
    std::unique_ptr<AbstractTask> task;
    ASSERT_NO_THROW(task = std::make_unique<TestTask>());
    ASSERT_NE(task, nullptr);
}

TEST(AbstractTaskTest, doWork_empty_void)
{
    EXPECT_TRUE(MethodCheck_Execute<AbstractTask>) << "AbstractTask hasn't expected void execute() method";
    
    auto task = std::make_unique<TestTask>();
    ASSERT_NO_THROW(task->execute());
}
