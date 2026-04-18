add_library(PhuGLUtils STATIC
    GLSLShaderBuilder.cpp
    GLSLShaderBuilder.h
    GLSnapshotRenderer.h
    GLRendererBase.h
)

target_include_directories(PhuGLUtils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(PhuGLUtils PUBLIC juce::juce_opengl juce::juce_gui_basics)
