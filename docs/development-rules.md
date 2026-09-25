# Development Commands & Rules, TrekLink Firmware

This document details the exact commands to build, test, and manage the **TrekLink Firmware** codebase using PlatformIO.

---

## 1. PlatformIO Build & Execution Commands

### Prerequisites

- **Python 3.9+**
- **PlatformIO Core (CLI)** (`pip install platformio` or installed via VS Code extension)

### Build Commands

From the repository root (`/`):

| Action                           | Command                         |
| -------------------------------- | ------------------------------- |
| Build default environment        | `pio run`                       |
| Build specific board environment | `pio run -e <environment_name>` |
| Clean build artifacts            | `pio run -t clean`              |
| Verbose build                    | `pio run -v`                    |

---

## 2. Flashing & Testing Commands

| Action                            | Command                                   |
| --------------------------------- | ----------------------------------------- |
| Upload to connected board         | `pio run -t upload`                       |
| Upload to specific board target   | `pio run -e <env> -t upload`              |
| Open Serial Monitor               | `pio device monitor`                      |
| Upload & Open Serial Monitor      | `pio run -t upload && pio device monitor` |
| Run Unit Tests                    | `pio test`                                |
| Run tests on specific environment | `pio test -e <env>`                       |

---

## 3. Git Workflow Summary

### Branching Strategy

- `main`: Production release branch.
- `dev`: Primary integration branch.
- `feat/<scope>`: Feature development branches.

### Step-by-Step Workflow

1. **Sync with latest `dev`**:

   ```bash
   git checkout dev
   git pull origin dev
   ```

2. **Create feature branch**:

   ```bash
   git checkout -b feat/your-feature
   ```

3. **Verify build before commit**:

   ```bash
   pio run
   ```

4. **Rebase against origin/dev**:

   ```bash
   git fetch origin dev
   git rebase origin/dev
   ```

5. **Push and create PR**:
   ```bash
   git push origin feat/your-feature --force-with-lease
   ```

---

## 4. Pre-Commit Verification Checklist

Before submitting a commit or PR, ensure:

- [ ] **Compilation**: `pio run` completes with zero errors.
- [ ] **Tests**: `pio test` passes for target platforms.
- [ ] **Git Status**: `git status` verifies no temporary files or build artifacts (`.pio/`) are tracked.
- [ ] **Clean Code**: `git diff` shows no hardcoded credentials, debug print statements, or dead code.
- [ ] **Formatting**: Code follows C++/Embedded conventions.
