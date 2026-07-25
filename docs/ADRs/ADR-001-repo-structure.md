# ADR-001: Meta-Repo + Submodule Structure

## Status
Accepted

## Context
As a solo developer, I need a repository structure that balances:
- Clear separation of concerns (engine vs. game vs. tools)
- Ability to work on components in isolation
- Simple CI/CD without enterprise complexity
- Easy rollback and versioning per component

## Decision
Use a **Meta-Repo with Git Submodules**:
- `TheSeed/` (meta-repo) orchestrates builds, CI, docs
- Each major subsystem is a submodule (`seed-core`, `seed-network`, etc.)
- Meta-repo contains shared cmake modules, scripts, and cross-submodule tests

## Consequences

### Positive
- Solo-dev friendly: one `git clone --recursive` gets everything
- Each submodule can be built standalone for fast iteration
- CI runs on meta-repo, tests all submodules together
- Clear ownership per submodule

### Negative
- Submodule updates require explicit commits in meta-repo
- Slightly more complex initial setup
- Risk of submodule drift if not automated

## Mitigations
- Weekly automated submodule update PRs (`submodule_update.yml`)
- `seed_check_submodules()` CMake function validates initialization
- `setup.sh` script handles initial clone + submodule init
