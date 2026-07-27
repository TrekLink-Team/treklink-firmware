# Tool Reference Guide

This document provides a complete catalog of all tools available to Kiro's AI agent, including usage patterns, constraints, and platform-specific considerations. Understanding these tools is essential for effective task execution and maintaining code quality.

## Tool Categories

Tools are organized into six categories:
- **File Operation Tools**: Create, modify, and delete files
- **File Reading Tools**: Read file contents
- **Search Tools**: Find files and search content
- **Directory Tools**: List and navigate directories
- **Execution Tools**: Run shell commands
- **Interaction Tools**: Communicate with users
- **Task Management Tools**: Track task status

## File Operation Tools

### fsWrite

Creates a new file or overwrites an existing file with the specified content.

**Parameters:**
- `path` (required): Path to file relative to workspace root (e.g., 'file.py', 'src/main.ts')
- `text` (required): Contents to write into the file

**Usage Guidelines:**
- Use for creating new files
- Use for completely replacing file contents
- For files larger than 50 lines, write initial content with fsWrite, then use fsAppend for additional content
- Automatically creates parent directories if they don't exist

**Example:**
```typescript
// Create a new TypeScript file
fsWrite({
  path: "src/models/user.ts",
  text: `export interface User {
  id: string;
  name: string;
  email: string;
}

export class UserService {
  // Implementation
}`
});
```

**Constraints:**
- Will overwrite existing files without warning
- Content should be complete and syntactically correct
- For large files, prefer fsWrite + fsAppend pattern

### fsAppend

Adds content to the end of an existing file, automatically adding a newline if the file doesn't end with one.

**Parameters:**
- `path` (required): Path to file relative to workspace root
- `text` (required): Contents to append to the end of the file

**Usage Guidelines:**
- Use for adding content to existing files
- Use in combination with fsWrite for large files (>50 lines)
- Automatically handles newline management
- File must already exist

**Example:**
```typescript
// Add a new method to existing class
fsAppend({
  path: "src/models/user.ts",
  text: `
  async findById(id: string): Promise<User | null> {
    // Implementation
    return null;
  }`
});
```

**Constraints:**
- File must exist before appending
- Cannot insert content at specific positions (use strReplace for that)
- Automatically adds newline if file doesn't end with one

### strReplace

Replaces specific text in an existing file. Especially useful for editing portions of large files while leaving the rest unchanged.

**Parameters:**
- `path` (required): Path to file where replacement is requested
- `oldStr` (required): Exact text to find and replace
- `newStr` (required): Text that will replace oldStr

**Usage Guidelines:**
- Use for editing specific sections of existing files
- Include 2-3 lines of context before and after the change point
- Ensure oldStr matches exactly (including whitespace)
- oldStr must uniquely identify a single location in the file
- For multiple independent changes, invoke strReplace multiple times in parallel

**Example:**
```typescript
// Replace a method implementation
strReplace({
  path: "src/services/auth.ts",
  oldStr: `  async login(email: string, password: string): Promise<User> {
    // TODO: Implement
    throw new Error("Not implemented");
  }`,
  newStr: `  async login(email: string, password: string): Promise<User> {
    const user = await this.userRepo.findByEmail(email);
    if (!user || !await this.verifyPassword(password, user.passwordHash)) {
      throw new Error("Invalid credentials");
    }
    return user;
  }`
});
```

**Critical Constraints:**
- **EXACT MATCHING**: oldStr must match EXACTLY including all whitespace, tabs, and line endings
- **WHITESPACE**: All whitespace must match exactly (critical for indentation-sensitive languages like Python)
- **UNIQUENESS**: oldStr must uniquely identify a single instance in the file
- **CONTEXT**: Include sufficient context (2-3 lines) before and after the change
- **DIFFERENCE**: oldStr and newStr MUST be different
- **PARALLEL EXECUTION**: For multiple independent replacements, invoke strReplace multiple times simultaneously

**Common Pitfalls:**
- Forgetting to match indentation exactly
- Not including enough context (causing multiple matches)
- Including too much context (making the match too fragile)
- Mismatching line endings or trailing whitespace

### deleteFile

Deletes a file at the specified path.

**Parameters:**
- `targetFile` (required): Path of the file to delete, relative to workspace root
- `explanation` (required): One sentence explanation of why the file is being deleted

**Usage Guidelines:**
- Use when removing obsolete or incorrect files
- Operation fails gracefully if file doesn't exist
- Cannot be undone (use with caution)

**Example:**
```typescript
deleteFile({
  targetFile: "src/temp/debug.log",
  explanation: "Removing temporary debug log file that is no longer needed"
});
```

**Constraints:**
- Cannot delete directories (only files)
- Fails gracefully if file doesn't exist
- Requires explanation for audit trail

## File Reading Tools

### readFile

Reads the content of a single file with optional line range specification.

**Parameters:**
- `path` (required): Path to file relative to workspace root
- `explanation` (required): One sentence explanation of why this file is being read
- `start_line` (optional): Starting line number (default: 1)
- `end_line` (optional): Ending line number (default: -1, meaning end of file)

**Usage Guidelines:**
- Prefer reading entire files over line ranges
- Use line ranges only when absolutely necessary for large files
- For reading multiple files, use readMultipleFiles instead

**Example:**
```typescript
// Read entire file
readFile({
  path: "src/config/database.ts",
  explanation: "Reading database configuration to understand connection settings"
});

// Read specific lines
readFile({
  path: "src/models/user.ts",
  start_line: 10,
  end_line: 25,
  explanation: "Reading User class definition to understand the data model"
});
```

**Constraints:**
- Path must be relative to workspace root
- File must exist
- Line numbers are 1-indexed
- Negative line numbers count from end of file

### readMultipleFiles

Reads the content of multiple files with optional line range specification.

**Parameters:**
- `paths` (required): Array of file paths relative to workspace root
- `explanation` (required): One sentence explanation of why these files are being read
- `start_line` (optional): Starting line number for all files
- `end_line` (optional): Ending line number for all files

**Usage Guidelines:**
- **PREFERRED** over multiple single-file reads
- Use when you need to read several related files
- More efficient than multiple readFile calls
- Line ranges apply to all files

**Example:**
```typescript
// Read multiple related files
readMultipleFiles({
  paths: [
    "src/models/user.ts",
    "src/services/user-service.ts",
    "src/repositories/user-repository.ts"
  ],
  explanation: "Reading user-related files to understand the complete user management implementation"
});
```

**Constraints:**
- All paths must be relative to workspace root
- All files must exist
- Line ranges apply to all files equally
- Prefer this over multiple readFile calls

## Search Tools

### grepSearch

Fast text-based regex search that finds exact pattern matches within files using ripgrep. Results include line numbers, file paths, and 2 lines of context around each match.

**Parameters:**
- `query` (required): The regex pattern to search for (Rust regex syntax)
- `explanation` (optional): Brief description of why this search is being performed
- `caseSensitive` (optional): true for case-sensitive, false or omit for case-insensitive
- `includePattern` (optional): Glob pattern for files to include (e.g., '*.ts', 'src/**/*.js')
- `excludePattern` (optional): Glob pattern for files to exclude (e.g., '*.log', 'node_modules/**')

**Usage Guidelines:**
- Use for finding specific text patterns in code
- Use includePattern to narrow search scope for better performance
- Keep regex patterns simple (complex patterns may fail)
- Always escape special regex characters: ( ) [ ] { } + * ? ^ $ | . \
- Results capped at 50 matches

**Example:**
```typescript
// Search for function definitions in TypeScript files
grepSearch({
  query: "function\\s+\\w+",
  includePattern: "*.ts",
  explanation: "Finding all function definitions to understand the codebase structure"
});

// Search for TODO comments
grepSearch({
  query: "TODO:",
  explanation: "Finding all TODO comments to identify pending work"
});

// Search for import statements
grepSearch({
  query: "^import",
  includePattern: "src/**/*.ts",
  explanation: "Finding all import statements to understand dependencies"
});
```

**Valid Query Patterns:**
- Basic text: `"function"`, `"error"`, `"TODO"`
- Word boundaries: `"\\bword\\b"` (matches 'word' but not 'password')
- Multiple words: `"auth.*failed"`
- Line starts with: `"^import"`
- Line ends with: `"};$"`
- Numbers: `"\\d+\\.\\d+"` (finds decimal numbers like 3.14)
- Function declarations: `"function\\s+\\w+"`

**Constraints:**
- Must escape special regex characters with \\
- Results capped at 50 matches
- Long lines truncated with "[truncated: line too long]"
- Too many matches truncated with "[truncated: too many matches]"
- **NEVER use bash 'grep' command** - always use this tool instead

### fileSearch

Fast file search based on fuzzy matching against file paths. Use when you know part of the file path but don't know the exact location.

**Parameters:**
- `query` (required): The regex pattern to search for in file paths
- `explanation` (required): Description of what file you're looking for
- `includeIgnoredFiles` (optional): "yes" or "no" for whether to include .gitignore files
- `excludePattern` (optional): Glob pattern for files to exclude

**Usage Guidelines:**
- Use when you know part of a filename but not its location
- Results capped at 10 matches
- Make query more specific if you need to filter results further
- Searches file paths, not file contents

**Example:**
```typescript
// Find configuration files
fileSearch({
  query: "config",
  explanation: "Looking for configuration files in the project"
});

// Find test files for a specific module
fileSearch({
  query: "user.*test",
  explanation: "Finding test files related to user functionality"
});
```

**Constraints:**
- Results capped at 10 matches
- Searches file paths only (not contents)
- Use grepSearch for content searches

## Directory Tools

### listDirectory

Lists directory contents in "long format" similar to `ls -la`, with optional recursive listing.

**Parameters:**
- `path` (required): Path to directory (absolute or starting with ~)
- `explanation` (required): One sentence explanation of why listing this directory
- `depth` (optional): Depth of recursive listing

**Usage Guidelines:**
- Use to understand project structure
- Use depth parameter for recursive listing
- Shows file permissions, sizes, and timestamps

**Example:**
```typescript
// List workspace root
listDirectory({
  path: ".",
  explanation: "Listing workspace root to understand project structure"
});

// Recursive listing
listDirectory({
  path: "src",
  depth: 2,
  explanation: "Recursively listing src directory to understand code organization"
});
```

**Constraints:**
- Path should be absolute or start with ~
- Depth parameter controls recursion level
- Shows hidden files (like .git)

## Execution Tools

### executePwsh

Executes shell commands (CMD or PowerShell on Windows).

**Parameters:**
- `command` (required): Shell command to execute
- `path` (optional): Path to run the command in (relative to workspace root)

**Usage Guidelines:**
- Avoid using for search and discovery (use grepSearch, fileSearch, readFile instead)
- Avoid using for file writing (use fsWrite, fsAppend instead)
- **NEVER use 'cd' command** - use path parameter instead
- Use path parameter to run commands in subdirectories
- Adapt commands to platform (Windows/Linux/Mac)

**Example:**
```typescript
// Run tests
executePwsh({
  command: "npm test",
  explanation: "Running test suite to verify implementation"
});

// Run command in subdirectory
executePwsh({
  command: "tsc --noEmit",
  path: "src",
  explanation: "Type-checking TypeScript files in src directory"
});
```

**Platform-Specific Commands:**

**Windows (PowerShell):**
- List files: `Get-ChildItem`
- Remove file: `Remove-Item file.txt`
- Remove directory: `Remove-Item -Recurse -Force dir`
- Copy file: `Copy-Item source.txt destination.txt`
- Copy directory: `Copy-Item -Recurse source destination`
- Create directory: `New-Item -ItemType Directory -Path dir`
- View file: `Get-Content file.txt`
- Find in files: `Select-String -Path *.txt -Pattern "search"`
- Command separator: `;` (Always replace && with ;)

**Windows (CMD):**
- List files: `dir`
- Remove file: `del file.txt`
- Remove directory: `rmdir /s /q dir`
- Copy file: `copy source.txt destination.txt`
- Create directory: `mkdir dir`
- View file: `type file.txt`
- Command separator: `&`

**Linux/Mac (Bash):**
- List files: `ls -la`
- Remove file: `rm file.txt`
- Remove directory: `rm -rf dir`
- Copy file: `cp source.txt destination.txt`
- Copy directory: `cp -r source destination`
- Create directory: `mkdir -p dir`
- View file: `cat file.txt`
- Find in files: `grep -r "search" .`
- Command separator: `&&`

**Critical Constraints:**
- **NEVER use 'cd' command** - it will fail
- Use `path` parameter to run commands in subdirectories
- Prefer file tools over CLI commands for file operations
- Adapt commands to the detected platform
- Use `;` instead of `&&` on Windows PowerShell

## Interaction Tools

### userInput

Gets input from the user. Use when you need additional information or explicit approval to proceed.

**Parameters:**
- `question` (required): Question to ask the user (format in bold using **question text**)
- `reason` (optional): Reason for asking (used for spec workflow tracking)

**Usage Guidelines:**
- Use when stuck and need user input to proceed
- Use when you need explicit approval (especially in spec workflow)
- Format questions in bold using markdown syntax
- User can answer or skip the question

**Reason Codes (for spec workflow):**
- `spec-requirements-review`: Asking user to review requirements document
- `spec-design-review`: Asking user to review design document
- `spec-tasks-review`: Asking user to review tasks document

**Example:**
```typescript
// General question
userInput({
  question: "**Which authentication method would you like to use: JWT or session-based?**"
});

// Spec workflow approval
userInput({
  question: "**Do the requirements look good? If so, we can move on to the design.**",
  reason: "spec-requirements-review"
});
```

**Constraints:**
- Questions should be clear and specific
- Format questions in bold for visibility
- In spec workflow, must use appropriate reason code
- User can skip questions to proceed

## Task Management Tools

### taskStatus

Updates the status of tasks from the task list in spec files.

**Parameters:**
- `taskFilePath` (required): Path to tasks.md file relative to workspace root
- `task` (required): Task text that matches exactly from tasks.md
- `status` (required): Status to set ("not_started", "in_progress", "completed")

**Usage Guidelines:**
- Always set task to "in_progress" before starting work
- Always set task to "completed" when fully finished
- For tasks with sub-tasks, complete sub-tasks first, then parent task
- Task text must match exactly from tasks.md file

**Task Status States:**
- `not_started`: Task has not been started
- `in_progress`: Task is currently being worked on
- `completed`: Task is fully complete

**Example:**
```typescript
// Start a task
taskStatus({
  taskFilePath: ".kiro/specs/user-auth/tasks.md",
  task: "1.1 Create User model with validation",
  status: "in_progress"
});

// Complete a task
taskStatus({
  taskFilePath: ".kiro/specs/user-auth/tasks.md",
  task: "1.1 Create User model with validation",
  status: "completed"
});
```

**Constraints:**
- Task text must match EXACTLY from tasks.md (including task number)
- Example: If tasks.md has `  - [-] 3.2 This is my task`, use `"3.2 This is my task"`
- Never use with tasks not defined in the spec
- Complete sub-tasks before parent tasks
- Only use during task execution, not during spec creation

## Tool Selection Guidelines

### When to Use Which Tool

**For File Creation:**
- New file or complete replacement → `fsWrite`
- Adding to existing file → `fsAppend`
- Editing specific section → `strReplace`

**For File Reading:**
- Single file → `readFile`
- Multiple files → `readMultipleFiles` (preferred)
- Entire file → omit line range parameters
- Specific lines → use start_line and end_line (only when necessary)

**For Searching:**
- Finding text in files → `grepSearch`
- Finding files by name → `fileSearch`
- Understanding structure → `listDirectory`

**For Execution:**
- Running build/test commands → `executePwsh`
- File operations → Use file tools instead
- Search operations → Use search tools instead

**For Interaction:**
- Need user input → `userInput`
- Spec workflow approval → `userInput` with reason code

**For Task Management:**
- Starting task → `taskStatus` with "in_progress"
- Completing task → `taskStatus` with "completed"

### Tool Selection Decision Tree

```
Need to work with files?
├─ Creating new file? → fsWrite
├─ Adding to existing file? → fsAppend
├─ Editing specific section? → strReplace
├─ Deleting file? → deleteFile
└─ Reading file(s)?
   ├─ Single file? → readFile
   └─ Multiple files? → readMultipleFiles

Need to search?
├─ Search file contents? → grepSearch
├─ Search file names? → fileSearch
└─ List directory? → listDirectory

Need to execute command?
├─ Build/test/run? → executePwsh
├─ File operation? → Use file tools instead
└─ Search? → Use search tools instead

Need user input?
├─ General question? → userInput
└─ Spec approval? → userInput with reason code

Working with tasks?
├─ Starting task? → taskStatus (in_progress)
└─ Finishing task? → taskStatus (completed)
```

## Common Patterns

### Pattern: Create Large File

```typescript
// Step 1: Write initial content (first 50 lines)
fsWrite({
  path: "src/large-file.ts",
  text: `// Initial content
// ... (up to ~50 lines)`
});

// Step 2: Append remaining content
fsAppend({
  path: "src/large-file.ts",
  text: `// Additional content
// ... (remaining lines)`
});
```

### Pattern: Edit Multiple Sections

```typescript
// Make multiple independent edits in parallel
strReplace({
  path: "src/file.ts",
  oldStr: "// First section to edit",
  newStr: "// First section edited"
});

strReplace({
  path: "src/file.ts",
  oldStr: "// Second section to edit",
  newStr: "// Second section edited"
});
```

### Pattern: Read Related Files

```typescript
// Read all related files at once
readMultipleFiles({
  paths: [
    "src/models/user.ts",
    "src/services/user-service.ts",
    "src/repositories/user-repository.ts"
  ],
  explanation: "Reading user-related files to understand implementation"
});
```

### Pattern: Search and Edit

```typescript
// Step 1: Search for pattern
grepSearch({
  query: "TODO: implement",
  includePattern: "src/**/*.ts",
  explanation: "Finding TODO comments to implement"
});

// Step 2: Read file with TODO
readFile({
  path: "src/services/auth.ts",
  explanation: "Reading file to understand context for TODO"
});

// Step 3: Edit file
strReplace({
  path: "src/services/auth.ts",
  oldStr: "// TODO: implement login",
  newStr: "async login(email: string, password: string) { /* implementation */ }"
});
```

### Pattern: Task Execution

```typescript
// Step 1: Mark task as in progress
taskStatus({
  taskFilePath: ".kiro/specs/feature/tasks.md",
  task: "1.1 Implement User model",
  status: "in_progress"
});

// Step 2: Implement the task
fsWrite({
  path: "src/models/user.ts",
  text: "// Implementation"
});

// Step 3: Mark task as completed
taskStatus({
  taskFilePath: ".kiro/specs/feature/tasks.md",
  task: "1.1 Implement User model",
  status: "completed"
});
```

## Anti-Patterns (What NOT to Do)

### ❌ Using 'cd' Command

```typescript
// WRONG - will fail
executePwsh({
  command: "cd src && npm test"
});

// CORRECT - use path parameter
executePwsh({
  command: "npm test",
  path: "src"
});
```

### ❌ Using CLI for File Operations

```typescript
// WRONG - use file tools instead
executePwsh({
  command: "echo 'content' > file.txt"
});

// CORRECT - use fsWrite
fsWrite({
  path: "file.txt",
  text: "content"
});
```

### ❌ Using CLI for Search

```typescript
// WRONG - use search tools instead
executePwsh({
  command: "grep -r 'pattern' ."
});

// CORRECT - use grepSearch
grepSearch({
  query: "pattern",
  explanation: "Searching for pattern in codebase"
});
```

### ❌ Multiple readFile Calls

```typescript
// WRONG - inefficient
readFile({ path: "file1.ts", explanation: "Reading file 1" });
readFile({ path: "file2.ts", explanation: "Reading file 2" });
readFile({ path: "file3.ts", explanation: "Reading file 3" });

// CORRECT - use readMultipleFiles
readMultipleFiles({
  paths: ["file1.ts", "file2.ts", "file3.ts"],
  explanation: "Reading related files"
});
```

### ❌ strReplace Without Context

```typescript
// WRONG - not unique, may match multiple locations
strReplace({
  path: "src/file.ts",
  oldStr: "return null;",
  newStr: "return user;"
});

// CORRECT - include context
strReplace({
  path: "src/file.ts",
  oldStr: `  async findById(id: string): Promise<User | null> {
    // TODO: implement
    return null;
  }`,
  newStr: `  async findById(id: string): Promise<User | null> {
    const user = await this.db.users.findOne({ id });
    return user;
  }`
});
```

### ❌ Forgetting Task Status Updates

```typescript
// WRONG - implementing without status updates
fsWrite({
  path: "src/models/user.ts",
  text: "// Implementation"
});

// CORRECT - update status before and after
taskStatus({ taskFilePath: "...", task: "...", status: "in_progress" });
fsWrite({
  path: "src/models/user.ts",
  text: "// Implementation"
});
taskStatus({ taskFilePath: "...", task: "...", status: "completed" });
```

## Platform-Specific Considerations

### Windows Considerations

- Use `;` instead of `&&` for command chaining in PowerShell
- Use `&` for command chaining in CMD
- Path separators: use forward slashes `/` in tool paths (automatically converted)
- PowerShell commands are more verbose than Unix equivalents
- Some Unix commands don't exist (use PowerShell equivalents)

### Linux/Mac Considerations

- Use `&&` for command chaining
- Path separators: forward slashes `/`
- More concise command syntax
- Standard Unix tools available

### Cross-Platform Best Practices

- Always use forward slashes in file paths (works on all platforms)
- Prefer file tools over platform-specific CLI commands
- Use executePwsh for build/test commands (adapt to platform)
- Test commands on target platform before using
- Document platform-specific behavior when necessary

## Summary

This tool reference provides the complete catalog of tools available to Kiro's AI agent. Key takeaways:

1. **Prefer specialized tools**: Use file tools over CLI, search tools over grep
2. **Never use 'cd'**: Use path parameter instead
3. **Read multiple files at once**: Use readMultipleFiles over multiple readFile calls
4. **Include context in strReplace**: Ensure uniqueness with 2-3 lines of context
5. **Update task status**: Always mark tasks in_progress and completed
6. **Adapt to platform**: Use platform-specific commands when necessary
7. **Escape regex characters**: Always escape special characters in search patterns
8. **Format user questions**: Use bold markdown for visibility

Understanding these tools and their constraints is essential for effective task execution and maintaining code quality throughout the AI-Driven Development Lifecycle.
