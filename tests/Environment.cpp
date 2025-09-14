#include "Environment.h"

// Definition of static member variable
ref<Device> BasicTestEnvironment::device = nullptr;

// Register the global test environment
::testing::Environment* const basicEnv = ::testing::AddGlobalTestEnvironment(new BasicTestEnvironment);
