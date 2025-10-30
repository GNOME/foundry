# Forgejo API Client Implementation Summary

This document provides a comprehensive guide for implementing a client library or application that interacts with the Forgejo API. It is based on the official Forgejo documentation and outlines all critical aspects needed for API client development.

## Table of Contents

1. [API Overview](#api-overview)
2. [Base URL and Versioning](#base-url-and-versioning)
3. [Authentication](#authentication)
4. [API Endpoints Reference](#api-endpoints-reference)
5. [Request/Response Formats](#requestresponse-formats)
6. [Pagination](#pagination)
7. [Error Handling](#error-handling)
8. [Token Scopes](#token-scopes)
9. [OAuth2 Integration](#oauth2-integration)
10. [Webhooks](#webhooks)
11. [Package Registry APIs](#package-registry-apis)
12. [Best Practices](#best-practices)

---

## API Overview

### Version Compatibility

- **Compatibility Guarantee**: All Forgejo versions sharing the same major version number (e.g., all 7.x.x versions) maintain API compatibility.
- **Breaking Changes**: Breaking changes (e.g., endpoint removal) occur only when the major version number changes.
- **Version Format**: Forgejo versions follow the format `X.Y.Z+gitea-A.B.C` (e.g., `7.0.0+gitea-1.22.0`).
- **Gitea Compatibility**: Forgejo 7.0.0+ is compatible with tools designed for Gitea 1.22.0 and below.

### API Discovery

- **Swagger UI**: Available at `https://forgejo.your.host/api/swagger`
- **OpenAPI Specification**: Available at `https://forgejo.your.host/swagger.v1.json`
- **Version Endpoint**: `GET /api/v1/version` or `GET /api/forgejo/v1/version`
- **API Settings**: `GET /api/v1/settings/api` - Returns pagination defaults and limits

---

## Base URL and Versioning

All API requests should be prefixed with `/api/v1/`:

```
Base URL: https://forgejo.your.host/api/v1
```

### Example Endpoints

- Repositories: `/api/v1/repos`
- Users: `/api/v1/users`
- Issues: `/api/v1/repos/:owner/:repo/issues`
- Pull Requests: `/api/v1/repos/:owner/:repo/pulls`

---

## Authentication

Forgejo supports three authentication methods. Clients should support all three for maximum compatibility:

### 1. HTTP Basic Authentication

```http
Authorization: Basic base64(username:password)
```

Or using curl:
```bash
curl -u username:password https://forgejo.your.host/api/v1/...
```

### 2. Bearer Token (OAuth2)

```http
Authorization: Bearer <access_token>
```

### 3. Token Header (Legacy API Tokens)

```http
Authorization: token <api_token>
```

**Note**: For historical reasons, API key tokens require the word `token` before the token value in the Authorization header.

### Two-Factor Authentication (2FA)

When using HTTP Basic Authentication with 2FA enabled, include the one-time password:

```http
X-Forgejo-OTP: 123456
```

Where `123456` is the 6-digit rotating token from the authenticator app.

### Sudo (Admin Feature)

Admin users can impersonate other users by adding:

- Query parameter: `?sudo=username`
- Or header: `Sudo: username`

---

## API Endpoints Reference

### Authentication Endpoints

#### Generate API Token

```http
POST /api/v1/users/:username/tokens
Content-Type: application/json
Authorization: Basic base64(username:password)
```

**Request Body:**
```json
{
  "name": "token-name",
  "scopes": ["read:repository", "write:repository"]
}
```

**Response:**
```json
{
  "id": 1,
  "name": "token-name",
  "sha1": "9fcb1158165773dd010fca5f0cf7174316c3e37d",
  "token_last_eight": "16c3e37d",
  "scopes": ["read:repository", "write:repository"]
}
```

**Important**: The `sha1` field contains the actual token and is **only returned once**. Store it securely.

#### List API Tokens

```http
GET /api/v1/users/:username/tokens
Authorization: Basic base64(username:password)
```

**Response:**
```json
[
  {
    "name": "token-name",
    "sha1": "",
    "token_last_eight": "16c3e37d"
  }
]
```

Note: The `sha1` field is empty when listing tokens for security reasons.

### Repository Endpoints

The API follows RESTful conventions. Common operations:

- `GET /api/v1/repos/search` - Search repositories
- `GET /api/v1/repos/:owner/:repo` - Get repository details
- `POST /api/v1/user/repos` - Create repository
- `PUT /api/v1/repos/:owner/:repo` - Update repository
- `DELETE /api/v1/repos/:owner/:repo` - Delete repository

### Issue Endpoints

- `GET /api/v1/repos/:owner/:repo/issues` - List issues
- `POST /api/v1/repos/:owner/:repo/issues` - Create issue
- `GET /api/v1/repos/:owner/:repo/issues/:index` - Get issue
- `PATCH /api/v1/repos/:owner/:repo/issues/:index` - Update issue

### Pull Request Endpoints

- `GET /api/v1/repos/:owner/:repo/pulls` - List pull requests
- `POST /api/v1/repos/:owner/:repo/pulls` - Create pull request
- `GET /api/v1/repos/:owner/:repo/pulls/:index` - Get pull request
- `PATCH /api/v1/repos/:owner/:repo/pulls/:index` - Update pull request

### User Endpoints

- `GET /api/v1/user` - Get authenticated user
- `GET /api/v1/users/:username` - Get user profile
- `GET /api/v1/user/repos` - List repositories for authenticated user

### Organization Endpoints

- `GET /api/v1/orgs` - List organizations
- `GET /api/v1/orgs/:org` - Get organization
- `POST /api/v1/orgs` - Create organization

### Package Endpoints

- `GET /api/v1/packages/:owner` - List packages for owner
- `GET /api/v1/packages/:owner/:type/:name` - Get package details
- `DELETE /api/v1/packages/:owner/:type/:name` - Delete package

**Note**: The full API reference is available via Swagger at `/api/swagger` or OpenAPI spec at `/swagger.v1.json`.

---

## Request/Response Formats

### Content-Type Headers

- **Request**: `Content-Type: application/json`
- **Response**: `Accept: application/json`

### Standard Request Format

```http
POST /api/v1/repos/:owner/:repo/issues
Content-Type: application/json
Authorization: token <api_token>

{
  "title": "Issue title",
  "body": "Issue description"
}
```

### Standard Response Format

API responses typically return JSON objects or arrays:

```json
{
  "id": 1,
  "title": "Issue title",
  "body": "Issue description",
  "state": "open",
  "created_at": "2024-01-01T00:00:00Z"
}
```

---

## Pagination

All list endpoints support pagination using query parameters:

### Pagination Parameters

- `page` - Page number (default: 1)
- `limit` - Items per page (default: 30, max: 50)

### Pagination Headers

Responses include pagination metadata:

```http
Link: <http://localhost/api/v1/repos/search?limit=1&page=2>; rel="next",
      <http://localhost/api/v1/repos/search?limit=1&page=5252>; rel="last"
X-Total-Count: 5252
```

### Getting Pagination Settings

```http
GET /api/v1/settings/api
```

**Response:**
```json
{
  "max_response_items": 50,
  "default_paging_num": 30,
  "default_git_trees_per_page": 1000,
  "default_max_blob_size": 10485760
}
```

### Pagination Implementation

Clients should:
1. Parse the `Link` header to find next/previous/last page URLs
2. Use `X-Total-Count` header to determine total number of items
3. Respect `max_response_items` limit (default: 50)
4. Implement automatic pagination helpers for convenience

---

## Error Handling

### HTTP Status Codes

Standard HTTP status codes are used:

- `200 OK` - Successful request
- `201 Created` - Resource created successfully
- `204 No Content` - Successful deletion
- `400 Bad Request` - Invalid request parameters
- `401 Unauthorized` - Authentication required
- `403 Forbidden` - Insufficient permissions
- `404 Not Found` - Resource doesn't exist
- `422 Unprocessable Entity` - Validation error
- `500 Internal Server Error` - Server error

### Error Response Format

```json
{
  "message": "Error description",
  "url": "https://forgejo.your.host/api/swagger#/..."
}
```

### Client Error Handling Recommendations

1. Check HTTP status codes
2. Parse error message from response body
3. Implement retry logic for transient errors (5xx)
4. Handle rate limiting (if implemented)
5. Provide meaningful error messages to end users

---

## Token Scopes

Forgejo uses scoped access tokens to restrict API access. All tokens require at least one scope.

### Scope Format

Scopes follow the pattern: `{action}:{resource}`

- `read:` - GET operations only
- `write:` - GET, POST, PUT, PATCH, DELETE operations

### Available Scopes

| Scope | Description | Endpoints |
|-------|-------------|-----------|
| `read:activitypub` | Read ActivityPub operations | `activitypub/*` |
| `write:activitypub` | Full ActivityPub operations | `activitypub/*` |
| `read:admin` | Read admin operations | `/admin/*` |
| `write:admin` | Full admin operations | `/admin/*` |
| `read:issue` | Read issue operations | `issues/*`, `labels/*`, `milestones/*` |
| `write:issue` | Full issue operations | `issues/*`, `labels/*`, `milestones/*` |
| `read:misc` | Read miscellaneous operations | Miscellaneous routes |
| `write:misc` | Full miscellaneous operations | Miscellaneous routes |
| `read:notification` | Read notifications | `notification/*` |
| `write:notification` | Full notification operations | `notification/*` |
| `read:organization` | Read org/team operations | `orgs/*`, `teams/*` |
| `write:organization` | Full org/team operations | `orgs/*`, `teams/*` |
| `read:package` | Read package operations | `/packages/*` |
| `write:package` | Full package operations | `/packages/*` |
| `read:repository` | Read repository operations | `/repos/*` (except issues) |
| `write:repository` | Full repository operations | `/repos/*` (except issues) |
| `read:user` | Read user operations | `/user/*`, `/users/*` |
| `write:user` | Full user operations | `/user/*`, `/users/*` |

### Scope Selection Strategy

1. **Principle of Least Privilege**: Request only the scopes needed
2. **Combining Scopes**: Multiple scopes can be requested in a single token
3. **Scope Validation**: Validate token has required scope before API calls
4. **Scope Documentation**: Document which scopes are required for each operation

---

## OAuth2 Integration

Forgejo acts as an OAuth2 provider, supporting Authorization Code Grant with PKCE and OpenID Connect.

### OAuth2 Endpoints

| Endpoint | URL |
|----------|-----|
| OpenID Connect Discovery | `/.well-known/openid-configuration` |
| Authorization Endpoint | `/login/oauth/authorize` |
| Access Token Endpoint | `/login/oauth/access_token` |
| OpenID Connect UserInfo | `/login/oauth/userinfo` |
| JSON Web Key Set | `/login/oauth/keys` |

### Supported Grant Types

1. **Authorization Code Grant** (standard)
2. **Authorization Code Grant with PKCE** (for public clients)
3. **OpenID Connect** (OIDC)

### Client Types

- **Confidential Clients**: Require client secret
- **Public Clients**: Use PKCE, no client secret required

### OAuth2 Flow (Confidential Client)

1. **Authorization Request**:
   ```
   GET /login/oauth/authorize?client_id=CLIENT_ID&redirect_uri=REDIRECT_URI&response_type=code&state=STATE
   ```

2. **Authorization Response**:
   ```
   REDIRECT_URI?code=AUTHORIZATION_CODE&state=STATE
   ```

3. **Token Exchange**:
   ```http
   POST /login/oauth/access_token
   Content-Type: application/json
   
   {
     "client_id": "CLIENT_ID",
     "client_secret": "CLIENT_SECRET",
     "code": "AUTHORIZATION_CODE",
     "grant_type": "authorization_code",
     "redirect_uri": "REDIRECT_URI"
   }
   ```

4. **Token Response**:
   ```json
   {
     "access_token": "eyJhbGci...",
     "token_type": "bearer",
     "expires_in": 3600,
     "refresh_token": "eyJhbGci..."
   }
   ```

### OAuth2 Flow (Public Client with PKCE)

1. **Generate Code Verifier**: Random string, 43-128 characters, containing alphanumeric and `-`, `.`, `_`, `~`

2. **Generate Code Challenge**:
   - **S256 method**: `base64url(sha256(code_verifier))`
   - **Plain method**: Use `code_verifier` directly

3. **Authorization Request**:
   ```
   GET /login/oauth/authorize?client_id=CLIENT_ID&redirect_uri=REDIRECT_URI&response_type=code&code_challenge_method=S256&code_challenge=CODE_CHALLENGE&state=STATE
   ```

4. **Token Exchange**:
   ```http
   POST /login/oauth/access_token
   Content-Type: application/json
   
   {
     "client_id": "CLIENT_ID",
     "code": "AUTHORIZATION_CODE",
     "grant_type": "authorization_code",
     "redirect_uri": "REDIRECT_URI",
     "code_verifier": "CODE_VERIFIER"
   }
   ```

### OAuth2 Token Usage

OAuth2 tokens can be used in three ways:

1. **Authorization Header**: `Authorization: Bearer <access_token>`
2. **Query Parameter**: `?token=<access_token>`
3. **Query Parameter (Alt)**: `?access_token=<access_token>`

### Important OAuth2 Notes

- **Scopes**: OAuth2 tokens currently do **not** support scopes and grant full user permissions
- **Security**: Prefer scoped API tokens or SSH keys for repository access when security is critical
- **Application Registration**: Applications can be registered at `/user/settings/applications`
- **Admin Applications**: Instance-wide OAuth2 apps created at `/admin/applications` grant admin rights

---

## Webhooks

Webhooks allow repositories to send notifications to external services on events.

### Webhook Types

#### Raw Event Payloads

- **Forgejo**: JSON or FORM payload (GET or POST)
- **Gitea**: JSON or FORM payload (GET or POST)
- **Gogs**: JSON payload (POST)

#### Dedicated Integrations

- **Packagist**: Refresh package notification
- **SourceHut Builds**: Submit build manifests (push events only)

#### Generic Notification Channels

- Matrix, Slack, Discord, Dingtalk, Telegram, Microsoft Teams, Feishu/Lark Suite, WeCom

### Webhook Headers

```http
X-Forgejo-Delivery: f6266f16-1bf3-46a5-9ea4-602e06ead473
X-Forgejo-Event: push
X-Forgejo-Signature: sha256=<signature>
```

Legacy headers (for compatibility):
- `X-GitHub-Delivery`, `X-GitHub-Event`
- `X-Gitea-Delivery`, `X-Gitea-Event`
- `X-Gogs-Delivery`, `X-Gogs-Event`

### Webhook Payload Example (Push Event)

```json
{
  "ref": "refs/heads/develop",
  "before": "28e1879d029cb852e4844d9c718537df08844e03",
  "after": "bffeb74224043ba2feb48d137756c8a9331c449a",
  "compare_url": "http://localhost:3000/forgejo/webhooks/compare/...",
  "commits": [
    {
      "id": "bffeb74224043ba2feb48d137756c8a9331c449a",
      "message": "Webhooks Yay!",
      "url": "http://localhost:3000/forgejo/webhooks/commit/...",
      "author": {
        "name": "Forgejo",
        "email": "someone@forgejo.org",
        "username": "forgejo"
      },
      "committer": {
        "name": "Forgejo",
        "email": "someone@forgejo.org",
        "username": "forgejo"
      },
      "timestamp": "2017-03-13T13:52:11-04:00"
    }
  ],
  "repository": {
    "id": 140,
    "owner": {
      "id": 1,
      "login": "forgejo",
      "full_name": "Forgejo",
      "email": "someone@forgejo.org",
      "avatar_url": "https://localhost:3000/avatars/1",
      "username": "forgejo"
    },
    "name": "webhooks",
    "full_name": "forgejo/webhooks",
    "description": "",
    "private": false,
    "fork": false,
    "html_url": "http://localhost:3000/forgejo/webhooks",
    "ssh_url": "ssh://forgejo@localhost:2222/forgejo/webhooks.git",
    "clone_url": "http://localhost:3000/forgejo/webhooks.git",
    "default_branch": "master",
    "created_at": "2017-02-26T04:29:06-05:00",
    "updated_at": "2017-03-13T13:51:58-04:00"
  },
  "pusher": {
    "id": 1,
    "login": "forgejo",
    "username": "forgejo"
  },
  "sender": {
    "id": 1,
    "login": "forgejo",
    "username": "forgejo"
  }
}
```

### Webhook Signature Verification

Webhooks include an HMAC SHA-256 signature in the `X-Forgejo-Signature` header:

```php
// Example PHP verification
$secret_key = 'webhook_secret';
$payload = file_get_contents('php://input');
$header_signature = $_SERVER['HTTP_X_FORGEJO_SIGNATURE'];
$payload_signature = hash_hmac('sha256', $payload, $secret_key, false);

if ($header_signature !== $payload_signature) {
    // Invalid signature
    exit();
}
```

### Webhook Configuration

Webhooks can be configured at:
- Repository level: `/:username/:reponame/settings/hooks`
- Organization level: Organization settings
- Instance level: Admin panel (default webhooks)

### Webhook Authorization Header

Webhooks can be configured to send an `Authorization` header to the target URL. The authentication string is stored encrypted in the database.

---

## Package Registry APIs

Forgejo supports package registries for multiple package managers. Package operations use the `/packages/*` API routes.

### Supported Package Managers

| Package Manager | Language | Client Tools |
|----------------|----------|--------------|
| Alpine | - | `apk` |
| ALT | - | `apt-rpm` |
| Arch | - | `pacman` |
| Cargo | Rust | `cargo` |
| Chef | - | `knife` |
| Composer | PHP | `composer` |
| Conan | C++ | `conan` |
| Conda | - | `conda` |
| Container | - | OCI compliant clients |
| CRAN | R | - |
| Debian | - | `apt` |
| Generic | - | Any HTTP client |
| Go | Go | `go` |
| Helm | - | HTTP client, `cm-push` |
| Maven | Java | `mvn`, `gradle` |
| npm | JavaScript | `npm`, `yarn`, `pnpm` |
| NuGet | .NET | `nuget` |
| Pub | Dart | `dart`, `flutter` |
| PyPI | Python | `pip`, `twine` |
| RPM | - | `yum`, `dnf` |
| RubyGems | Ruby | `gem`, `Bundler` |
| Swift | Swift | `swift` |
| Vagrant | - | `vagrant` |

### Package Access Control

| Owner Type | Read Access | Write Access |
|------------|-------------|--------------|
| User | Public if user is public, otherwise user only | Owner only |
| Organization | Public if org is public, otherwise org members only | Org members with admin/write access |

### Package Scopes

- `read:package` - Read and download packages
- `write:package` - Read, write, and delete packages (currently same as `read:package`)

### Package Endpoints

- `GET /api/v1/packages/:owner` - List packages
- `GET /api/v1/packages/:owner/:type/:name` - Get package details
- `DELETE /api/v1/packages/:owner/:type/:name` - Delete package

**Note**: Each package type has specific upload/download endpoints. Refer to individual package documentation for details.

### Package Linking

Packages belong to owners (users or organizations), not repositories. To link a package to a repository:

1. Upload package to owner
2. Open package settings
3. Select repository to link

The entire package (all versions) is linked, not just a single version.

---

## Best Practices

### 1. Authentication

- **Use Scoped Tokens**: Always request the minimum required scopes
- **Store Tokens Securely**: Never commit tokens to version control
- **Handle Token Expiration**: Implement token refresh for OAuth2 tokens
- **Support Multiple Auth Methods**: Allow users to choose authentication method

### 2. Rate Limiting

- Monitor rate limit headers (if implemented)
- Implement exponential backoff for retries
- Cache responses when appropriate

### 3. Error Handling

- Always check HTTP status codes
- Parse and display error messages from responses
- Implement retry logic for transient errors (5xx)
- Provide user-friendly error messages

### 4. Pagination

- Respect `max_response_items` limit (default: 50)
- Implement automatic pagination helpers
- Parse `Link` headers for navigation
- Use `X-Total-Count` for progress indicators

### 5. Request Headers

Always include:
```http
Content-Type: application/json
Accept: application/json
Authorization: token <token>  (or Bearer for OAuth2)
```

### 6. Version Discovery

- Check API version on startup: `GET /api/v1/version`
- Verify compatibility before making requests
- Handle version-specific features gracefully

### 7. Webhook Implementation

- Verify webhook signatures
- Validate event types
- Handle duplicate deliveries (use `X-Forgejo-Delivery` ID)
- Implement proper error handling in webhook endpoints

### 8. Package Registry

- Use appropriate authentication for package operations
- Handle package type-specific requirements
- Respect package access controls
- Implement proper cleanup for package uploads

### 9. Development

- Use Swagger UI for API exploration: `/api/swagger`
- Download OpenAPI spec for code generation: `/swagger.v1.json`
- Test with different token scopes
- Verify 2FA support if needed

### 10. Security

- Never log tokens or sensitive data
- Use HTTPS for all API requests
- Validate webhook signatures
- Follow principle of least privilege for scopes
- Implement CSRF protection for OAuth2 flows (use `state` parameter)

---

## Example Client Implementation Structure

```python
class ForgejoClient:
    def __init__(self, base_url, token=None, username=None, password=None):
        self.base_url = base_url.rstrip('/')
        self.token = token
        self.username = username
        self.password = password
    
    def _get_auth_headers(self):
        if self.token:
            return {'Authorization': f'token {self.token}'}
        elif self.username and self.password:
            # Use basic auth
            return {}
        return {}
    
    def _request(self, method, endpoint, **kwargs):
        url = f"{self.base_url}/api/v1{endpoint}"
        headers = {
            'Content-Type': 'application/json',
            'Accept': 'application/json',
            **self._get_auth_headers()
        }
        # Make HTTP request
        # Handle pagination
        # Handle errors
        pass
    
    def create_token(self, name, scopes):
        """Create API token"""
        # POST /api/v1/users/:username/tokens
        pass
    
    def list_repositories(self, page=1, limit=30):
        """List repositories with pagination"""
        # GET /api/v1/repos/search?page={page}&limit={limit}
        pass
    
    def create_issue(self, owner, repo, title, body):
        """Create issue"""
        # POST /api/v1/repos/:owner/:repo/issues
        pass
```

---

## Additional Resources

- **Swagger UI**: `https://forgejo.your.host/api/swagger`
- **OpenAPI Spec**: `https://forgejo.your.host/swagger.v1.json`
- **Version Endpoint**: `GET /api/v1/version`
- **API Settings**: `GET /api/v1/settings/api`
- **Delightful Forgejo**: SDK list at https://codeberg.org/forgejo-contrib/delightful-forgejo

---

## Summary

This API client implementation guide covers:

? **Authentication**: Three methods (Basic Auth, Bearer Token, Token Header)  
? **Token Management**: Token creation, listing, scopes  
? **OAuth2**: Full OAuth2 provider integration with PKCE support  
? **Pagination**: Standard pagination with `page` and `limit` parameters  
? **Token Scopes**: Comprehensive scope system for fine-grained access control  
? **Webhooks**: Event notifications with signature verification  
? **Package Registry**: Multi-package manager support  
? **Error Handling**: Standard HTTP status codes and error responses  
? **Best Practices**: Security, rate limiting, and development guidelines  

For complete endpoint documentation, refer to the Swagger UI or OpenAPI specification available on your Forgejo instance.