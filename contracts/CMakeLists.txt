add_library(aerosim_contracts STATIC
    src/RegisterMetaTypes.cpp
    include/aerosim/contracts/TaskPorts.h
    include/aerosim/ui/IntegrationPorts.h
)
add_library(aerosim::contracts ALIAS aerosim_contracts)
target_include_directories(aerosim_contracts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(aerosim_contracts PUBLIC Qt5::Core)
