#pragma once

#include <vigine/api/taskflow/abstracttask.h>

namespace vigine
{
class DatabaseService;
}

class InitBDTask : public vigine::AbstractTask
{
  public:
    InitBDTask();

    /**
     * @brief Binds the DatabaseService the task will read from.
     *
     * Called by `main()` immediately after the task is constructed,
     * before the TaskFlow is registered on the FSM. The pointer must
     * outlive every `run()` call; the demo guarantees this by keeping
     * the service alive in main's scope through the engine's
     * registerService handle.
     */
    void setDatabaseService(vigine::DatabaseService *service);

    [[nodiscard]] vigine::Result run() override;

  private:
    vigine::DatabaseService *_dbService{nullptr};
};
