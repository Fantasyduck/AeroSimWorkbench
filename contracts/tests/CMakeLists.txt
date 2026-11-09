set(AEROSIM_CONTRACT_HEADER_COMPILE_SOURCES
    PublicHeadersCompileMain.cpp
    compile/AccessResultHeader.cpp
    compile/ConfigurationModelsHeader.cpp
    compile/DataAccessHeader.cpp
    compile/EnumTokensHeader.cpp
    compile/ExecutionModelsHeader.cpp
    compile/IntegrationPortsHeader.cpp
    compile/MeshModelsHeader.cpp
    compile/PostProcessRefsHeader.cpp
    compile/ReferencesHeader.cpp
    compile/ResourceModelsHeader.cpp
    compile/ResultModelsHeader.cpp
    compile/TaskModelsHeader.cpp
    compile/TaskPortsHeader.cpp
    compile/TelemetryHeader.cpp
    compile/WorkflowModelsHeader.cpp
)

add_executable(aerosim_contract_headers_compile
    ${AEROSIM_CONTRACT_HEADER_COMPILE_SOURCES})
target_link_libraries(aerosim_contract_headers_compile PRIVATE aerosim_contracts)
add_test(NAME contracts_public_headers_compile
    COMMAND aerosim_contract_headers_compile)

add_executable(aerosim_contract_reference_shape_test
    ReferenceShapeTest.cpp)
target_link_libraries(aerosim_contract_reference_shape_test PRIVATE aerosim_contracts)
add_test(NAME contracts_reference_shape
    COMMAND aerosim_contract_reference_shape_test)

add_executable(aerosim_contract_enum_tokens_test
    StableEnumTokenManifestTest.cpp)
target_link_libraries(aerosim_contract_enum_tokens_test PRIVATE aerosim_contracts)
add_test(NAME contracts_stable_enum_token_manifest
    COMMAND aerosim_contract_enum_tokens_test)

add_executable(aerosim_contract_access_result_test
    AccessResultTest.cpp)
target_link_libraries(aerosim_contract_access_result_test PRIVATE aerosim_contracts)
add_test(NAME contracts_access_result
    COMMAND aerosim_contract_access_result_test)

add_executable(aerosim_contract_reference_rules_test
    ReferenceRulesTest.cpp)
target_link_libraries(aerosim_contract_reference_rules_test PRIVATE aerosim_contracts)
add_test(NAME contracts_reference_rules
    COMMAND aerosim_contract_reference_rules_test)

add_executable(aerosim_contract_resource_models_test
    ResourceModelsTest.cpp)
target_link_libraries(aerosim_contract_resource_models_test PRIVATE aerosim_contracts)
add_test(NAME contracts_resource_models
    COMMAND aerosim_contract_resource_models_test)

add_executable(aerosim_contract_workflow_v2_test
    WorkflowV2ContractTests.cpp)
target_link_libraries(aerosim_contract_workflow_v2_test PRIVATE aerosim_contracts)
add_test(NAME contracts_workflow_v2
    COMMAND aerosim_contract_workflow_v2_test)

add_executable(aerosim_contract_configuration_models_test
    ConfigurationModelsTest.cpp)
target_link_libraries(aerosim_contract_configuration_models_test PRIVATE aerosim_contracts)
add_test(NAME contracts_configuration_models
    COMMAND aerosim_contract_configuration_models_test)

add_executable(aerosim_contract_execution_models_test
    ExecutionModelsTest.cpp)
target_link_libraries(aerosim_contract_execution_models_test PRIVATE aerosim_contracts)
add_test(NAME contracts_execution_models
    COMMAND aerosim_contract_execution_models_test)

add_executable(aerosim_contract_mesh_models_test
    MeshModelsTest.cpp)
target_link_libraries(aerosim_contract_mesh_models_test PRIVATE aerosim_contracts)
add_test(NAME contracts_mesh_models
    COMMAND aerosim_contract_mesh_models_test)

add_executable(aerosim_contract_telemetry_models_test
    TelemetryModelsTest.cpp)
target_link_libraries(aerosim_contract_telemetry_models_test PRIVATE aerosim_contracts)
add_test(NAME contracts_telemetry_models
    COMMAND aerosim_contract_telemetry_models_test)

add_executable(aerosim_contract_result_models_test
    ResultModelsTest.cpp)
target_link_libraries(aerosim_contract_result_models_test PRIVATE aerosim_contracts)
add_test(NAME contracts_result_models
    COMMAND aerosim_contract_result_models_test)

add_test(NAME contracts_migration_fixture_layout
    COMMAND "${CMAKE_COMMAND}"
            "-DCONTRACT_FIXTURE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/VerifyFixtureLayout.cmake")
