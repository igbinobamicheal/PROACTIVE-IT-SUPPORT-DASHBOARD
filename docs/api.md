# REST API Specifications

The proactive IT support dashboard backend exposes the following endpoints.

---

## 1. User Authentication

### POST `/api/login`
Authenticates an administrator.
- **Request Body**:
  ```json
  {
    "username": "admin",
    "password": "admin123"
  }
  ```
- **Response (200 OK)**:
  ```json
  {
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
  }
  ```
- **Response (401 Unauthorized)**:
  ```json
  {
    "error": "Invalid username or password"
  }
  ```

---

## 2. Device Registration

### POST `/api/register-device`
Registers a new monitoring agent.
- **Security**: Requires Authorization header `Bearer <user_jwt_token>`.
- **Request Body**:
  ```json
  {
    "name": "Production Web Server",
    "ip_address": "192.168.1.150"
  }
  ```
- **Response (201 Created)**:
  ```json
  {
    "id": 1,
    "name": "Production Web Server",
    "token": "f49b3f2e8a7b9f5d0c12d3e4f5...",
    "ip_address": "192.168.1.150",
    "status": "offline",
    "created_at": "2026-06-25 22:45:00"
  }
  ```

---

## 3. Metric Ingestion

### POST `/api/metrics`
Ingests CPU, RAM, Disk, Network and Uptime metrics from agents.
- **Security**: Requires header `X-Device-Token` containing the device's unique token.
- **Request Body**:
  ```json
  {
    "cpu_usage": 18.5,
    "ram_usage": 62.4,
    "disk_usage": 45.1,
    "network_in": 12450.5,
    "network_out": 25480.2,
    "uptime": 3600
  }
  ```
- **Response (201 Created)**:
  ```json
  {
    "message": "Metrics ingested successfully"
  }
  ```

---

## 4. Device Management

### GET `/api/devices`
Lists all registered monitoring agents.
- **Security**: Requires Authorization header `Bearer <user_jwt_token>`.
- **Response (200 OK)**:
  ```json
  [
    {
      "id": 1,
      "name": "Production Web Server",
      "token": "f49b3f2e...",
      "ip_address": "192.168.1.150",
      "status": "online",
      "created_at": "2026-06-25 22:45:00"
    }
  ]
  ```

### GET `/api/device/{id}`
Returns details for a specific device, including its latest metrics.
- **Security**: Requires Authorization header `Bearer <user_jwt_token>`.
- **Response (200 OK)**:
  ```json
  {
    "id": 1,
    "name": "Production Web Server",
    "token": "f49b3f2e...",
    "ip_address": "192.168.1.150",
    "status": "online",
    "created_at": "2026-06-25 22:45:00",
    "latest_metrics": {
      "id": 12,
      "device_id": 1,
      "cpu_usage": 18.5,
      "ram_usage": 62.4,
      "disk_usage": 45.1,
      "network_in": 12450.5,
      "network_out": 25480.2,
      "uptime": 3600,
      "timestamp": "2026-06-25 22:46:12"
    }
  }
  ```
