# TrekLink Firmware — Documentation & Conventions

This directory contains the documentation, coding standards, and workflow rules for **TrekLink Firmware**.

---

## 1. Documentation Index

- [AGENT.md](./AGENT.md): Overview of developer identity, repository context, and core architecture.
- [commit-rules.md](./commit-rules.md): Git branching strategy, conventional commit standards, and PR rebase workflow.
- [conventions.md](./conventions.md): C++17 embedded firmware coding standards, memory safety, and naming conventions.
- [development-rules.md](./development-rules.md): PlatformIO build, flash, test, and CLI commands.
- [lifecycle.md](./lifecycle.md): Development lifecycle and build verification steps.

---

## 2. Quick Start

```bash
# Build default firmware target
pio run

# Build specific hardware target
pio run -e <environment>

# Flash connected hardware
pio run -t upload

# Run test suite
pio test
```

---

## 3. Git Workflow Summary

1. Create branch from `dev`: `git checkout -b feat/my-feature`
2. Commit with Conventional Commits: `git commit -m "feat(mesh): description"`
3. Rebase against `origin/dev`: `git fetch origin dev && git rebase origin/dev`
4. Push and open PR targeting `dev` branch: `git push origin feat/my-feature --force-with-lease`
