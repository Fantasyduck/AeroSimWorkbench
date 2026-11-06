# AeroSimWorkbench Constitution

## Core Principles

### I. Enforce Layered Architecture
All production changes MUST preserve the dependency direction UI Layer → Service Layer →
Infrastructure & Adapter Layer. `modules/ui/` MUST only present state and submit operation
requests. Business rules, validation, and scheduling MUST reside in their responsible services.
`app/ApplicationController` MUST be limited to object composition, dependency injection, and
system-level connections. Services MUST NOT depend on `QWidget`, the main window, or VTK display
widgets, and public CLI services MUST remain usable without a GUI. This separation keeps business
logic independently testable and prevents UI or rendering technology from controlling the domain.

### II. Review Shared Contracts Before Adoption
Cross-module project, workflow, task, mesh, and result concepts MUST reuse the contracts in
`contracts/`; modules MUST NOT create competing copies of shared types. A contract change MUST
identify affected callers, compatibility impact, ownership, lifetime, and thread semantics before
implementation. Large VTK objects and mesh arrays MUST NOT enter public contracts. Draft
post-processing layouts, scene operations, CMake targets, and feature ports MUST remain proposals
until an explicit interface review approves them; only the existing lightweight `DatasetRef` and
`SceneObjectRef` may be treated as established on that basis. This prevents accidental API
freezing and incompatible representations across modules.

### III. Preserve Thread, UI, and Object Lifecycles
Cross-thread Qt signals and slots MUST use `Qt::QueuedConnection`, carry lightweight messages by
value, and register custom metatypes before use. Widget creation and VTK rendering MUST occur on
the GUI thread. Every asynchronous object and connection MUST have explicit ownership, shutdown,
and cleanup behavior; integrations MUST prevent duplicate object creation and duplicate signal
connections. Reviews MUST reject changes whose thread affinity or ownership cannot be determined
from the code and contract. These rules protect the application from races, use-after-free defects,
and nondeterministic shutdown failures.

### IV. Build and Test Modules Independently
Each production module MUST define its own target, direct dependencies, and a focused test or
executable demonstration before top-level integration. A module MUST remain buildable without
unrelated optional stacks; in particular, headless and Qt Core services MUST NOT require VTK,
SARibbon, or GUI targets. Full builds MUST run CTest after successful compilation. `mocks/` MUST
be clearly identified as placeholders and their tests MUST claim only assembly and asynchronous
message coverage, never real solver, data-reader, numerical, or post-processing correctness.
Production post-processing MUST use C++, Qt 5.12.12, and VTK 8.2.0. This makes failures local,
test claims auditable, and incremental replacement of placeholders safe.

### V. Maintain a Reproducible Toolchain Baseline
The supported development baseline MUST remain C++14, Qt 5.12.12, Windows VS2017 v141 x64 Debug,
CMake 3.18.2, VTK 8.2.0 Debug exports, and SARibbon 2.9.3 until a reviewed amendment records new
validation evidence. Targets MUST declare the dependencies they use and MUST NOT embed personal
absolute paths. Changes MUST NOT mix compiler ABI, architecture, or incompatible Debug and Release
libraries. The unresolved nlohmann/json version and logging-library choice MUST NOT be silently
decided by an unrelated feature. Any broader platform or Release claim MUST be backed by a dated,
repeatable build record. A fixed, evidence-based baseline prevents machine-specific success from
being mistaken for supported integration.

## Technology and Integration Constraints

- Windows development and integration MUST use `scripts/build-windows.cmd <target|all> Debug`.
  A headless build MUST support `AEROSIM_BUILD_GUI=OFF` where applicable.
- Linux or macOS full-stack support and Windows VTK Release support MUST be described as
  unverified until repeatable evidence is added; the available VTK 8.2.0 Debug exports MUST NOT be
  linked into Release targets.
- Formal post-processing code MUST remain in `modules/visualization/`; configuration, logging,
  runtime, project, and workflow responsibilities MUST remain in their named modules.
- Third-party dependencies MUST be declared by the consuming target. A dependency change MUST
  record exact version, source, license, compiler and architecture, build configuration, CMake
  package identity, and integration-test result.
- Existing public interfaces and general-purpose script execution unrelated to the
  post-processing prototype MUST be preserved unless a reviewed migration explicitly supersedes
  them.
- Documentation status labels are binding: a proposal or historical validation record MUST NOT be
  cited as an approved current interface or current platform guarantee.

## Development Workflow and Quality Gates

1. Select task-specific references through `docs/README.md`; consult module README files and the
   applicable integration or environment guide before changing their scope.
2. For contract changes, document consumers, compatibility, threading, lifetime, and migration,
   then obtain interface review before changing contracts, mocks, or implementations.
3. Implement production modules behind their declared boundaries. Keep the application controller
   to composition and keep UI code free of duplicated service rules.
4. Build and test the affected module independently. Then run the relevant top-level build and
   CTest suite. Missing matching dependencies or unverified platforms MUST be reported, not
   bypassed or represented as passing.
5. A review MUST verify dependency direction, direct target dependencies, thread affinity,
   ownership and cleanup, contract compatibility, test-claim accuracy, and absence of personal
   paths or generated build artifacts.
6. A pull request MUST state behavior changes, affected contracts and dependencies, exact build
   and test commands with results, and any unverified platform. UI changes MUST include adequate
   interface evidence; dependency changes with binary or license impact MUST be called out.

## Governance

This constitution is the highest-priority project governance document. When it conflicts with
module guidance, drafts, or historical records, the constitution governs; code and newer verified
evidence determine facts that the constitution does not settle.

An amendment MUST be proposed as a focused documentation change that states its motivation,
affected principles, compatibility or migration impact, and validation evidence. Approval requires
review by the project maintainers responsible for the affected architecture or toolchain area. A
change that removes or redefines a principle requires a MAJOR version increment; a new principle,
section, or materially expanded obligation requires MINOR; a non-semantic clarification requires
PATCH. The amendment date MUST be updated whenever normative text changes, while the original
ratification date remains fixed.

Every feature specification, implementation plan, task list, and pull request review MUST include
a constitution compliance check. Any exception MUST be explicit, time-bounded, assigned to an
owner, and include a remediation or migration plan; undocumented exceptions are non-compliant.
Reviewers MUST use `AGENTS.md`, `docs/README.md`, and the task-specific documents it identifies for
operational guidance, while preserving the precedence defined here.

**Version**: 1.0.0 | **Ratified**: 2026-11-06 | **Last Amended**: 2026-11-06
