# TeenJobs — Render build

This version is designed for Render's native web service environment and does not require Docker on your PC.

## Admins

Admin credentials are NOT hardcoded in `main.cpp`.

Use a secret `.env` file with entries such as:

TEENJOBS_ADMIN_1_EMAIL=owner@example.com
TEENJOBS_ADMIN_1_PASSWORD=your-password
TEENJOBS_ADMIN_1_NAME=Owner

TEENJOBS_ADMIN_2_EMAIL=moderator@example.com
TEENJOBS_ADMIN_2_PASSWORD=another-password
TEENJOBS_ADMIN_2_NAME=Moderator

The application supports up to 50 numbered admin accounts.

## Render

1. Push the project files to GitHub. Do NOT push `.env`.
2. In Render, open the TeenJobs web service.
3. Go to Environment.
4. Under Secret Files, add a secret file named `.env`.
5. Paste your real admin entries into it.
6. Save and deploy.

The program reads `/etc/secrets/.env` on Render and `.env` locally.

## Build / Start commands

Build:

g++ -std=c++17 -O2 main.cpp -o TeenJobs

Start:

./TeenJobs

The program automatically uses Render's `PORT` value and binds to `0.0.0.0`.

## Important demo limitation

The current app still uses simple text files for application data and stores passwords in those text files. That is acceptable for a short demo, but it should be replaced with password hashing and a real database before using TeenJobs with real users at scale.
