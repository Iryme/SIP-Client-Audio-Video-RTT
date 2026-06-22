# Development Workflow

## Branch Strategy

- `main` — stable, always builds
- `feature/<task-name>` — one branch per task
- Never commit directly to `main`

## Git Flow Per Task

```bash
git checkout main
git pull
git checkout -b feature/<task-name>

# implement task
# update docs

git status
git add <specific files>
git commit -m "<clear message>"
git push -u origin feature/<task-name>
```

Report after each task: branch name, commit hash, files changed, next step.

## Task Size

One task = one coherent feature or fix. Small atomic commits within a branch are acceptable. Do not mix unrelated changes.

## Build Verification

Before committing:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Fix all warnings at error-level (`-Wall -Wextra` for GCC/Clang, `/W4` for MSVC).

## Documentation

Every task must update relevant documentation:
- Feature added → update `docs/project-status.md`
- Architecture changed → update `docs/architecture.md`
- New module → add corresponding doc file
- API changed → update relevant `docs/*.md`

## Code Style

- C++17
- Qt naming conventions for Qt objects
- `m_` prefix for private members
- No raw `new` without clear ownership (prefer `new` with parent, or smart pointers for non-Qt objects)
- No blocking calls on the GUI thread
- No hardcoded server addresses or credentials

## Testing

- Run Qt Test suite when available: `cmake --build build --target test`
- Manual UI smoke test before pushing

## Multi-Machine Development

This project is developed from multiple machines. Before starting any task:

```bash
git checkout main
git pull
```

Never force-push to `main` or rewrite shared history.

## Project Handoff

Every 10 completed tasks, generate a handoff document:
```
docs/project-handoff/handoff-<NNN>.md
```
Commit and push. Open a new conversation using the handoff content as context.
