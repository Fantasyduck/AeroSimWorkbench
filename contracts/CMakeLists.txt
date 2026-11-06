add_library(aerosim_contracts STATIC
    src/EnumTokens.cpp
    src/RegisterMetaTypes.cpp
    src/ResourceModels.cpp
    include/aerosim/contracts/AccessResult.h
    include/aerosim/contracts/DataAccess.h
    include/aerosim/contracts/EnumTokens.h
    include/aerosim/contracts/PostProcessRefs.h
    include/aerosim/contracts/References.h
    include/aerosim/contracts/ResourceModels.h
    include/aerosim/contracts/TaskModels.h
    include/aerosim/contracts/TaskPorts.h
    include/aerosim/contracts/Telemetry.h
    include/aerosim/contracts/WorkflowModels.h
    include/aerosim/ui/IntegrationPorts.h
)
add_library(aerosim::contracts ALIAS aerosim_contracts)
target_include_directories(aerosim_contracts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(aerosim_contracts PUBLIC Qt5::Core)

if(AEROSIM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
