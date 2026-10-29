# Tasks Phase: Converting Design into Actionable Implementation Plans

## Phase Overview

The Tasks phase is the final planning stage in Kiro's spec-driven development methodology. After requirements are clarified and a comprehensive design is created, this phase transforms the technical design into a structured, actionable implementation plan that a coding agent can execute step-by-step.

The Tasks phase produces a checklist of discrete coding tasks that:
- Build incrementally on each other
- Reference specific requirements
- Focus exclusively on code implementation
- Can be executed by an AI agent without additional clarification
- Integrate all code into a cohesive system (no orphaned code)

## Objectives

1. **Convert Design to Discrete Steps**: Break down the design document into manageable, sequential coding tasks
2. **Ensure Actionability**: Each task must be executable by a coding agent without ambiguity
3. **Maintain Traceability**: Link each task back to specific requirements
4. **Enable Incremental Progress**: Structure tasks so each builds on previous work
5. **Facilitate Testing**: Prioritize test-driven development where appropriate
6. **Establish Completion Criteria**: Provide clear definition of done for each task

## File Structure

### Location and Naming

Tasks are documented in a markdown file at:

```
.kiro/specs/{feature_name}/tasks.md
```

Where `{feature_name}` is the kebab-case feature identifier established during requirements phase (e.g., `user-authentication`, `api-client`, `data-validation`).

### File Creation

- The file is created automatically if it doesn't exist
- The agent generates the initial task list based on the approved design document
- The file can be updated iteratively based on user feedback

## Task List Format

### Structure Requirements

Tasks must follow a specific format to ensure consistency and executability:

**Hierarchy Rules:**
- Maximum two levels of hierarchy
- Top-level items for major implementation areas (like epics)
- Sub-tasks numbered with decimal notation (1.1, 1.2, 2.1, 2.2)
- Simple structure is preferred over complex nesting

**Checkbox Format:**
- All items must be checkboxes using `- [ ]` syntax
- Completed tasks marked with `- [x]`
- Status managed by taskStatus tool during execution

**Task Content:**
- Clear, actionable objective as the task description
- Sub-bullets under each task with additional details
- Requirement references in italics (e.g., `_Requirements: 1.1, 2.3_`)

### Example Format

```markdown
# Implementation Plan

- [ ] 1. Set up project structure and core interfaces
  - Sub tasks
  - _Requirements: 1.1, 1.2_

- [ ] 2. Implement data models and validation
- [ ] 2.1 Create core data model interfaces
  - Sub tasks
  - _Requirements: 2.1, 3.3_

- [ ] 2.2 Implement User model with validation
  - Sub tasks
  - _Requirements: 1.2, 2.1_

- [ ] 3. Create storage mechanism
- [ ] 3.1 Implement database connection utilities
  - Sub tasks
  - _Requirements: 2.1, 3.3_
```

## Coding-Only Constraint

### What Tasks MUST Include

Tasks in the implementation plan must focus exclusively on activities that involve writing, modifying, or testing code:

- Implementing functions, classes, or modules
- Creating or modifying configuration files
- Writing unit tests, integration tests, or end-to-end tests
- Setting up test frameworks or testing infrastructure
- Creating data models or database schemas
- Implementing API endpoints or service methods
- Writing validation logic or error handling
- Creating utility functions or helper methods
- Refactoring existing code
- Adding code documentation or inline comments

### What Tasks MUST NOT Include

The following non-coding activities must be explicitly excluded from the task list:

- **User Testing**: User acceptance testing, beta testing, user feedback gathering
- **Deployment**: Deploying to production, staging, or any environment
- **Performance Metrics**: Gathering performance data, analyzing metrics, monitoring
- **Running Applications**: Manually running the application to test end-to-end flows (automated tests are acceptable)
- **User Training**: Creating training materials, conducting training sessions
- **Documentation Creation**: Writing user guides, API documentation (code comments are acceptable)
- **Business Processes**: Organizational changes, workflow modifications
- **Marketing**: Communication activities, announcements, promotional materials
- **Manual Testing**: Any testing that requires human interaction beyond code review

### Rationale

The coding-only constraint ensures that:
- All tasks can be executed within the development environment
- The agent can complete tasks autonomously without external dependencies
- Progress can be measured objectively through code completion
- The implementation plan remains focused on technical delivery

## Actionability Criteria

### What Makes a Task Actionable

Each task must meet these criteria to be considered actionable by a coding agent:

1. **Specificity**: Task specifies what files or components need to be created or modified
2. **Clarity**: Task description is concrete enough to execute without additional clarification
3. **Scope**: Task is scoped to specific coding activities, not high-level concepts
4. **Implementation Focus**: Task describes implementation details rather than abstract features
5. **Self-Contained**: Task can be completed with information from requirements, design, and previous tasks

### Examples

**Good (Actionable):**
- "Implement authentication middleware function that validates JWT tokens"
- "Create User model class with email validation and password hashing methods"
- "Write unit tests for the UserRepository CRUD operations"

**Bad (Not Actionable):**
- "Support user authentication" (too high-level)
- "Make the system secure" (too vague)
- "Improve performance" (not specific)

## Incremental Building Principle

### No Orphaned Code

A critical principle of the Tasks phase is that each task must integrate with previous work. There should be no "hanging" or orphaned code that isn't connected to the rest of the system.

**Requirements:**
- Each task builds on the foundation established by previous tasks
- Code written in one task is used or integrated in subsequent tasks
- The final task should "wire everything together" if needed
- No dead code or unused implementations

### Example Sequence

```markdown
### Phase 1: Project Setup (Keeping Existing Codebase)

- [x] 1. Project setup and foundation
  - Initialize Vite + React + TypeScript project with required dependencies
  - Install shadcn/ui, React Hook Form, Zustand, Axios, Recharts
  - Configure Tailwind CSS with Apple Blue theme (inspired by Apple's design language)
  - Set up project structure (components, pages, services, store, types, utils)
  - Create .env files with AWS credentials from aws-secret.md
  - Create .env.example with placeholder values
  - Add aws-secret.md to .gitignore
  - Configure for S3/CloudFront deployment
  - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5_

- [x] 1.1 Configure build and development tools


  - Set up Vite configuration with code splitting and optimization for S3
  - Configure TypeScript in non-strict mode with path aliases
  - Add ESLint and Prettier for code quality
  - Configure build output for static hosting
  - _Requirements: 11.5_

- [x] 1.2 Create theme and global styles
  - Implement Apple Blue color palette in theme.ts (inspired by Apple's design language)
  - Create global CSS with typography and spacing variables
  - Set up Tailwind configuration with custom theme
  - _Requirements: 10.2, 10.3_

- [x] 2. Common UI components with Apple Blue styling
  - Set up shadcn/ui components (Button, Input, Select, Dialog, Card, Textarea)
  - Customize components with Apple Blue theme
  - Create LoadingSpinner component with theme colors
  - Create Toast notification component
  - Create ConfirmDialog component
  - Create ErrorBoundary component
  - Fully style all components as they are built
  - Test each component in isolation
  - _Requirements: 10.3, 10.5, 9.5, 10.2_

- [x] 3. Routing and layout structure
  - Set up React Router with route configuration
  - Create MainLayout component with Header and Sidebar
  - Create ProtectedRoute component for role-based access
  - Implement navigation menu with role-based visibility
  - _Requirements: 1.2, 10.1_

- [x] 4. API service layer with AWS integration


  - Create Axios instance with base configuration pointing to API Gateway
  - Implement request interceptor for JWT token attachment
  - Implement response interceptor for token refresh and error handling
  - Create error handler utility for consistent error messages
  - Update .env with API Gateway URL from aws-secret.md
  - _Requirements: 11.6_
```

Each task explicitly uses code from previous tasks, ensuring integration.

## Requirement References

### Granular References

Tasks must reference specific, granular sub-requirements from the requirements document, not just high-level user stories.

**Good:**
- `_Requirements: 1.1, 1.2, 2.3_` (references specific acceptance criteria)

**Bad:**
- `_Requirements: Requirement 1_` (too broad, references entire user story)

### Traceability

Requirement references provide:
- **Verification**: Ability to verify implementation against specific criteria
- **Coverage**: Ensures all requirements are addressed by tasks
- **Context**: Helps agent understand why task is needed
- **Prioritization**: Identifies critical tasks tied to core requirements

## Review and Approval Process

### UserInput Tool Usage

After creating or updating the tasks document, the agent must ask for user approval using the userInput tool:

```typescript
userInput({
  question: "Do the tasks look good?",
  reason: "spec-tasks-review"
})
```

The reason code `spec-tasks-review` is mandatory and signals that this is a tasks phase review checkpoint.

### Feedback-Revision Cycle

1. **Agent creates/updates tasks.md**
2. **Agent asks for approval** using userInput tool
3. **User reviews** the task list
4. **If changes needed**: User provides feedback
5. **Agent modifies** tasks based on feedback
6. **Agent asks for approval again** (repeat until approved)
7. **If approved**: User provides clear approval signal ("yes", "approved", "looks good")
8. **Workflow complete**: Agent stops and informs user that spec creation is done

### Explicit Approval Required

The agent must NOT proceed to implementation or consider the workflow complete until receiving explicit approval. Acceptable approval signals include:
- "yes"
- "approved"
- "looks good"
- "that works"
- "proceed"
- "perfect"

Ambiguous responses require clarification.

## Backward Navigation

### Returning to Previous Phases

If gaps or issues are identified during task creation, the agent can return to previous phases:

**Return to Design:**
- If the design lacks sufficient detail for task creation
- If technical approach needs clarification
- If design doesn't address all requirements

**Return to Requirements:**
- If requirements are ambiguous or incomplete
- If new requirements are discovered
- If scope needs adjustment

### User-Initiated Changes

Users can request changes to previous phases at any time:
- "We need to update the design first"
- "Let's add a requirement for X"
- "The design approach won't work, let's revise"

The agent must accommodate these requests and return to the appropriate phase.

## Separation from Implementation

### Spec Creation vs. Task Execution

The Tasks phase is part of spec creation, NOT implementation. This is a critical distinction:

**During Tasks Phase (Spec Creation):**
- Generate task list
- Iterate on task structure and content
- Get user approval
- Stop when approved

**During Task Execution (Separate Workflow):**
- Read all spec documents (requirements, design, tasks)
- Execute one task at a time
- Update task status
- Stop after each task for review

### Communicating Completion

When the tasks document is approved, the agent must:
1. Clearly state that spec creation is complete
2. Inform user that implementation is a separate workflow
3. Explain how to begin execution: "You can start executing tasks by opening tasks.md and clicking 'Start task' next to task items"

## Best Practices

### Task Granularity

- **Too Large**: "Implement entire authentication system" (break into smaller tasks)
- **Too Small**: "Add semicolon to line 42" (combine with related work)
- **Just Right**: "Implement JWT token validation middleware with error handling"

### Task Sequencing

1. **Foundation First**: Start with core interfaces, data models, utilities
2. **Build Upward**: Progress from data layer → business logic → API layer
3. **Test Early**: Include test tasks early to validate core functionality
4. **Integrate Continuously**: Each task should integrate with previous work

### Test-Driven Development

Where appropriate, structure tasks to follow TDD:
```markdown
- [ ] 1.1 Write unit tests for User validation
- [ ] 1.2 Implement User validation to pass tests
```

### Clear Objectives

Each task should answer:
- **What** needs to be implemented?
- **Where** should it be implemented (which files/components)?
- **Why** is it needed (requirement references)?

## Common Pitfalls

### Pitfall 1: Including Non-Coding Tasks

**Problem**: "Deploy to staging environment"
**Solution**: Remove deployment tasks; focus only on code implementation

### Pitfall 2: Vague Task Descriptions

**Problem**: "Make the system better"
**Solution**: "Refactor UserService to use dependency injection pattern"

### Pitfall 3: Missing Requirement References

**Problem**: Task has no requirement references
**Solution**: Add `_Requirements: X.X, Y.Y_` to every task

### Pitfall 4: Orphaned Code

**Problem**: Task creates code that's never used
**Solution**: Ensure subsequent tasks integrate the code

### Pitfall 5: Too Much Detail

**Problem**: Task includes implementation code in description
**Solution**: Keep task description high-level; details go in design document

## Complete Example

Here's a complete tasks.md example for a user authentication feature:

```markdown
# Implementation Plan

- [ ] 1. Set up project structure and dependencies
  - Create directory structure: src/models, src/services, src/middleware, src/routes
  - Install dependencies: bcrypt, jsonwebtoken, express-validator
  - Configure TypeScript with strict mode
  - _Requirements: 1.1_

- [ ] 2. Implement User data model
- [ ] 2.1 Create User interface and type definitions
  - Define User interface with id, email, passwordHash, createdAt fields
  - Create UserDTO for API responses (exclude passwordHash)
  - _Requirements: 1.2, 2.1_

- [ ] 2.2 Implement User model class with validation
  - Write User class with email format validation
  - Implement password hashing using bcrypt
  - Create unit tests for validation logic
  - _Requirements: 1.2, 2.2, 3.1_

- [ ] 3. Create authentication service
- [ ] 3.1 Implement password hashing utilities
  - Write hashPassword function using bcrypt
  - Write comparePassword function for verification
  - Create unit tests for hashing functions
  - _Requirements: 2.2, 3.1_

- [ ] 3.2 Implement JWT token generation and validation
  - Write generateToken function with user payload
  - Write verifyToken function with error handling
  - Create unit tests for token operations
  - _Requirements: 2.3, 3.2_

- [ ] 3.3 Create AuthService with register and login methods
  - Implement register method with duplicate email check
  - Implement login method with password verification
  - Write unit tests for AuthService methods
  - _Requirements: 1.3, 1.4, 2.1, 2.2_

- [ ] 4. Implement authentication middleware
  - Create authenticateToken middleware function
  - Add error handling for invalid/expired tokens
  - Write unit tests for middleware
  - _Requirements: 2.3, 3.2_

- [ ] 5. Create API routes
- [ ] 5.1 Implement registration endpoint
  - Create POST /api/auth/register route
  - Add request validation using express-validator
  - Wire up AuthService.register method
  - Write integration tests for registration
  - _Requirements: 1.3, 2.1_

- [ ] 5.2 Implement login endpoint
  - Create POST /api/auth/login route
  - Add request validation
  - Wire up AuthService.login method
  - Write integration tests for login
  - _Requirements: 1.4, 2.2_

- [ ] 5.3 Implement protected route example
  - Create GET /api/user/profile route
  - Apply authenticateToken middleware
  - Return user data from token
  - Write integration tests for protected route
  - _Requirements: 2.3, 3.2_

- [ ] 6. Add error handling and edge cases
  - Implement global error handler middleware
  - Add validation for edge cases (empty fields, invalid formats)
  - Create tests for error scenarios
  - _Requirements: 3.1, 3.2, 3.3_

- [ ] 7. Create end-to-end test suite
  - Write E2E tests for complete registration → login → access protected route flow
  - Test error scenarios (invalid credentials, expired tokens)
  - Verify all requirements are met
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 2.1, 2.2, 2.3, 3.1, 3.2, 3.3_
```

This example demonstrates:
- Clear task hierarchy with decimal notation
- Specific, actionable objectives
- Incremental building (each task uses previous work)
- Requirement references for every task
- Test-driven approach
- Coding-only focus (no deployment or user testing)
- Integration in final tasks

## Summary

The Tasks phase converts approved designs into executable implementation plans by:

1. Breaking design into discrete, manageable coding tasks
2. Structuring tasks in a two-level checkbox hierarchy
3. Ensuring each task is actionable by a coding agent
4. Referencing specific requirements for traceability
5. Building incrementally with no orphaned code
6. Focusing exclusively on coding activities
7. Obtaining explicit user approval before completion

Once the tasks document is approved, spec creation is complete. Implementation begins in a separate workflow where tasks are executed one at a time with status tracking and user review after each task.
