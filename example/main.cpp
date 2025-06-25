#include "state/closestate.h"
#include "state/errorstate.h"
#include "state/initstate.h"
#include "state/workstate.h"

#include "task/addsomedatatask.h"
#include "task/checkbdshecmetask.h"
#include "task/initbdtask.h"
#include "task/readsomedatatask.h"

#include <vigine/statemachine.h>
#include <vigine/taskflow.h>
#include <vigine/vigine.h>

#include <glm/glm.hpp>
#include <iostream>

#include <memory>

using namespace vigine;
// clang-format off
int main() {
  Engine engine;
  StateMachine *stMachine = engine.state();

  // init states
  auto initState = std::make_unique<InitState>();
  auto workState = std::make_unique<WorkState>();
  auto errorState = std::make_unique<ErrorState>();
  auto closeState = std::make_unique<CloseState>();

  // init task flows
  auto initTaskFlow = std::make_unique<TaskFlow>();
  auto workTaskFlow = std::make_unique<TaskFlow>();
  auto errorTaskFlow = std::make_unique<TaskFlow>();
  auto closeTaskFlow = std::make_unique<TaskFlow>();

  // Add tasks
  auto* initDBTask = initTaskFlow->addTask(std::make_unique<InitBDTask>());
  auto* checkBDShecmeTask = initTaskFlow->addTask(std::make_unique<CheckBDShecmeTask>());
  initTaskFlow->addTransition(initDBTask, checkBDShecmeTask, Result::Code::Success);
  initTaskFlow->changeTaskTo(initDBTask);

  auto* addSomeDataTask = workTaskFlow->addTask(std::make_unique<AddSomeDataTask>());
  auto* readSomeDataTask = workTaskFlow->addTask(std::make_unique<ReadSomeDataTask>());
  workTaskFlow->addTransition(addSomeDataTask, readSomeDataTask, Result::Code::Success);
  workTaskFlow->changeTaskTo(addSomeDataTask);


  // Add states to Vigine
  auto initPtr = stMachine->addState(std::move(initState));
  auto workPtr = stMachine->addState(std::move(workState));
  auto errorPtr = stMachine->addState(std::move(errorState));
  auto closePtr = stMachine->addState(std::move(closeState));

  initPtr->setTaskFlow(std::move(initTaskFlow));
  workPtr->setTaskFlow(std::move(workTaskFlow));
  errorPtr->setTaskFlow(std::move(errorTaskFlow));
  closePtr->setTaskFlow(std::move(closeTaskFlow));

  // Add transitions
  stMachine->addTransition(initPtr, workPtr, Result::Code::Success);
  stMachine->addTransition(initPtr, errorPtr, Result::Code::Error);

  stMachine->addTransition(workPtr, closePtr, Result::Code::Success);
  stMachine->addTransition(workPtr, errorPtr, Result::Code::Error);

  stMachine->addTransition(errorPtr, workPtr, Result::Code::Success);
  stMachine->addTransition(errorPtr, closePtr, Result::Code::Error);

  stMachine->changeStateTo(initPtr);

  engine.run();

  return 0;
}

// clang-format on
