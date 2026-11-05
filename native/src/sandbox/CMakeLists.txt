add_executable(sandbox main.cpp)
target_link_libraries(sandbox PRIVATE Xordor::Engine)
xe_set_warnings(sandbox)
