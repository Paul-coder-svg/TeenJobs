FROM gcc:14-bookworm

RUN apt-get update \\
    && apt-get install -y --no-install-recommends curl \\
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY TeenJobs_main_2FA.cpp main.cpp

RUN g++ -std=c++17 -O2 main.cpp -o TeenJobs

EXPOSE 10000
CMD ["./TeenJobs"]
