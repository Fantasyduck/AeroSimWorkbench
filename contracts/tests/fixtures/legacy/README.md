# Legacy fixture policy

Legacy fixtures are immutable inputs for compatibility tests. Add an aggregate
subdirectory only when its current reader and actual historical shape are known.
Do not invent legacy JSON merely to satisfy a test.

Planned categories are project/resource references, workflow snapshots, single
tasks using `caseId`, batch tasks using legacy `batchId`, and telemetry without
`attemptId`. Concrete files are deferred to the PR that implements the matching
reader or migrator.
