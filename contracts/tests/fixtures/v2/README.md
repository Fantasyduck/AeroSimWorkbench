# V2 fixture policy

V2 fixtures are introduced together with the public V2 type and its reviewed
serializer. Each fixture must use stable string enum tokens, opaque IDs,
aggregate-specific revision values, and an explicit schemaVersion.

PR 0 intentionally contains no V2 data files because the V2 public structures
and persistence adapter have not been implemented yet.
