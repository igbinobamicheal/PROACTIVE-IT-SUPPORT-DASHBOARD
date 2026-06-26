# Deployment Guide

Follow these steps to deploy the Proactive IT Dashboard in a production environment.

## 1. Database Deployment (Docker Option)
Using the provided `docker-compose.yml` is the simplest way to get MySQL 8 running:
```bash
docker-compose up -d
```
This boots MySQL 8, exposes port 3306 and 33060, and mounts volume storage. It also automatically runs `schema.sql` and `seed.sql` to initialize the database tables.

## 2. Server Configuration
Create `/backend/config/config.json` containing the database host, user, and password matching your production credentials:
```json
{
  "db_host": "database_host",
  "db_port": 33060,
  "db_user": "root",
  "db_password": "secure_production_password",
  "db_schema": "it_monitoring",
  "server_host": "0.0.0.0",
  "server_port": 8080,
  "jwt_secret": "long_random_alphanumeric_signing_key_12345",
  "jwt_expiration_hours": 12,
  "bcrypt_work_factor": 12
}
```

## 3. Building the Backend Binary
On Linux:
```bash
cd backend
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg_root]/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release ..
make
./proactive_it_dashboard
```

## 4. Serving the Web Frontend
The `/frontend` files are completely static. You can serve them using Nginx, Apache, or any static file host.
Example Nginx configuration:
```nginx
server {
    listen 80;
    server_name monitoring.company.local;

    location / {
        root /var/www/dashboard/frontend;
        index index.html;
    }

    location /api/ {
        proxy_pass http://localhost:8080/api/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```
