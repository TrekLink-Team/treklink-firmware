# Integration Features

This document covers Kiro's integration capabilities that enhance the spec-driven development workflow. These features provide additional context, automation, and extensibility to the core methodology.

## Chat Context Features

Kiro provides several context mechanisms that allow you to bring specific information into the conversation. These context types help the agent understand your codebase, current state, and specific areas of focus.

### #File

Include a specific file in the conversation context. This is useful when you want Kiro to reference or modify a particular file.

**Usage**: Type `#File` in chat and select the file you want to include.

**Example scenarios**:
- "Review #File for potential improvements"
- "Update #File to implement the new validation logic"
- "Explain how #File works"

### #Folder

Include an entire folder in the conversation context. This gives Kiro visibility into a directory structure and its contents.

**Usage**: Type `#Folder` in chat and select the folder you want to include.

**Example scenarios**:
- "Analyze the structure of #Folder"
- "Add a new component to #Folder following the existing patterns"
- "Review all tests in #Folder"

### #Problems

Include current IDE problems (errors, warnings, linting issues) in the conversation context. This helps Kiro understand what needs to be fixed.

**Usage**: Type `#Problems` in chat to include current IDE diagnostics.

**Example scenarios**:
- "Fix the issues shown in #Problems"
- "Explain why #Problems are occurring"
- "Address the TypeScript errors in #Problems"

### #Terminal

Include terminal output in the conversation context. This is useful for debugging command execution, build failures, or test results.

**Usage**: Type `#Terminal` in chat to include recent terminal output.

**Example scenarios**:
- "The build failed, see #Terminal for details"
- "Debug the test failures shown in #Terminal"
- "Explain the error message in #Terminal"

### #Git

Include Git diff or status information in the conversation context. This helps Kiro understand what changes have been made.

**Usage**: Type `#Git` in chat to include Git information.

**Example scenarios**:
- "Review the changes in #Git"
- "Write a commit message for #Git changes"
- "Explain the impact of #Git diff"

### #Codebase

Scan and search across your entire indexed codebase. This provides Kiro with broad context about your project structure and patterns.

**Usage**: Type `#Codebase` in chat to enable codebase-wide context.

**Example scenarios**:
- "Find all implementations of the authentication pattern in #Codebase"
- "How is error handling done across #Codebase?"
- "Search #Codebase for similar components"

**Note**: Your codebase must be indexed for this feature to work effectively.

## Agent Hooks

Agent hooks allow you to trigger automated agent executions based on events or manual actions. This enables workflow automation and consistency enforcement.

### Event-Triggered Hooks

These hooks automatically execute when specific events occur in the IDE.

**Common event types**:
- File save
- File open
- Git commit
- Build completion
- Test run

**Example use cases**:

**Auto-update tests on save**:
When you save a code file, automatically update and run the corresponding test file.

```yaml
trigger: onFileSave
pattern: "src/**/*.ts"
action: "Update tests for the modified file and run them"
```

**Translation updates**:
When you update translation strings in one language, ensure other languages are updated as well.

```yaml
trigger: onFileSave
pattern: "i18n/en/**/*.json"
action: "Review and update corresponding translations in other languages"
```

### Manual Hooks

These hooks are triggered by user action, typically through a button or command.

**Example use cases**:

**Spell-check README**:
When you click the spell-check hook, review and fix grammar errors in documentation.

```yaml
trigger: manual
name: "Spell Check Documentation"
action: "Review and fix spelling and grammar errors in README.md"
```

**Code review assistant**:
Manually trigger a code review of current changes.

```yaml
trigger: manual
name: "Review Current Changes"
action: "Perform code review on current Git diff, checking for best practices and potential issues"
```

### Managing Hooks

You can view and manage hooks through:

1. **Explorer View**: Look for the "Agent Hooks" section in the Kiro explorer panel
2. **Command Palette**: Search for "Open Kiro Hook UI" to create or edit hooks

Hooks provide a powerful way to automate repetitive tasks and maintain consistency across your codebase.

## Model Context Protocol (MCP)

MCP (Model Context Protocol) is a standardized way to extend Kiro's capabilities by connecting to external tools and services. MCP servers provide additional tools that Kiro can use during execution.

### Configuration Files

MCP can be configured at two levels:

**Workspace Level**: `.kiro/settings/mcp.json`
- Specific to the current project
- Committed to version control (optional)
- Takes precedence over user-level config for server names

**User Level**: `~/.kiro/settings/mcp.json`
- Global configuration across all projects
- Stored in user home directory
- Provides default MCP servers

If both configs exist, they are merged with workspace-level settings taking precedence for conflicting server names.

### Configuration Format

```json
{
  "mcpServers": {
    "server-name": {
      "command": "uvx",
      "args": ["package-name@latest"],
      "env": {
        "ENV_VAR": "value"
      },
      "disabled": false,
      "autoApprove": ["tool-name-1", "tool-name-2"],
      "disabledTools": ["tool-name-3"]
    }
  }
}
```

**Configuration options**:

- `command`: The command to run the MCP server (typically "uvx")
- `args`: Arguments passed to the command (package name and version)
- `env`: Environment variables for the server
- `disabled`: Set to `true` to disable the entire server
- `autoApprove`: List of tool names that don't require user approval
- `disabledTools`: List of specific tools to disable from this server

### Setting Up MCP Servers

Most MCP servers use `uvx` to run, which requires `uv` (a Python package manager) to be installed.

**Installing uv**:
- Use your Python package manager (pip, homebrew, etc.)
- Or follow the installation guide: https://docs.astral.sh/uv/getting-started/installation/

**Note**: Once `uv` is installed, `uvx` will automatically download and run MCP servers. There is no separate "uvx install" command needed.

### Example MCP Configuration

```json
{
  "mcpServers": {
    "aws-docs": {
      "command": "uvx",
      "args": ["awslabs.aws-documentation-mcp-server@latest"],
      "env": {
        "FASTMCP_LOG_LEVEL": "ERROR"
      },
      "disabled": false,
      "autoApprove": [],
      "disabledTools": []
    },
    "filesystem": {
      "command": "uvx",
      "args": ["mcp-server-filesystem@latest"],
      "env": {},
      "disabled": false,
      "autoApprove": ["read_file", "list_directory"],
      "disabledTools": ["delete_file"]
    }
  }
}
```

### Managing MCP Servers

**Reconnecting servers**:
- Servers automatically reconnect when configuration changes
- Manual reconnection available in the MCP Server view (Kiro feature panel)
- No need to restart Kiro

**Finding MCP commands**:
- Open Command Palette and search for "MCP"
- Available commands for configuration, testing, and management

**Testing MCP tools**:
When testing an MCP tool, start using it immediately. Don't check configuration first unless you encounter issues.

### Tool Management

**Auto-approval**:
List tool names in the `autoApprove` array to skip user confirmation for those tools.

```json
"autoApprove": ["search_docs", "get_api_reference"]
```

**Disabling tools**:
List tool names in the `disabledTools` array to prevent Kiro from using them.

```json
"disabledTools": ["delete_resource", "modify_production"]
```

**Disabling entire server**:
Set `disabled: true` to turn off the server without removing its configuration.

```json
"disabled": true
```

## Autonomy Modes

Kiro provides two autonomy modes that control how file modifications are applied and reviewed.

### Autopilot Mode

In Autopilot mode, Kiro can modify files within the workspace autonomously. Changes are applied immediately as the agent works through tasks.

**Characteristics**:
- Changes applied automatically
- Faster workflow for trusted operations
- Best for well-defined tasks with clear requirements
- User maintains oversight through conversation

**When to use**:
- Executing well-specified tasks from approved specs
- Making routine updates or refactoring
- Working on non-critical code sections
- When you trust the agent's understanding of requirements

### Supervised Mode

In Supervised mode, users have the opportunity to review and revert changes after they are applied. This provides an additional safety layer.

**Characteristics**:
- Changes can be reviewed before finalizing
- Ability to revert unwanted modifications
- Slower but safer workflow
- Better for critical or complex changes

**When to use**:
- Working on critical production code
- Making architectural changes
- Learning how Kiro approaches problems
- When requirements are ambiguous or complex

### Choosing the Right Mode

Consider these factors:

**Use Autopilot when**:
- Requirements are clear and approved
- Working on isolated features
- Time efficiency is important
- You have good test coverage

**Use Supervised when**:
- Making changes to critical systems
- Requirements are still being refined
- Learning Kiro's patterns
- Working on unfamiliar code

Both modes maintain the core spec-driven development principles: explicit approval at phase boundaries, one-task-at-a-time execution, and stop-and-review after each task.

## Integration Best Practices

### Combining Context Types

You can use multiple context types together for comprehensive understanding:

```
"Fix the errors in #Problems for #File, using patterns from #Codebase"
```

### Hooks and Specs

Agent hooks can complement spec-driven development:
- Use hooks for routine maintenance tasks
- Use specs for feature development
- Hooks can enforce standards defined in steering files

### MCP and Steering

MCP tools can be referenced in steering files to guide their usage:

```markdown
# API Development Standards

When working with AWS services, use the aws-docs MCP server to verify API usage.

Always check current documentation before implementing AWS SDK calls.
```

### Context and Task Execution

When executing spec tasks, combine context types for better results:

```
"Execute task 3.2, referencing #File for the current implementation and #Problems for any issues"
```

## Troubleshooting Integration Features

### Chat Context Not Working

- Ensure files are saved before including them
- Check that #Codebase index is up to date
- Verify file paths are correct

### Hooks Not Triggering

- Check hook configuration in Agent Hooks view
- Verify event patterns match your files
- Ensure hooks are enabled

### MCP Server Issues

- Verify `uv` is installed: `uv --version`
- Check server configuration in mcp.json
- Look for errors in MCP Server view
- Try reconnecting the server
- Check environment variables are set correctly

### Autonomy Mode Confusion

- Check current mode in Kiro settings
- Remember: mode doesn't affect approval requirements for spec phases
- Both modes still require explicit approval at Requirements, Design, and Tasks phases
