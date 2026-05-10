add_library(PhuAnalogLib STATIC
    circuit/Circuit.cpp
    nonlinear/NR.cpp
    nonlinear/TriodeKoren.cpp
    models/TubeStage.cpp
    models/VariableMuStage.cpp
    models/transformer/TransformerLinear.cpp
    models/sidechain/RectifierDetector.cpp
)
target_include_directories(PhuAnalogLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(PhuAnalogLib PUBLIC cxx_std_17)
if(MSVC)
    target_compile_definitions(PhuAnalogLib PRIVATE _USE_MATH_DEFINES)
endif()
