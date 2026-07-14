find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(beamdrop_native
    beamdrop_pybind.cpp
)

target_link_libraries(beamdrop_native PRIVATE beamdrop_core)

if(BEAMDROP_BUILD_TESTS)
    add_test(
        NAME beamdrop_python_binding_tests
        COMMAND ${Python_EXECUTABLE} -m pytest
                ${CMAKE_CURRENT_SOURCE_DIR}/tests
                -v
    )
    set_tests_properties(beamdrop_python_binding_tests PROPERTIES
        ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:beamdrop_native>"
    )
endif()
