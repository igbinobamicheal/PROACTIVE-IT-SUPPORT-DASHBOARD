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

## Render Backend Environment Variables

When deploying the backend on Render, add these in **Service** -> **Environment**:

| Key | Value |
|-----|-------|
| `DB_HOST` | Your MySQL host, for example `mysql.example.com` |
| `DB_PORT` | `33060` for MySQL X Protocol |
| `DB_USER` | Your MySQL username |
| `DB_PASSWORD` | Your MySQL password |
| `DB_NAME` | `it_monitoring` |
| `JWT_SECRET` | A long random secret string |
| `REGISTRATION_KEY` | A long random key shared with agents |
| `SERVER_HOST` | `0.0.0.0` |

Render automatically provides `PORT`, and the backend will use it if present. For local runs, the backend still supports `config.json`; environment variables override `config.json` when both are present.

## Deploying the Frontend to Vercel (from GitHub)

The `frontend/` directory is a static HTML/CSS/JS site — no build step required. Vercel serves it directly.

### Step 1: Push Your Code to GitHub

Make sure all your latest changes are committed and pushed:

```bash
git add -A
git commit -m "prepare for Vercel deployment"
git push origin main
```

### Step 2: Import the Project on Vercel

1. Go to [vercel.com](https://vercel.com) and sign in with your GitHub account.
2. Click **"Add New…"** → **"Project"**.
3. Find and select the **PROACTIVE-IT-SUPPORT-DASHBOARD** repository from the list.
4. Click **"Import"**.

### Step 3: Configure the Project Settings

On the configuration screen before deploying, set the following:

| Setting              | Value        |
|----------------------|--------------|
| **Framework Preset** | `Other`      |
| **Root Directory**   | `frontend`   |
| **Build Command**    | *(leave empty — no build step)* |
| **Output Directory** | `.`          |

> **Important:** Click **"Edit"** next to Root Directory and type `frontend`. This tells Vercel to serve only the frontend folder, not the entire repo.

### Step 4: Set the Backend API URL (Environment Variable)

If your C++ backend is running on a public server, you need to tell the frontend where to find it:

1. Expand the **"Environment Variables"** section on the same page.
2. This project reads the API URL from a script tag. To configure it for production, add a `<script>` before `api.js` in each HTML file:

```html
<script>window.API_BASE_URL = 'https://your-backend-server.com/api';</script>
```

Alternatively, for local development and demo purposes, the frontend defaults to `http://localhost:8080/api`.

### Step 5: Deploy

1. Click **"Deploy"**.
2. Vercel will deploy your static files in ~10 seconds.
3. You'll receive a live URL like `https://proactive-it-support-dashboard.vercel.app`.

### Step 6: Automatic Deployments

After the initial setup, every `git push` to `main` will automatically trigger a new deployment on Vercel. No manual steps needed.

### Clean URLs

The included `vercel.json` configures clean URL rewrites, so these all work:

- `yourdomain.vercel.app/dashboard` → `dashboard.html`
- `yourdomain.vercel.app/login` → `login.html`
- `yourdomain.vercel.app/devices` → `devices.html`
- `yourdomain.vercel.app/alerts` → `alerts.html`
- `yourdomain.vercel.app/device-details` → `device-details.html`
