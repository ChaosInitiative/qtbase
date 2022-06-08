set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER /usr/local/bin/clang-cl)
set(CMAKE_CXX_COMPILER /usr/local/bin/clang-cl)
set(CMAKE_MT /usr/local/bin/llvm-mt)
set(CMAKE_C_FLAGS "-Wno-ignored-pragmas -Wno-unused-function -Wno-overloaded-virtual -Wno-unused-command-line-argument")
set(CMAKE_CXX_FLAGS "-Wno-ignored-pragmas -Wno-unused-function -Wno-overloaded-virtual -Wno-unused-command-line-argument")