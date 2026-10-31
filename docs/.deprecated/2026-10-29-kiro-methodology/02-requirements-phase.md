# Requirements Phase: Transforming Ideas into Structured Requirements

## Phase Overview

The Requirements phase is the first step in Kiro's AI-Driven Development Lifecycle. It transforms rough feature ideas into structured, testable requirements using the EARS (Easy Approach to Requirements Syntax) format combined with user stories. This phase establishes the ground-truth for what will be built, ensuring shared understanding between developer and AI agent before any design or implementation begins.

The Requirements phase produces a single artifact: `requirements.md`, located at `.kiro/specs/{feature_name}/requirements.md`.

## Objectives

The Requirements phase aims to:

- Capture the feature idea in structured, testable format
- Identify all functional and non-functional requirements
- Define clear acceptance criteria for each requirement
- Establish user stories that explain the "why" behind features
- Create a foundation for the Design phase
- Ensure developer and agent have shared understanding before proceeding

## EARS Format Specification

EARS (Easy Approach to Requirements Syntax) provides a structured way to write clear, testable requirements. It uses specific keywords to define system behavior under different conditions.

### EARS Patterns

#### WHEN/THEN Pattern
Used for event-driven requirements where a specific trigger causes a system response.

**Format**: WHEN [event] THEN [system] SHALL [response]

**Examples**:
- WHEN user clicks the submit button THEN the system SHALL validate all form fields
- WHEN authentication fails THEN the system SHALL display an error message
- WHEN file upload completes THEN the system SHALL notify the user of success

#### IF/THEN Pattern
Used for conditional requirements where system behavior depends on a precondition.

**Format**: IF [precondition] THEN [system] SHALL [response]

**Examples**:
- IF user is not authenticated THEN the system SHALL redirect to login page
- IF file size exceeds 10MB THEN the system SHALL reject the upload
- IF database connection fails THEN the system SHALL retry up to 3 times

#### WHILE/THEN Pattern
Used for continuous requirements where system behavior persists during a state.

**Format**: WHILE [state] THEN [system] SHALL [response]

**Examples**:
- WHILE data is loading THEN the system SHALL display a loading indicator
- WHILE user is typing THEN the system SHALL provide autocomplete suggestions
- WHILE file is uploading THEN the system SHALL show progress percentage

#### Combined Patterns
Multiple conditions can be combined for complex requirements.

**Format**: WHEN [event] AND [condition] THEN [system] SHALL [response]

**Examples**:
- WHEN user submits form AND all fields are valid THEN the system SHALL save the data
- WHEN API request fails AND retry count is less than 3 THEN the system SHALL retry the request
- WHEN user clicks delete AND confirmation is provided THEN the system SHALL remove the item

### SHALL vs. SHOULD vs. MAY

- **SHALL**: Mandatory requirement, must be implemented
- **SHOULD**: Recommended requirement, should be implemented unless good reason not to
- **MAY**: Optional requirement, can be implemented if time/resources allow

Use SHALL for all acceptance criteria unless explicitly documenting optional features.

## User Story Format

Each requirement begins with a user story that provides context and explains the value.

### Format

**User Story:** As a [role], I want [feature], so that [benefit]

### Components

- **Role**: Who needs this feature? (user, admin, developer, system, etc.)
- **Feature**: What functionality is needed?
- **Benefit**: Why is this valuable? What problem does it solve?

### Examples

**Good User Stories**:
- As a developer, I want comprehensive error messages, so that I can quickly diagnose and fix issues
- As an admin, I want to view user activity logs, so that I can monitor system usage and identify problems
- As a user, I want to save my preferences, so that I don't have to reconfigure settings each session

**Poor User Stories** (avoid these):
- As a user, I want a button (missing benefit)
- I want the system to be fast (missing role and specific feature)
- As a developer, I want good code (too vague, not testable)

## Requirements Document Structure

### Template

```markdown
# Requirements Document

## Introduction

[2-3 paragraphs explaining the feature, its purpose, and scope. Provide context for why this feature is needed and what problem it solves.]

## Requirements

### Requirement 1: [Descriptive Name]

**User Story:** As a [role], I want [feature], so that [benefit]

#### Acceptance Criteria

1. WHEN [event] THEN [system] SHALL [response]
2. IF [precondition] THEN [system] SHALL [response]
3. WHILE [state] THEN [system] SHALL [response]
[Additional criteria as needed]

### Requirement 2: [Descriptive Name]

**User Story:** As a [role], I want [feature], so that [benefit]

#### Acceptance Criteria

1. WHEN [event] THEN [system] SHALL [response]
2. WHEN [event] AND [condition] THEN [system] SHALL [response]
[Additional criteria as needed]

[Additional requirements as needed]
```

### Structure Guidelines

- **Introduction**: Provide context, explain the feature's purpose, define scope
- **Requirements**: Numbered hierarchically (1, 2, 3, etc.)
- **Requirement Names**: Descriptive, specific (e.g., "User Authentication", "Data Validation", "Error Handling")
- **User Stories**: One per requirement, explains the "why"
- **Acceptance Criteria**: Numbered list under each requirement, uses EARS format
- **Granularity**: Each acceptance criterion should be independently testable

## File Location and Naming

### Directory Structure

Requirements are stored in the spec directory for the feature:

```
.kiro/specs/{feature_name}/requirements.md
```

### Feature Naming Convention

- Use kebab-case: `user-authentication`, `api-client`, `data-export`
- Be descriptive but concise
- Avoid special characters except hyphens
- Use lowercase only

### Example Paths

- `.kiro/specs/user-authentication/requirements.md`
- `.kiro/specs/api-rate-limiting/requirements.md`
- `.kiro/specs/export-to-csv/requirements.md`

## Review and Approval Process

The Requirements phase follows a strict approval workflow to ensure quality and shared understanding.

### Workflow Steps

1. **Agent generates initial requirements**: Based on user's rough idea, agent creates complete requirements.md
2. **Agent requests review**: Uses userInput tool with reason 'spec-requirements-review'
3. **User reviews**: Examines requirements for completeness, accuracy, clarity
4. **User provides feedback**: Either approves or requests changes
5. **Agent modifies**: If changes requested, agent updates requirements.md
6. **Repeat**: Steps 2-5 continue until user explicitly approves

### UserInput Tool Usage

The agent MUST use the userInput tool to request approval:

```typescript
userInput({
  question: "Do the requirements look good? If so, we can move on to the design.",
  reason: "spec-requirements-review"
})
```

### Approval Signals

The agent MUST wait for explicit approval before proceeding to Design phase.

**Clear Approval Signals**:
- "yes"
- "approved"
- "looks good"
- "proceed"
- "move on"
- "continue"
- "that works"

**Not Approval Signals**:
- Silence or no response
- "maybe"
- "I think so"
- Questions about specific requirements
- Requests for changes or additions

### Modification Workflow

If user requests changes:

1. Agent MUST make the requested modifications
2. Agent MUST re-request approval using userInput tool
3. Agent MUST NOT proceed to Design phase without explicit approval
4. Agent MUST continue this cycle until approval is received

## Iteration and Modification Workflow

Requirements often need multiple iterations to get right. The agent should facilitate this process smoothly.

### Common Iteration Scenarios

#### Adding Requirements
User: "Can you add a requirement for error handling?"

Agent response:
1. Add new requirement with user story and acceptance criteria
2. Update requirements.md
3. Request approval again

#### Modifying Acceptance Criteria
User: "The validation requirement needs to check email format too"

Agent response:
1. Update specific acceptance criterion
2. Ensure EARS format is maintained
3. Request approval again

#### Clarifying Vague Requirements
User: "The performance requirement is too vague"

Agent response:
1. Ask clarifying questions about specific metrics
2. Update requirement with concrete, measurable criteria
3. Request approval again

#### Splitting Complex Requirements
User: "Requirement 3 is trying to do too much"

Agent response:
1. Split into multiple focused requirements
2. Ensure each has clear user story and acceptance criteria
3. Request approval again

### Iteration Best Practices

- Make requested changes promptly and accurately
- Maintain EARS format throughout iterations
- Keep requirements focused and testable
- Ask clarifying questions when feedback is unclear
- Summarize changes made before requesting re-approval
- Don't combine multiple unrelated changes without user request

## Best Practices for Writing Requirements

### Be Specific and Measurable

**Good**: WHEN user submits form THEN the system SHALL validate all fields within 100ms

**Bad**: WHEN user submits form THEN the system SHALL validate quickly

### Focus on "What", Not "How"

**Good**: WHEN user logs in THEN the system SHALL authenticate credentials

**Bad**: WHEN user logs in THEN the system SHALL check password hash using bcrypt

### Make Requirements Testable

**Good**: IF file size exceeds 10MB THEN the system SHALL reject the upload

**Bad**: IF file is too large THEN the system SHALL handle it appropriately

### Avoid Implementation Details

**Good**: WHEN data changes THEN the system SHALL persist updates

**Bad**: WHEN data changes THEN the system SHALL write to PostgreSQL database

### Consider Edge Cases

Include requirements for:
- Error conditions
- Boundary values
- Invalid inputs
- Network failures
- Concurrent operations
- Performance constraints

### Use Consistent Terminology

- Pick terms and stick with them (e.g., "user" vs. "customer")
- Define domain-specific terms in introduction
- Use same terms across all requirements

## Common Pitfalls and How to Avoid Them

### Pitfall 1: Vague Requirements

**Problem**: "The system should be user-friendly"

**Solution**: Define specific, measurable criteria
- WHEN user completes task THEN the system SHALL require no more than 3 clicks
- WHEN error occurs THEN the system SHALL display actionable error message

### Pitfall 2: Implementation Disguised as Requirements

**Problem**: "The system shall use React hooks for state management"

**Solution**: Focus on behavior, not implementation
- WHEN user updates form field THEN the system SHALL reflect changes immediately
- WHEN component unmounts THEN the system SHALL clean up resources

### Pitfall 3: Missing Error Handling

**Problem**: Only specifying happy path scenarios

**Solution**: Include error conditions
- WHEN API request fails THEN the system SHALL retry up to 3 times
- IF authentication token expires THEN the system SHALL prompt for re-authentication

### Pitfall 4: Overly Complex Requirements

**Problem**: Single requirement trying to cover too many scenarios

**Solution**: Split into focused, single-purpose requirements
- Separate authentication from authorization
- Separate validation from submission
- Separate display from data fetching

### Pitfall 5: Missing User Stories

**Problem**: Jumping straight to acceptance criteria without context

**Solution**: Always start with user story explaining the "why"
- Helps understand the value
- Guides design decisions
- Provides context for implementation

### Pitfall 6: Untestable Requirements

**Problem**: "The system shall be secure"

**Solution**: Define specific, verifiable criteria
- WHEN user attempts SQL injection THEN the system SHALL sanitize input
- WHEN password is created THEN the system SHALL enforce minimum 12 characters

## Complete Example: User Authentication Feature

```markdown
# Requirements Document

## Introduction

This document outlines the requirements for implementing user authentication in the application. The authentication system will allow users to securely create accounts, log in, and maintain sessions. This feature is essential for protecting user data and providing personalized experiences.

The authentication system will support email/password authentication with secure password storage, session management, and basic account recovery. It will integrate with the existing user database and provide authentication state to other application components.

## Requirements

### Requirement 1: User Registration

**User Story:** As a new user, I want to create an account with email and password, so that I can access protected features of the application.

#### Acceptance Criteria

1. WHEN user submits registration form THEN the system SHALL validate email format
2. WHEN user submits registration form THEN the system SHALL validate password meets minimum requirements (12 characters, 1 uppercase, 1 lowercase, 1 number)
3. IF email already exists THEN the system SHALL display error message "Email already registered"
4. IF validation passes THEN the system SHALL create user account with hashed password
5. WHEN account is created THEN the system SHALL send confirmation email
6. WHEN user confirms email THEN the system SHALL activate the account

### Requirement 2: User Login

**User Story:** As a registered user, I want to log in with my credentials, so that I can access my account and protected features.

#### Acceptance Criteria

1. WHEN user submits login form THEN the system SHALL validate email and password
2. IF credentials are invalid THEN the system SHALL display error message "Invalid email or password"
3. IF account is not activated THEN the system SHALL display error message "Please confirm your email"
4. IF credentials are valid AND account is activated THEN the system SHALL create session
5. WHEN session is created THEN the system SHALL redirect user to dashboard
6. WHEN login fails 5 times THEN the system SHALL temporarily lock account for 15 minutes

### Requirement 3: Session Management

**User Story:** As a logged-in user, I want my session to persist across page refreshes, so that I don't have to log in repeatedly.

#### Acceptance Criteria

1. WHEN user logs in THEN the system SHALL create session token with 24-hour expiration
2. WHEN user refreshes page THEN the system SHALL validate session token
3. IF session token is valid THEN the system SHALL maintain authenticated state
4. IF session token is expired THEN the system SHALL redirect to login page
5. WHEN user logs out THEN the system SHALL invalidate session token
6. WHILE user is inactive for 30 minutes THEN the system SHALL display session timeout warning

### Requirement 4: Password Reset

**User Story:** As a user who forgot my password, I want to reset it securely, so that I can regain access to my account.

#### Acceptance Criteria

1. WHEN user requests password reset THEN the system SHALL send reset link to registered email
2. WHEN user clicks reset link THEN the system SHALL validate token is not expired (1-hour expiration)
3. IF token is valid THEN the system SHALL display password reset form
4. WHEN user submits new password THEN the system SHALL validate password requirements
5. IF new password is valid THEN the system SHALL update password hash
6. WHEN password is updated THEN the system SHALL invalidate all existing sessions
7. WHEN password is updated THEN the system SHALL send confirmation email

### Requirement 5: Security Constraints

**User Story:** As a system administrator, I want authentication to follow security best practices, so that user accounts are protected from common attacks.

#### Acceptance Criteria

1. WHEN password is stored THEN the system SHALL use bcrypt with minimum cost factor of 12
2. WHEN session token is generated THEN the system SHALL use cryptographically secure random values
3. WHEN authentication fails THEN the system SHALL not reveal whether email exists
4. WHEN user attempts brute force THEN the system SHALL implement rate limiting (5 attempts per 15 minutes)
5. WHEN sensitive operations occur THEN the system SHALL log authentication events
6. WHEN password reset token is used THEN the system SHALL invalidate token immediately
```

This example demonstrates:
- Clear introduction with scope and context
- Well-structured requirements with descriptive names
- User stories explaining the value
- EARS-format acceptance criteria
- Coverage of happy path and error conditions
- Security considerations
- Edge cases (rate limiting, token expiration)

## Summary

The Requirements phase is critical for establishing shared understanding before design and implementation. By using EARS format and user stories, requirements become clear, testable, and valuable. The explicit approval workflow ensures developers remain in control while the agent transforms rough ideas into structured requirements.

Key points:
- Use EARS format (WHEN/THEN, IF/THEN, WHILE/THEN) for acceptance criteria
- Start each requirement with user story explaining the "why"
- Store in `.kiro/specs/{feature_name}/requirements.md`
- Request approval using userInput tool with 'spec-requirements-review' reason
- Iterate based on feedback until explicit approval received
- Focus on "what" not "how", avoid implementation details
- Include error handling and edge cases
- Make requirements specific, measurable, and testable
