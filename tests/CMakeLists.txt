# ---------------------------------------------------------------------------
# 单元 / 集成测试
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Test)

function(vocab_add_test name)
    add_executable(${name} ${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp)
    target_link_libraries(${name} PRIVATE vocab_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    # 让测试可独立运行（无需手动设置 PATH）
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>;${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endfunction()

vocab_add_test(test_rules)
vocab_add_test(test_generator)
vocab_add_test(test_repositories)
vocab_add_test(test_services)

# 需要 QApplication 的界面测试（Theme 属于界面层，直接编入该测试目标）
add_executable(test_ui
    ${CMAKE_CURRENT_SOURCE_DIR}/test_ui.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Theme.cpp)
target_link_libraries(test_ui PRIVATE vocab_core Qt6::Test Qt6::Widgets)
add_test(NAME test_ui COMMAND test_ui)
set_tests_properties(test_ui PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>;${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
