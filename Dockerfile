FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY main.cpp .

RUN g++ -std=c++17 -O2 -pthread main.cpp -o TeenJobs

FROM debian:bookworm-slim

WORKDIR /app
COPY --from=build /app/TeenJobs /app/TeenJobs

ENV PORT=18080

EXPOSE 18080

CMD ["/app/TeenJobs"]
