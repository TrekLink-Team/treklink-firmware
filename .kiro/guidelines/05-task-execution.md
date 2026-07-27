# Task Execution Guide

## Overview

Task execution is the implementation phase where the agent transforms the approved task list into working code. This guide covers the complete workflow for executing tasks, including pre-execution requirements, task selection, status management, verification, and the critical stop-and-review pattern that ensures quality and user control.

The task execution workflow operates separately from spec creation. Once you have an approved tasks.md file, you can begin executing tasks one at a time, with mandatory user review after each completion.

## Pre-Execution Requirements

Before executing any task, the agent MUST read all three spec documents:

1. **requirements.md** - Provides the acceptance criteria and user stories that define success
2. **design.md** - Provides the technical architecture and design decisions
3. **tasks.md** - Provides the task list with implementation details

Executing tasks without this context leads to inaccurate implementations that don't align with requirements or design decisions. The agent must have complete understanding of what needs to be built, how it should be architected, and what specific task is being addressed.

## One-Task-at-a-Time Principle

The most critical rule in task execution is: **execute ONLY ONE task at a time**.

After completing a task:
- Stop immediately
- Let the user review the implementation
- Wait for explicit instruction to proceed

DO NOT automatically continue to the next task. This principle ensures:
- User maintains control over implementation progress
- Each task can be reviewed for quality
- Issues are caught early before compounding
- User can adjust direction based on results

## Task Selection and Sub-Task Handling

### Task Hierarchy

Tasks follow a maximum two-level hierarchy:

```markdown
- [ ] 1. Parent task description
  - Details about the parent task
  - _Requirements: 1.1, 1.2_

- [ ] 1.1 First sub-task
  - Details about sub-task
  - _Requirements: 1.1_

- [ ] 1.2 Second sub-task
  - Details about sub-task
  - _Requirements: 1.2_

- [ ] 2. Another parent task
  - Details
  - _Requirements: 2.1_
```

### Sub-Task Execution Order

When a task has sub-tasks, ALWAYS start with the sub-tasks:

1. Execute sub-task 1.1
2. Stop for user review
3. Execute sub-task 1.2 (if approved)
4. Stop for user review
5. Execute parent task 1 (if approved)
6. Stop for user review

Only mark the parent task as complete after all sub-tasks are complete.

### Task Selection Logic

If the user doesn't specify which task to execute:

1. Look at the task list
2. Find the first incomplete task (not_started or in_progress)
3. If it has sub-tasks, recommend the first incomplete sub-task
4. If no sub-tasks, recommend the task itself
5. Present the recommendation to the user
6. Wait for confirmation before proceeding

## Task Status Management

### Status States

Tasks have three possible states:

- **not_started** - Task has not been worked on yet
- **in_progress** - Task is currently being implemented
- **completed** - Task is fully finished and verified

### TaskStatus Tool Usage

The taskStatus tool updates task states in the tasks.md file.

**Tool Parameters:**
- `taskFilePath` - Path to tasks.md (e.g., ".kiro/specs/feature-name/tasks.md")
- `task` - Exact task text from tasks.md (e.g., "1.1 First sub-task")
- `status` - New status: "not_started", "in_progress", or "completed"

**Important:** The task text must match EXACTLY as it appears in tasks.md, including the number prefix.

### Status Update Workflow

1. **Before starting work:**
   ```
   Set task to "in_progress"
   ```

2. **During implementation:**
   - Task remains "in_progress"
   - Make code changes
   - Write tests
   - Verify against requirements

3. **After completing work:**
   ```
   Set task to "completed"
   ```

### Example Status Updates

```markdown
# Starting task 1.1
taskStatus(
  taskFilePath: ".kiro/specs/user-auth/tasks.md",
  task: "1.1 Create user model with validation",
  status: "in_progress"
)

# After completing task 1.1
taskStatus(
  taskFilePath: ".kiro/specs/user-auth/tasks.md",
  task: "1.1 Create user model with validation",
  status: "completed"
)
```

## Task Execution Workflow

### Step-by-Step Process

1. **Read Context**
   - Read requirements.md
   - Read design.md
   - Read tasks.md
   - Understand the complete feature context

2. **Identify Task**
   - User specifies task, or
   - Agent recommends next incomplete task
   - Confirm task selection with user

3. **Set Status to In Progress**
   - Use taskStatus tool
   - Mark task as "in_progress"

4. **Implement Task**
   - Focus ONLY on the current task
   - Follow design specifications
   - Write minimal, necessary code
   - Create tests if specified
   - Ensure code is immediately runnable

5. **Verify Against Requirements**
   - Check task details for requirement references
   - Verify implementation meets acceptance criteria
   - Test functionality
   - Ensure integration with previous work

6. **Set Status to Completed**
   - Use taskStatus tool
   - Mark task as "completed"

7. **Stop and Report**
   - Summarize what was implemented
   - Highlight any important decisions
   - Stop and wait for user review
   - DO NOT proceed to next task

### Verification Process

Each task includes requirement references in its details:

```markdown
- [ ] 1.1 Implement user authentication
  - Create login endpoint with JWT token generation
  - Add password hashing with bcrypt
  - _Requirements: 1.1, 1.2, 3.3_
```

Verification steps:

1. **Locate Requirements**
   - Find requirements 1.1, 1.2, and 3.3 in requirements.md
   - Read the acceptance criteria

2. **Check Acceptance Criteria**
   - Verify each EARS statement is satisfied
   - WHEN conditions trigger correct THEN responses
   - IF conditions produce correct outcomes

3. **Test Implementation**
   - Run code to verify it works
   - Test edge cases mentioned in requirements
   - Ensure error handling works as designed

4. **Confirm Integration**
   - Verify code integrates with previous tasks
   - No orphaned or unused code
   - Follows design architecture

## Stop-and-Review Pattern

After completing each task, the agent MUST:

1. **Stop immediately** - Do not continue to next task
2. **Summarize work** - Briefly describe what was implemented
3. **Highlight decisions** - Note any important choices made
4. **Wait for feedback** - User may request changes or approve
5. **Respond to feedback** - Make adjustments if requested

This pattern ensures:
- User maintains oversight
- Quality is verified incrementally
- Direction can be adjusted
- Issues are caught early

### Example Stop-and-Review

```
Task 1.1 completed: Created user model with validation

Implementation:
- Added User class in src/models/user.ts
- Implemented email and password validation
- Added unit tests for validation logic
- All tests passing

The user model validates email format and password strength
according to requirements 1.1 and 1.2.

Ready for your review. Let me know if you'd like any changes,
or if you're ready to move to the next task.
```

## Task Recommendations

When the user asks "what's next?" or doesn't specify a task:

1. **Analyze Task List**
   - Find first incomplete task
   - Check for sub-tasks

2. **Make Recommendation**
   ```
   The next task is 1.2: "Implement password hashing"
   
   This task will add bcrypt hashing to the user model
   and satisfies requirement 1.2 for secure password storage.
   
   Would you like me to proceed with this task?
   ```

3. **Wait for Confirmation**
   - User may accept recommendation
   - User may request different task
   - User may ask questions first

## Distinguishing Questions from Execution Requests

Not every mention of tasks means the user wants execution:

### Informational Questions

User asks: "What's the next task?"
- Provide task information
- Don't start execution
- Wait for explicit request

User asks: "How many tasks are left?"
- Count incomplete tasks
- Provide summary
- Don't start execution

### Execution Requests

User says: "Start task 1.1"
- Begin execution workflow
- Set status to in_progress
- Implement the task

User says: "Let's do the next task"
- Recommend next task
- Wait for confirmation
- Then begin execution

### Ambiguous Requests

User says: "Tell me about task 2"
- Provide task details
- Ask if they want to execute it
- Wait for clarification

## Complete Execution Example

### Initial State

```markdown
# tasks.md
- [ ] 1. Set up user authentication
  - _Requirements: 1.1-1.5_

- [ ] 1.1 Create user model
  - Implement User class with email and password fields
  - Add validation for email format and password strength
  - _Requirements: 1.1, 1.2_

- [ ] 1.2 Implement password hashing
  - Add bcrypt for password hashing
  - Hash passwords before storage
  - _Requirements: 1.2_
```

### Execution Session

**User:** "Start task 1.1"

**Agent:**
1. Reads requirements.md, design.md, tasks.md
2. Sets task 1.1 to "in_progress"
3. Creates src/models/user.ts with User class
4. Implements email validation (regex for format)
5. Implements password validation (min length, complexity)
6. Creates tests in tests/models/user.test.ts
7. Runs tests - all passing
8. Sets task 1.1 to "completed"
9. Reports completion and stops

**Agent Response:**
```
Task 1.1 completed: Created user model with validation

Files created:
- src/models/user.ts - User class with validation
- tests/models/user.test.ts - Unit tests

Implementation details:
- Email validation uses RFC 5322 regex pattern
- Password requires 8+ chars, uppercase, lowercase, number
- Validation throws descriptive errors
- All tests passing (5/5)

This satisfies requirements 1.1 (user data model) and 1.2
(validation rules).

Ready for review. Would you like any changes?
```

**User:** "Looks good, continue"

**Agent:**
1. Reads task list
2. Identifies task 1.2 as next
3. Recommends task 1.2

**Agent Response:**
```
Next task is 1.2: "Implement password hashing"

This will add bcrypt hashing to secure passwords before
storage, satisfying requirement 1.2.

Should I proceed?
```

**User:** "Yes"

**Agent:**
1. Sets task 1.2 to "in_progress"
2. Installs bcrypt dependency
3. Updates User class with hash method
4. Modifies save logic to hash passwords
5. Updates tests to verify hashing
6. Runs tests - all passing
7. Sets task 1.2 to "completed"
8. Reports completion and stops

**Agent Response:**
```
Task 1.2 completed: Implemented password hashing

Changes:
- Added bcrypt dependency
- User.hashPassword() method added
- Passwords hashed before storage (10 salt rounds)
- Tests verify hashing and comparison
- All tests passing (8/8)

Passwords are now securely hashed using bcrypt with
10 salt rounds, meeting requirement 1.2.

Ready for review.
```

**User:** "Perfect, let's do task 1 now"

**Agent:**
1. Checks task 1 sub-tasks (1.1 and 1.2)
2. Verifies both sub-tasks are completed
3. Sets task 1 to "in_progress"
4. Wires together user model components
5. Creates integration tests
6. Verifies all requirements 1.1-1.5
7. Sets task 1 to "completed"
8. Reports completion and stops

## Best Practices

### Do's

- Always read all spec documents before starting
- Set status to in_progress before working
- Focus on one task only
- Verify against requirement references
- Write minimal, necessary code
- Test your implementation
- Set status to completed when done
- Stop and wait for user review
- Provide clear completion summaries

### Don'ts

- Don't skip reading spec documents
- Don't work on multiple tasks simultaneously
- Don't automatically continue to next task
- Don't implement features not in current task
- Don't leave orphaned or unused code
- Don't forget to update task status
- Don't proceed without user approval
- Don't assume user wants to continue

## Troubleshooting

### Task Unclear

If task description is ambiguous:
- Review design document for clarification
- Check requirement references for details
- Ask user for clarification if still unclear
- Don't guess or assume

### Requirements Not Met

If implementation doesn't satisfy requirements:
- Review acceptance criteria again
- Identify what's missing
- Implement missing functionality
- Re-verify before marking complete

### Integration Issues

If task doesn't integrate with previous work:
- Review previous task implementations
- Check design for integration points
- Refactor to ensure proper integration
- Verify no orphaned code remains

### Status Tool Errors

If taskStatus tool fails:
- Verify task text matches exactly
- Check file path is correct
- Ensure tasks.md exists
- Try with exact text from file

## Summary

Task execution follows a disciplined workflow:

1. Read all spec documents (requirements, design, tasks)
2. Select one task (handle sub-tasks first)
3. Set status to in_progress
4. Implement the task (minimal, focused code)
5. Verify against requirements
6. Set status to completed
7. Stop and report to user
8. Wait for approval before next task

This workflow ensures quality, maintains user control, and produces implementations that accurately reflect requirements and design decisions.
