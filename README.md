# Proactive IT Support Dashboard

A high-performance system monitoring solution with a C++17 Crow-based web server, MySQL 8 storage, a C++ monitoring agent, and a modern glassmorphic web UI frontend.

## Project Structure
- `/backend`: The REST web server compiled with C++17, utilizing Crow, JWT authentication, and MySQL X DevAPI.
- `/agent`: C++ metric collection agent that posts local host metrics (CPU, RAM, Disk, Uptime) to the backend.
- `/database`: Schema initialization and seed scripts for MySQL 8.
- `/frontend`: Modern glassmorphic web interface showing real-time agent statuses and ingested metrics.
- `/docs`: Architecture, API specifications, and deployment documentation.
- `/scripts`: Automation batch scripts for building and launching components.

## Prerequisites
1. **C++17 Compiler** (MSVC 2022+ / GCC 9+)
2. **CMake 3.15+**
3. **vcpkg** package manager with:
   - `crow`
   - `mysql-connector-cpp` (X DevAPI)
   - `jwt-cpp`
   - `nlohmann-json`
   - `openssl`
4. **MySQL 8.0 Server** (listening with X Protocol active on port 33060)

## Quick Start

### 1. Database Setup
Execute `database/schema.sql` and `database/seed.sql` on your MySQL server to set up the `it_monitoring` database and seed a default admin account.

### 2. Configure Backend
Copy `backend/config/config.json.template` to `backend/config/config.json` and configure your database credentials.

### 3. Build & Run Backend
Using vcpkg toolchain integration:
```bash
cd backend
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg_root]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
./build/Release/proactive_it_dashboard
```

### 4. Build & Run Agent
```bash
cd agent
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg_root]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
./build/Release/proactive_it_agent
```
On first start, provide the administrator JWT token (obtained via POST `/api/login` using username `admin` and password `admin123`) to register the agent device.
