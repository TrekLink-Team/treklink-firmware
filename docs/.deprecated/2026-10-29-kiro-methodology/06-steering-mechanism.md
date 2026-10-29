# Steering Mechanism

## Overview

Steering files are Kiro's mechanism for providing persistent context, standards, and instructions that influence agent behavior across your project. Think of them as guidelines that Kiro automatically considers when working on your code - whether that's team coding standards, architectural decisions, or references to important specifications like OpenAPI schemas.

Located in `.kiro/steering/*.md`, these files allow you to shape how Kiro approaches tasks without repeating yourself in every conversation.

## File Location

All steering files must be placed in:

```
.kiro/steering/*.md
```

Each file should be a markdown document with a descriptive name that indicates its purpose (e.g., `coding-standards.md`, `api-guidelines.md`, `testing-approach.md`).

## Inclusion Modes

Steering files support three inclusion modes that control when and how they're applied:

### 1. Always Included (Default)

By default, any steering file without front-matter configuration is always included in the agent's context.

**Example: `.kiro/steering/team-standards.md`**

```markdown
# Team Coding Standards

## TypeScript Guidelines

- Always use strict mode
- Prefer interfaces over types for object shapes
- Use async/await over raw promises
- Include JSDoc comments for public APIs

## Testing Requirements

- Minimum 80% code coverage
- Unit tests for all business logic
- Integration tests for API endpoints
```

This file will be automatically included in every agent interaction.

### 2. Conditional Inclusion (File Match)

Conditional steering files are only included when specific files are read into context. This is useful for domain-specific guidelines that only apply to certain parts of your codebase.

**Configuration:** Add front-matter with `inclusion: fileMatch` and `fileMatchPattern`

**Example: `.kiro/steering/api-guidelines.md`**

```markdown
---
inclusion: fileMatch
fileMatchPattern: 'src/api/**/*.ts'
---

# API Development Guidelines

## REST Conventions

- Use plural nouns for resource endpoints
- HTTP status codes: 200 (success), 201 (created), 400 (bad request), 404 (not found)
- Always validate input with Zod schemas

## Error Handling

- Return consistent error format: `{ error: string, code: string, details?: any }`
- Log all 500 errors with full stack traces
```

This file will only be included when Kiro reads files matching `src/api/**/*.ts`.

**Pattern Syntax:**
- Use glob patterns for matching
- `*` matches any characters within a path segment
- `**` matches any number of path segments
- Examples: `README*`, `src/**/*.test.ts`, `*.config.js`

### 3. Manual Inclusion (Context Key)

Manual steering files are only included when explicitly referenced by the user via a context key in chat.

**Configuration:** Add front-matter with `inclusion: manual`

**Example: `.kiro/steering/deployment-guide.md`**

```markdown
---
inclusion: manual
---

# Deployment Guide

## Build Process

1. Run `npm run build`
2. Run tests: `npm test`
3. Build Docker image: `docker build -t myapp:latest .`

## Deployment Steps

1. Push image to registry
2. Update Kubernetes manifests
3. Apply with `kubectl apply -f k8s/`
```

To use this file, reference it in chat: `#deployment-guide` or use the context picker.

## File Reference Syntax

Steering files can reference external files using special syntax. This is particularly powerful for including specifications that should influence implementation.

**Syntax:**

```markdown
#[[file:<relative_file_name>]]
```

The path is relative to the steering file's location.

### Use Cases

#### OpenAPI Specification Integration

**Example: `.kiro/steering/api-spec.md`**

```markdown
# API Specification

Our API follows the OpenAPI specification defined here:

#[[file:../specs/openapi.yaml]]

When implementing API endpoints, ensure:
- All endpoints match the spec exactly
- Request/response schemas are validated
- Error responses follow the spec's error format
```

When Kiro reads this steering file, it will also load and consider the OpenAPI specification, ensuring implementations align with your API contract.

#### GraphQL Schema Integration

**Example: `.kiro/steering/graphql-schema.md`**

```markdown
# GraphQL Schema

Our GraphQL API is defined by:

#[[file:../schema.graphql]]

Implementation requirements:
- All resolvers must match schema types
- Use DataLoader for N+1 query prevention
- Implement proper error handling per schema directives
```

#### Configuration Reference

**Example: `.kiro/steering/build-config.md`**

```markdown
# Build Configuration

Reference our build configuration:

#[[file:../webpack.config.js]]

When modifying build process:
- Maintain compatibility with existing config
- Test in both development and production modes
- Update documentation if adding new build steps
```

## Common Use Cases

### Team Standards and Norms

Define coding conventions, naming patterns, and architectural decisions that should be consistently applied.

```markdown
# Team Standards

## Naming Conventions

- Components: PascalCase (e.g., `UserProfile.tsx`)
- Utilities: camelCase (e.g., `formatDate.ts`)
- Constants: UPPER_SNAKE_CASE (e.g., `MAX_RETRY_COUNT`)

## Architecture Decisions

- Use Redux for global state
- Use React Query for server state
- Keep components under 200 lines
- Extract business logic to custom hooks
```

### Project Information

Provide context about the project structure, dependencies, and key concepts.

```markdown
# Project Overview

## Architecture

This is a microservices application with:
- Frontend: React + TypeScript
- Backend: Node.js + Express
- Database: PostgreSQL
- Cache: Redis

## Key Directories

- `/src/api` - REST API endpoints
- `/src/services` - Business logic
- `/src/models` - Data models and schemas
- `/src/utils` - Shared utilities
```

### Build and Test Instructions

Document how to build, test, and run the project.

```markdown
# Development Workflow

## Running Locally

```bash
npm install
npm run dev
```

## Testing

```bash
npm test              # Run all tests
npm run test:watch    # Watch mode
npm run test:coverage # With coverage
```

## Building

```bash
npm run build         # Production build
npm run build:dev     # Development build
```
```

### Domain-Specific Guidelines

Create conditional steering files for specific parts of your codebase.

```markdown
---
inclusion: fileMatch
fileMatchPattern: 'src/components/**/*.tsx'
---

# React Component Guidelines

## Component Structure

1. Props interface at top
2. Component function
3. Helper functions below
4. Styles at bottom (if using styled-components)

## Hooks Order

1. useState
2. useEffect
3. Custom hooks
4. useCallback/useMemo

## Accessibility

- All interactive elements must have proper ARIA labels
- Support keyboard navigation
- Maintain focus management
```

## Creating and Updating Steering Files

### Creating a New Steering File

1. Create a markdown file in `.kiro/steering/`
2. Add front-matter if you want conditional or manual inclusion
3. Write your guidelines, standards, or references
4. Save the file - it's immediately active

### Updating Existing Steering Files

Simply edit the file and save. Changes take effect immediately in new conversations or when files matching the pattern are read.

### Asking Kiro to Create Steering Files

You can ask Kiro to create or update steering files:

```
"Create a steering file for our TypeScript coding standards"
"Add a conditional steering file for our database models"
"Update the API guidelines steering file to include rate limiting rules"
```

## Best Practices

### Keep Files Focused

Each steering file should cover a specific domain or concern. Don't create one massive file with everything.

**Good:**
- `typescript-standards.md`
- `testing-guidelines.md`
- `api-conventions.md`

**Avoid:**
- `everything.md`

### Use Conditional Inclusion Wisely

Apply conditional steering to avoid overwhelming the agent with irrelevant context.

- Frontend guidelines only for frontend files
- Database guidelines only for model/repository files
- API guidelines only for API endpoint files

### Reference Specifications

When you have formal specifications (OpenAPI, GraphQL, JSON Schema), reference them in steering files rather than duplicating their content.

### Update as You Learn

Steering files should evolve with your project. When you make architectural decisions or establish new patterns, document them in steering files.

### Be Specific and Actionable

Write guidelines that are clear and actionable, not vague principles.

**Good:**
```markdown
- Use Zod for runtime validation
- Prefix private methods with underscore
- Maximum function length: 50 lines
```

**Avoid:**
```markdown
- Write good code
- Be consistent
- Follow best practices
```

## Examples

### Example 1: Always-Included Standards

**`.kiro/steering/code-quality.md`**

```markdown
# Code Quality Standards

## General Principles

- Write self-documenting code with clear variable names
- Keep functions small and focused (single responsibility)
- Avoid deep nesting (max 3 levels)
- Handle errors explicitly, never silently fail

## TypeScript Specific

- Enable strict mode in tsconfig.json
- Avoid `any` type - use `unknown` if type is truly unknown
- Use type guards for narrowing
- Prefer `const` over `let`, never use `var`

## Testing

- Write tests before fixing bugs (TDD for bug fixes)
- Test behavior, not implementation
- Use descriptive test names: "should return error when user not found"
- Mock external dependencies
```

### Example 2: Conditional API Guidelines

**`.kiro/steering/rest-api.md`**

```markdown
---
inclusion: fileMatch
fileMatchPattern: 'src/api/**/*.ts'
---

# REST API Guidelines

## Endpoint Design

- Use plural nouns: `/users`, `/orders`, `/products`
- Use HTTP methods correctly:
  - GET: Retrieve resources
  - POST: Create resources
  - PUT: Replace resources
  - PATCH: Update resources
  - DELETE: Remove resources

## Request Validation

```typescript
import { z } from 'zod';

const CreateUserSchema = z.object({
  email: z.string().email(),
  name: z.string().min(1).max(100),
  age: z.number().int().positive().optional()
});

// Validate in route handler
const result = CreateUserSchema.safeParse(req.body);
if (!result.success) {
  return res.status(400).json({ error: result.error });
}
```

## Response Format

Success:
```json
{
  "data": { ... },
  "meta": {
    "timestamp": "2025-10-08T12:00:00Z"
  }
}
```

Error:
```json
{
  "error": "User not found",
  "code": "USER_NOT_FOUND",
  "details": { "userId": 123 }
}
```

## Authentication

- Use JWT tokens in Authorization header
- Validate tokens in middleware
- Return 401 for invalid/missing tokens
- Return 403 for insufficient permissions
```

### Example 3: OpenAPI Integration

**`.kiro/steering/api-contract.md`**

```markdown
# API Contract

Our API contract is defined in OpenAPI format:

#[[file:../docs/openapi.yaml]]

## Implementation Requirements

1. All endpoints must match the OpenAPI spec exactly
2. Use the spec's schema definitions for validation
3. Response codes must match spec
4. Generate TypeScript types from spec using `openapi-typescript`

## Validation

```bash
# Validate spec
npm run validate:openapi

# Generate types
npm run generate:api-types
```

## Testing

- Test all endpoints against spec using `jest-openapi`
- Ensure request/response examples in spec are valid
- Update spec before implementing new endpoints
```

### Example 4: Manual Deployment Guide

**`.kiro/steering/deployment.md`**

```markdown
---
inclusion: manual
---

# Deployment Guide

## Prerequisites

- Docker installed
- kubectl configured
- Access to container registry

## Build Process

```bash
# Install dependencies
npm ci

# Run tests
npm test

# Build application
npm run build

# Build Docker image
docker build -t myapp:$(git rev-parse --short HEAD) .
```

## Deploy to Staging

```bash
# Tag image
docker tag myapp:$(git rev-parse --short HEAD) registry.example.com/myapp:staging

# Push to registry
docker push registry.example.com/myapp:staging

# Update Kubernetes
kubectl set image deployment/myapp myapp=registry.example.com/myapp:staging -n staging

# Verify deployment
kubectl rollout status deployment/myapp -n staging
```

## Deploy to Production

1. Ensure staging tests pass
2. Create release tag: `git tag v1.2.3`
3. Push tag: `git push origin v1.2.3`
4. CI/CD will automatically deploy to production
5. Monitor logs: `kubectl logs -f deployment/myapp -n production`

## Rollback

```bash
# Rollback to previous version
kubectl rollout undo deployment/myapp -n production

# Rollback to specific revision
kubectl rollout undo deployment/myapp --to-revision=2 -n production
```
```

## Troubleshooting

### Steering File Not Being Applied

**Check:**
1. File is in `.kiro/steering/` directory
2. File has `.md` extension
3. Front-matter syntax is correct (if using conditional/manual)
4. For conditional: matching files are actually being read into context
5. For manual: you're referencing it with the correct context key

### File Reference Not Working

**Check:**
1. Syntax is exactly `#[[file:<path>]]`
2. Path is relative to the steering file location
3. Referenced file exists at that path
4. No typos in filename

### Too Much Context

If you have many always-included steering files and context feels overwhelming:

1. Convert some to conditional inclusion
2. Split large files into focused, smaller files
3. Use manual inclusion for rarely-needed guidelines

### Conflicting Guidelines

If multiple steering files provide conflicting guidance:

1. Be explicit about precedence in the files themselves
2. Consolidate related guidelines into single files
3. Use conditional inclusion to separate concerns by file pattern

## Summary

Steering files give you powerful control over Kiro's behavior:

- **Always included**: Team standards that apply everywhere
- **Conditional**: Domain-specific guidelines for parts of your codebase
- **Manual**: Reference documentation you invoke when needed
- **File references**: Include specifications that influence implementation

Use them to encode your team's knowledge, maintain consistency, and ensure Kiro understands your project's unique requirements and constraints.
