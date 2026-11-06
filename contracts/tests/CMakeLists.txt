set(AEROSIM_CONTRACT_HEADER_COMPILE_SOURCES
    PublicHeadersCompileMain.cpp
    compile/AccessResultHeader.cpp
    compile/DataAccessHeader.cpp
    compile/EnumTokensHeader.cpp
    compile/IntegrationPortsHeader.cpp
    compile/PostProcessRefsHeader.cpp
    compile/ReferencesHeader.cpp
    compile/ResourceModelsHeader.cpp
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

add_executable(aerosim_contract_legacy_ref_shape_test
    LegacyReferenceShapeTest.cpp)
target_link_libraries(aerosim_contract_legacy_ref_shape_test PRIVATE aerosim_contracts)
add_test(NAME contracts_legacy_ref_shape
    COMMAND aerosim_contract_legacy_ref_shape_test)

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

add_test(NAME contracts_migration_fixture_layout
    COMMAND "${CMAKE_COMMAND}"
            "-DCONTRACT_FIXTURE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/VerifyFixtureLayout.cmake")
