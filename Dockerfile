FROM gcc:13-bookworm

WORKDIR /app

RUN apt-get update \
    && apt-get install -y --no-install-recommends libpq-dev \
    && rm -rf /var/lib/apt/lists/*

COPY main.cpp .

RUN g++ -std=c++17 -O2 main.cpp -o TeenJobs -lpq

EXPOSE 10000

CMD ["./TeenJobs"]
