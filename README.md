# TeenJobs — Bunny-compatible build

This version keeps the TeenJobs application and UI but replaces the Windows-only Winsock startup/shutdown code with a portable socket layer that builds on Windows and Linux.

## Windows

```powershell
g++ main.cpp -o TeenJobs.exe -lws2_32
```

Set the admin credentials before starting:

```powershell
$env:TEENJOBS_ADMIN_EMAIL="your-admin-email@example.com"
$env:TEENJOBS_ADMIN_PASSWORD="use-a-long-random-password"
.\TeenJobs.exe
```

## Linux / Bunny container

```bash
g++ -std=c++17 -O2 -pthread main.cpp -o TeenJobs
TEENJOBS_ADMIN_EMAIL="your-admin-email@example.com" \
TEENJOBS_ADMIN_PASSWORD="use-a-long-random-password" \
PORT=18080 ./TeenJobs
```

The server listens on `0.0.0.0`, and uses `PORT` when provided. This is required for a container platform to reach the app.

## Docker

Build:

```bash
docker build -t teenjobs .
```

Run:

```bash
docker run --rm -p 18080:18080 \
  -e TEENJOBS_ADMIN_EMAIL="your-admin-email@example.com" \
  -e TEENJOBS_ADMIN_PASSWORD="use-a-long-random-password" \
  -e PORT=18080 \
  -v teenjobs-data:/app \
  teenjobs
```

## Data

The current prototype stores `users.txt`, `jobs.txt`, and `applications.txt` in the application's working directory. Mount persistent storage in production so container restarts do not erase the data.

The application is still a prototype: passwords are stored in plaintext and the text-file format is not suitable for a high-traffic production service. Before accepting real users, move authentication to hashed passwords and a real database.
