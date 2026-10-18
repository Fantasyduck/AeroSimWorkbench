add_library(aerosim_contracts STATIC
    src/RegisterMetaTypes.cpp
    include/aerosim/contracts/TaskPorts.h
)
target_include_directories(aerosim_contracts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(aerosim_contracts PUBLIC Qt5::Core)
