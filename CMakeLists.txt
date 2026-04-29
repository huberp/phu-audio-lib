cmake_minimum_required(VERSION 3.22)
project(PhuAudioLib LANGUAGES CXX)

# --- Header-only core (events, audio utilities, memory, util) ---
add_library(PhuAudioLib INTERFACE)
target_include_directories(PhuAudioLib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(PhuAudioLib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/AudioSampleFifo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/BeatSyncBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/BucketSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/PacketFifo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/FFTProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/LinkwitzRileyFilter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/NoteToFreq.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/PpqAddressedRingBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/RingBufferInsertResult.h
    ${CMAKE_CURRENT_SOURCE_DIR}/events/Event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/events/EventSource.h
    ${CMAKE_CURRENT_SOURCE_DIR}/events/SyncGlobals.h
    ${CMAKE_CURRENT_SOURCE_DIR}/events/SyncGlobalsListener.h
    ${CMAKE_CURRENT_SOURCE_DIR}/network/MulticastBroadcasterBase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/network/StatefulBroadcaster.h
    ${CMAKE_CURRENT_SOURCE_DIR}/network/SpectrumBroadcaster.h
    ${CMAKE_CURRENT_SOURCE_DIR}/network/SampleBroadcaster.h
    ${CMAKE_CURRENT_SOURCE_DIR}/network/CommandBroadcaster.h
    ${CMAKE_CURRENT_SOURCE_DIR}/network/CommandBroadcasterBase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/network/CtrlBroadcaster.h
    ${CMAKE_CURRENT_SOURCE_DIR}/debug/EditorLogger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/debug/DebugLogEventQueue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/debug/DebugLogSink.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/AlignedAllocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/StringUtil.h
)

# --- Compiled network + debug sources (sockets, logger impl) ---
add_library(PhuNetworkLib STATIC
    network/MulticastBroadcasterBase.cpp
    network/SpectrumBroadcaster.cpp
    network/SampleBroadcaster.cpp
    network/CommandBroadcaster.cpp
    network/CtrlBroadcaster.cpp
    debug/EditorLogger.cpp
)
target_include_directories(PhuNetworkLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PhuNetworkLib PUBLIC PhuAudioLib)
if(WIN32)
    target_link_libraries(PhuNetworkLib PRIVATE ws2_32)
endif()

# --- OpenGL utilities (optional) ---
add_subdirectory(gl)
