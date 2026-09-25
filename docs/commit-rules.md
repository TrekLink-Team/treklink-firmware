# Commit & Branch Rules, TrekLink Firmware

## ⚠️ CRITICAL: BRANCH NAMING & PR WORKFLOW

### Target Repository & Integration Branches

- **Organization**: `TrekLink-Team`
- **Repository**: `treklink-firmware`
- **Protected Mainline Branches**:
  - `main`: Stable production releases.
  - `dev`: Primary integration branch for active development.

### Branch Naming Convention

- **Feature Branches**: `feat/<scope>` or `dev/<username>-<feature>`
  - Examples: `feat/lora-driver`, `fix/ble-reconnect`, `refactor/power-mgmt`, `dev/khoa-mesh-sync`
- **Bugfix Branches**: `fix/<bug-name>`
- **Refactor / Chore**: `refactor/<scope>` or `chore/<scope>`
- ⚠️ **NEVER push directly to `main` or `dev` on origin.** All changes must land via Pull Requests.

---

## Merge Request / Pull Request Workflow

1. **Checkout and update the latest `dev` branch**:

   ```bash
   git switch dev
   git pull origin dev
   ```

2. **Create your feature branch from `dev`**:

   ```bash
   git checkout -b feat/your-feature-name
   ```

3. **Develop and commit locally using Conventional Commits**:

   ```bash
   git commit -m "feat(mesh): add adaptive power control"
   ```

4. **Sync with latest `dev` before pushing (Rebase Flow)**:

   ```bash
   git fetch origin dev
   git rebase origin/dev
   ```

5. **Push to remote feature branch**:

   ```bash
   git push origin feat/your-feature-name --force-with-lease
   ```

6. **Open GitHub Pull Request (PR)**:
   - **Source branch**: `feat/your-feature-name`
   - **Target branch**: `dev`
   - **Title**: Conventional commit title (e.g. `feat(mesh): add adaptive power control`)
   - **Description**: Summary of hardware/firmware changes, impacted targets, testing completed.

---

## Rebase & Divergence Conflict Resolution

If conflicts occur during `git rebase origin/dev`:

1. **Stash uncommitted changes** (if any):

   ```bash
   git stash
   ```

2. **Fetch latest changes**:

   ```bash
   git fetch origin dev
   ```

3. **Rebase against `origin/dev`**:

   ```bash
   git rebase origin/dev
   ```

4. **If conflicts occur**:
   - Inspect status: `git status`
   - Resolve conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`) in source files.
   - Stage resolved files: `git add <resolved-file>`
   - Continue rebase (Do **NOT** run `git commit`):
     ```bash
     git rebase --continue
     ```
   - _(Optional abort)_: `git rebase --abort`

5. **Pop stashed changes** (if stashed in Step 1):

   ```bash
   git stash pop
   ```

6. **Push updated branch**:
   ```bash
   git push origin feat/your-feature-name --force-with-lease
   ```

---

## ⛔ NEVER DO

- ❌ Never push directly to `main` or `dev` on origin.
- ❌ Never merge locally into `dev` or `main`, all merges occur via GitHub Pull Requests.
- ❌ Never commit binary compilation artifacts (`.pio/`, build outputs, binaries) to git.

---

## Commit Style (Conventional Commits)

Commit messages must follow the structure: `<type>(<scope>): <short description>`

### Types:

- `feat`: A new firmware feature or hardware driver support.
- `fix`: Bug fix in firmware, protocol, or build system.
- `refactor`: Code restructuring without changing behavior.
- `perf`: Performance or power consumption optimization.
- `docs`: Documentation updates.
- `chore`: Build script, PlatformIO config, or dependencies update.
- `test`: Unit or hardware integration test updates.

### Examples:

- `feat(lora): add SX1262 frequency hopping support`
- `fix(ble): resolve connection drop timeout on ESP32`
- `refactor(power): optimize deep sleep entry sequence`
- `chore(deps): update RadioLib dependency version`
