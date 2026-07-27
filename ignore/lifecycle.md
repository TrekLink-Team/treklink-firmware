# Development Lifecycle — TrekLink Firmware

## 1. Project Workflow & Development Lifecycle

The **TrekLink Firmware** project is built using C++ and PlatformIO for embedded hardware (ESP32 / NRF52).

- **Architecture Standards**: Modular firmware structure with strict separation between hardware variant definitions (`variants/`), platform configuration (`platformio.ini`), and core application logic (`src/`).
- **Build System**: PlatformIO handles cross-compilation, toolchain dependencies, and hardware flashing.
- **Continuous Integration**: GitHub Actions validates that all target board environments compile cleanly on PR creation against `dev`.

---

## 2. Local Execution

1. **Install Prerequisites**: Install PlatformIO Core (`pip install platformio`) or PlatformIO IDE extension in VS Code.
2. **Compile Firmware**:
   ```bash
   pio run
   ```
3. **Flash Connected Target Board**:
   ```bash
   pio run -t upload
   ```
4. **Monitor Serial Output**:
   ```bash
   pio device monitor
   ```

---

## 3. Git & Code Commit Rules

1. **Branch Naming Constraint**:
   - Feature branches must use `feat/<scope>` or `dev/<username>-<feature>`.
   - Never push directly to `main` or `dev` on origin.
2. **Commit Style Guidelines**:
   - Conventional Commits: `feat(scope): ...`, `fix(scope): ...`, `refactor(scope): ...`, `chore(scope): ...`.
3. **Pull Request Workflow**:
   - Rebase feature branches against `origin/dev` before pushing:
     ```bash
     git fetch origin dev
     git rebase origin/dev
     git push origin feat/your-feature --force-with-lease
     ```
   - Open Pull Request targeting `dev` branch on GitHub.
