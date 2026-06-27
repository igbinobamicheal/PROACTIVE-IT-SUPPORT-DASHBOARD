# Deployment Guide

This guide describes how to deploy the Proactive IT Dashboard in a production environment using **Supabase** for the database, **Railway** for the backend container, and **Vercel** for the static frontend.

---

## 1. Database Setup (Supabase)

Supabase provides hosted PostgreSQL databases. 

### Recommended Configuration
The dashboard backend uses PostgreSQL **prepared statements** for all database queries. Prepared statements are not supported by connection poolers in Transaction Mode.

* **Option A (Recommended)**: Use the **Direct Connection String** (port `5432`).
* **Option B**: If using the connection pooler (port `6543`), configure the pool mode to **Session** instead of Transaction in the Supabase dashboard.

### Environment Variable Format
Your Supabase connection string should look like this:
```env
DATABASE_URL="postgres://postgres.[YOUR_PROJECT_ID]:[YOUR_PASSWORD]@aws-0-[REGION].pooler.supabase.com:5432/postgres?sslmode=require"
```

---

## 2. Backend Deployment (Railway)

Railway is used to build and deploy the C++ Crow backend from the provided `Dockerfile`.

### Steps:
1. Connect your GitHub repository to Railway.
2. Add a new service from the repository and select the root directory (Railway will automatically detect the `backend/Dockerfile` if configured to look at the `backend` subdirectory, or you can specify the Root Directory path as `/backend` in Railway settings).
3. Expose the port by setting the `PORT` environment variable to `8080`.
4. Configure the following **Environment Variables** in the Railway service settings:

| Variable | Description | Example |
|---|---|---|
| `DATABASE_URL` | Complete connection string to Supabase | `postgres://postgres.xxx...:5432/postgres?sslmode=require` |
| `PORT` | Port for the backend server | `8080` (Railway automatically binds to this) |
| `JWT_SECRET` | Secret key used for signing JWT login tokens | `a_long_random_alphanumeric_signing_key_1234567890` |
| `JWT_EXPIRATION_HOURS` | Lifetime of a user session token | `24` |
| `BCRYPT_WORK_FACTOR` | Work factor for hashing passwords | `10` |
| `REGISTRATION_KEY` | Key required for agents to register | `secure_agent_shared_key_12345` |

---

## 3. Frontend Deployment (Vercel)

Vercel is used to host the static HTML/CSS/JS frontend.

### Steps:
1. Import your project into Vercel.
2. Set the root directory of the Vercel project to `/` (workspace root). The configuration file `vercel.json` in the root directs the build to output the `frontend` folder.
3. Configure the **Build Command** and **Environment Variables** under Project Settings:
   - **Framework Preset**: Other (Static)
   - **Build Command**: `node generate-config.js`
   - **Output Directory**: `frontend`
4. Add the following **Environment Variable** in Vercel:

| Variable | Description | Example |
|---|---|---|
| `API_URL` | The URL of your Railway backend API | `https://your-backend-service.up.railway.app/api` |

During the deployment, the build script will automatically run, create `frontend/js/config.js`, and inject your Railway URL so the frontend can interact with your backend service.

---

## 4. Local Development Fallback

For local development, when running the application without Vercel's build command:
1. The file `frontend/js/config.js` defaults to comments.
2. The frontend automatically falls back to `http://localhost:8080/api`.
3. The backend can be run using the standard `config.json` file or local environment variables.
