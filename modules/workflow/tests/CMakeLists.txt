add_executable(aerosim_workflow_definition_test
    WorkflowDefinitionTests.cpp)
target_link_libraries(aerosim_workflow_definition_test
    PRIVATE aerosim::workflow Qt5::Core)
add_test(NAME workflow_definition_validation
    COMMAND aerosim_workflow_definition_test)

add_custom_target(aerosim_workflow_tests
    DEPENDS aerosim_workflow_definition_test)

add_executable(aerosim_workflow_state_test
    WorkflowStateTests.cpp)
target_link_libraries(aerosim_workflow_state_test
    PRIVATE aerosim::workflow Qt5::Core)
add_test(NAME workflow_state_derivation
    COMMAND aerosim_workflow_state_test)
add_dependencies(aerosim_workflow_tests aerosim_workflow_state_test)

add_executable(aerosim_workflow_input_stamp_test
    WorkflowInputStampTests.cpp)
target_link_libraries(aerosim_workflow_input_stamp_test
    PRIVATE aerosim::workflow Qt5::Core)
add_test(NAME workflow_input_stamp
    COMMAND aerosim_workflow_input_stamp_test)
add_dependencies(aerosim_workflow_tests aerosim_workflow_input_stamp_test)

add_executable(aerosim_workflow_dependency_blocker_test
    WorkflowDependencyBlockerTests.cpp)
target_link_libraries(aerosim_workflow_dependency_blocker_test
    PRIVATE aerosim::workflow Qt5::Core)
add_test(NAME workflow_dependency_blockers
    COMMAND aerosim_workflow_dependency_blocker_test)
add_dependencies(aerosim_workflow_tests
    aerosim_workflow_dependency_blocker_test)

add_executable(aerosim_workflow_action_authorization_test
    WorkflowActionAuthorizationTests.cpp)
target_link_libraries(aerosim_workflow_action_authorization_test
    PRIVATE aerosim::workflow Qt5::Core)
add_test(NAME workflow_action_authorization
    COMMAND aerosim_workflow_action_authorization_test)
add_dependencies(aerosim_workflow_tests
    aerosim_workflow_action_authorization_test)

add_executable(aerosim_workflow_async_validation_test
    WorkflowAsyncValidationTests.cpp)
target_link_libraries(aerosim_workflow_async_validation_test
    PRIVATE aerosim::workflow Qt5::Core)
add_test(NAME workflow_async_validation
    COMMAND aerosim_workflow_async_validation_test)
add_dependencies(aerosim_workflow_tests
    aerosim_workflow_async_validation_test)

add_executable(aerosim_workflow_snapshot_test
    WorkflowSnapshotTests.cpp)
target_link_libraries(aerosim_workflow_snapshot_test
    PRIVATE aerosim::workflow Qt5::Core)
add_test(NAME workflow_snapshot_roundtrip
    COMMAND aerosim_workflow_snapshot_test)
add_dependencies(aerosim_workflow_tests
    aerosim_workflow_snapshot_test)

add_executable(aerosim_workflow_acceptance_test
    WorkflowAcceptanceTests.cpp)
target_link_libraries(aerosim_workflow_acceptance_test
    PRIVATE aerosim::workflow Qt5::Core)
add_test(NAME workflow_acceptance
    COMMAND aerosim_workflow_acceptance_test)
add_dependencies(aerosim_workflow_tests
    aerosim_workflow_acceptance_test)
