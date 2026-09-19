FROM gcc:13-bookworm

WORKDIR /app

COPY main.cpp .

RUN g++ -std=c++17 -O2 main.cpp -o TeenJobs

EXPOSE 10000

CMD ["./TeenJobs"]