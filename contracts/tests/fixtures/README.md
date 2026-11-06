# Contract migration fixtures

This directory reserves the fixture layout used by additive contract migrations.
PR 0 creates only the structure; it does not introduce a JSON library or claim
that a V2 persistence schema exists.

- `legacy/`: committed input produced by an already supported schemaVersion.
- `v2/`: expected V2 input/output added with the PR that implements that schema.

Every future fixture must state its `schemaVersion`, the owning aggregate, the
expected migration result, and whether unknown fields must be preserved or
rejected. Binary meshes, solver output, and personal absolute paths do not belong
in this contract-only fixture directory.
