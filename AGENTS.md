# Automation and contributor safeguards

- GitHub code review, source changes and commits are permitted when requested.
- **Do not execute GitHub Actions/CI, builds, tests, CTest, CMake configure or hardware diagnostics without the owner's fresh explicit authorization for the specific execution.**
- Do not add workflow triggers other than \`workflow_dispatch\` unless explicitly directed; avoid implicit automatic CI on push/PR.
- Preserve the independence of the GPL native bridge and browser extensions from the Lagosystem proprietary ERP.
- All browser origins and native devices require explicit scoped approval; never add wildcard permissions in native host registration.
- Do not publish private ERP files, credentials, certificate material or unrelated parent repository history.
