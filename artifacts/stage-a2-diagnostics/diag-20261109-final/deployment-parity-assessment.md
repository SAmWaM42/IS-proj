# Deployment-parity assessment

1. The repository currently defines both researcher and participant build paths in `CMakeLists.txt`.
2. The participant-facing build is behind the `ANIMAL_AGENT_PARTICIPANT_BUILD` option, and the participant binary is emitted under `build-participant-linux/AnimalAgent`.
3. The production deployment environment and Vercel project were explicitly excluded from this review and were not modified.
4. The unresolved issue is an artifact/export integrity issue, not a deployment-binary issue.
5. Until a fresh, complete provenance-linked training run is produced, deployment parity should remain unchanged.

Assessment: no deployment or production changes should be approved at this stage.
