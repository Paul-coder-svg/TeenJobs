// ============================================================
// TEENJOBS - PORTABLE C++ WEB SERVER
//
// Builds on both Windows and Linux (including Bunny.net
// containers). The server binds to 0.0.0.0 and uses the PORT
// environment variable when supplied, defaulting to 18080.
// ============================================================

#ifdef _WIN32
    #define _WIN32_WINNT 0x0601
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
    using SOCKET = int;
    constexpr int INVALID_SOCKET = -1;
    constexpr int SOCKET_ERROR = -1;
    inline int closesocket(SOCKET s) { return ::close(s); }
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ============================================================
// TEENJOBS
// Single-file C++ website
//
// No Crow
// No SQLite
// No CMake
//
// Windows + Linux socket support
// ============================================================


// ============================================================
// ADMIN CONFIGURATION
// ============================================================
//
// IMPORTANT:
// Change these before using the site.
//
// The owner uses these credentials through:
// Business -> Login
//
// If the email/password match, the account receives ADMIN access.
//
// For a real public deployment, move these credentials into
// environment variables or another protected configuration.
//

std::string ADMIN_EMAIL;

std::string ADMIN_PASSWORD;


// ============================================================
// DATA STRUCTURES
// ============================================================

enum class UserRole
{
    TEEN,
    BUSINESS,
    ADMIN
};


struct User
{
    int id = 0;

    std::string name;
    std::string email;
    std::string password;

    int age = 0;

    UserRole role = UserRole::TEEN;

    bool removed = false;
};


struct Question
{
    int id = 0;

    std::string text;
};


struct Job
{
    int id = 0;

    int businessId = 0;

    std::string title;
    std::string company;
    std::string location;
    std::string description;

    int minAge = 13;
    int maxAge = 18;

    std::string jobType;
    std::string schedule;
    std::string pay;

    bool applicationsOpen = true;
    bool removed = false;

    // Unix timestamps.
    // publishAt = 0 means immediately.
    // expireAt = 0 means no expiration.
    long long publishAt = 0;
    long long expireAt = 0;

    std::vector<Question> questions;
};


struct Application
{
    int id = 0;

    int jobId = 0;
    int teenId = 0;

    std::string answers;

    std::string status = "Submitted";

    bool removed = false;
};


struct Session
{
    std::string token;

    int userId = 0;

    UserRole role = UserRole::TEEN;
};


std::vector<User> users;
std::vector<Job> jobs;
std::vector<Application> applications;
std::vector<Session> sessions;


// ============================================================
// GENERAL HELPERS
// ============================================================

std::string htmlEscape(const std::string& value)
{
    std::string result;

    for (char c : value)
    {
        switch (c)
        {
            case '&':
                result += "&amp;";
                break;

            case '<':
                result += "&lt;";
                break;

            case '>':
                result += "&gt;";
                break;

            case '"':
                result += "&quot;";
                break;

            case '\'':
                result += "&#39;";
                break;

            default:
                result += c;
                break;
        }
    }

    return result;
}


std::string urlDecode(const std::string& value)
{
    std::string result;

    for (size_t i = 0; i < value.size(); ++i)
    {
        if (
            value[i] == '%' &&
            i + 2 < value.size()
        )
        {
            try
            {
                std::string hex =
                    value.substr(i + 1, 2);

                char decoded =
                    static_cast<char>(
                        std::stoi(
                            hex,
                            nullptr,
                            16
                        )
                    );

                result += decoded;

                i += 2;
            }
            catch (...)
            {
                result += value[i];
            }
        }
        else if (value[i] == '+')
        {
            result += ' ';
        }
        else
        {
            result += value[i];
        }
    }

    return result;
}


std::string toLower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(
                std::tolower(c)
            );
        }
    );

    return value;
}


bool containsIgnoreCase(
    const std::string& text,
    const std::string& search
)
{
    if (search.empty())
    {
        return true;
    }

    return toLower(text).find(
        toLower(search)
    ) != std::string::npos;
}


std::vector<std::string> split(
    const std::string& value,
    char delimiter
)
{
    std::vector<std::string> result;

    std::stringstream stream(value);

    std::string part;

    while (std::getline(stream, part, delimiter))
    {
        result.push_back(part);
    }

    return result;
}


long long now()
{
    return static_cast<long long>(
        std::time(nullptr)
    );
}


std::string roleToString(UserRole role)
{
    switch (role)
    {
        case UserRole::ADMIN:
            return "ADMIN";

        case UserRole::BUSINESS:
            return "BUSINESS";

        default:
            return "TEEN";
    }
}


// ============================================================
// IDS
// ============================================================

int nextUserId()
{
    int id = 1;

    for (const User& user : users)
    {
        id = std::max(id, user.id + 1);
    }

    return id;
}


int nextJobId()
{
    int id = 1;

    for (const Job& job : jobs)
    {
        id = std::max(id, job.id + 1);
    }

    return id;
}


int nextQuestionId()
{
    int id = 1;

    for (const Job& job : jobs)
    {
        for (const Question& question : job.questions)
        {
            id = std::max(id, question.id + 1);
        }
    }

    return id;
}


int nextApplicationId()
{
    int id = 1;

    for (const Application& application : applications)
    {
        id = std::max(
            id,
            application.id + 1
        );
    }

    return id;
}


// ============================================================
// USER LOOKUPS
// ============================================================

User* findUser(int id)
{
    for (User& user : users)
    {
        if (user.id == id)
        {
            return &user;
        }
    }

    return nullptr;
}


const User* findUserConst(int id)
{
    for (const User& user : users)
    {
        if (user.id == id)
        {
            return &user;
        }
    }

    return nullptr;
}


Job* findJob(int id)
{
    for (Job& job : jobs)
    {
        if (job.id == id)
        {
            return &job;
        }
    }

    return nullptr;
}


Application* findApplication(int id)
{
    for (Application& application : applications)
    {
        if (application.id == id)
        {
            return &application;
        }
    }

    return nullptr;
}


// ============================================================
// SIMPLE FILE STORAGE
// ============================================================
//
// This is intentionally simple for the no-database version.
//
// NOTE:
// This is a prototype storage system, NOT production-grade
// storage. Passwords are also stored in plaintext here.
//
// Before making this publicly accessible, use proper password
// hashing and a real database.
//

void saveUsers()
{
    std::ofstream file("users.txt");

    for (const User& user : users)
    {
        file
            << user.id << "|"
            << user.name << "|"
            << user.email << "|"
            << user.password << "|"
            << user.age << "|"
            << roleToString(user.role) << "|"
            << (user.removed ? 1 : 0)
            << "\n";
    }
}


void loadUsers()
{
    std::ifstream file("users.txt");

    if (!file)
    {
        return;
    }

    std::string line;

    while (std::getline(file, line))
    {
        std::vector<std::string> p =
            split(line, '|');

        if (p.size() != 7)
        {
            continue;
        }

        try
        {
            User user;

            user.id =
                std::stoi(p[0]);

            user.name =
                p[1];

            user.email =
                p[2];

            user.password =
                p[3];

            user.age =
                std::stoi(p[4]);

            if (p[5] == "BUSINESS")
            {
                user.role =
                    UserRole::BUSINESS;
            }
            else
            {
                user.role =
                    UserRole::TEEN;
            }

            user.removed =
                p[6] == "1";

            users.push_back(user);
        }
        catch (...)
        {
        }
    }
}


void saveJobs()
{
    std::ofstream file("jobs.txt");

    for (const Job& job : jobs)
    {
        file
            << job.id << "|"
            << job.businessId << "|"
            << job.title << "|"
            << job.company << "|"
            << job.location << "|"
            << job.description << "|"
            << job.minAge << "|"
            << job.maxAge << "|"
            << job.jobType << "|"
            << job.schedule << "|"
            << job.pay << "|"
            << (job.applicationsOpen ? 1 : 0) << "|"
            << (job.removed ? 1 : 0) << "|"
            << job.publishAt << "|"
            << job.expireAt << "|";

        // Questions
        for (size_t i = 0;
             i < job.questions.size();
             ++i)
        {
            if (i > 0)
            {
                file << "~";
            }

            file
                << job.questions[i].id
                << ":"
                << job.questions[i].text;
        }

        file << "\n";
    }
}


void loadJobs()
{
    std::ifstream file("jobs.txt");

    if (!file)
    {
        return;
    }

    std::string line;

    while (std::getline(file, line))
    {
        std::vector<std::string> p =
            split(line, '|');

        if (p.size() < 15)
        {
            continue;
        }

        try
        {
            Job job;

            job.id =
                std::stoi(p[0]);

            job.businessId =
                std::stoi(p[1]);

            job.title = p[2];
            job.company = p[3];
            job.location = p[4];
            job.description = p[5];

            job.minAge =
                std::stoi(p[6]);

            job.maxAge =
                std::stoi(p[7]);

            job.jobType = p[8];
            job.schedule = p[9];
            job.pay = p[10];

            job.applicationsOpen =
                p[11] == "1";

            job.removed =
                p[12] == "1";

            job.publishAt =
                std::stoll(p[13]);

            job.expireAt =
                std::stoll(p[14]);

            if (p.size() >= 16 &&
                !p[15].empty())
            {
                std::vector<std::string> questionParts =
                    split(p[15], '~');

                for (
                    const std::string& q :
                    questionParts
                )
                {
                    size_t colon =
                        q.find(':');

                    if (
                        colon ==
                        std::string::npos
                    )
                    {
                        continue;
                    }

                    Question question;

                    question.id =
                        std::stoi(
                            q.substr(
                                0,
                                colon
                            )
                        );

                    question.text =
                        q.substr(
                            colon + 1
                        );

                    job.questions.push_back(
                        question
                    );
                }
            }

            jobs.push_back(job);
        }
        catch (...)
        {
        }
    }
}


void saveApplications()
{
    std::ofstream file(
        "applications.txt"
    );

    for (
        const Application& application :
        applications
    )
    {
        file
            << application.id << "|"
            << application.jobId << "|"
            << application.teenId << "|"
            << application.answers << "|"
            << application.status << "|"
            << (application.removed ? 1 : 0)
            << "\n";
    }
}


void loadApplications()
{
    std::ifstream file(
        "applications.txt"
    );

    if (!file)
    {
        return;
    }

    std::string line;

    while (std::getline(file, line))
    {
        std::vector<std::string> p =
            split(line, '|');

        if (p.size() != 6)
        {
            continue;
        }

        try
        {
            Application application;

            application.id =
                std::stoi(p[0]);

            application.jobId =
                std::stoi(p[1]);

            application.teenId =
                std::stoi(p[2]);

            application.answers =
                p[3];

            application.status =
                p[4];

            application.removed =
                p[5] == "1";

            applications.push_back(
                application
            );
        }
        catch (...)
        {
        }
    }
}


// ============================================================
// ADMIN RECOGNITION
// ============================================================

bool isConfiguredAdmin(
    const std::string& email,
    const std::string& password
)
{
    return
        email == ADMIN_EMAIL &&
        password == ADMIN_PASSWORD;
}


// ============================================================
// SESSIONS
// ============================================================

std::string generateToken()
{
    static std::mt19937_64 generator(
        std::random_device{}()
    );

    std::stringstream ss;

    ss << std::hex
       << generator()
       << generator();

    return ss.str();
}


std::string createSession(
    int userId,
    UserRole role
)
{
    Session session;

    session.token =
        generateToken();

    session.userId =
        userId;

    session.role =
        role;

    sessions.push_back(session);

    return session.token;
}


Session* getSession(
    const std::string& token
)
{
    for (Session& session : sessions)
    {
        if (session.token == token)
        {
            return &session;
        }
    }

    return nullptr;
}


void deleteSession(
    const std::string& token
)
{
    sessions.erase(
        std::remove_if(
            sessions.begin(),
            sessions.end(),
            [&](const Session& session)
            {
                return session.token == token;
            }
        ),
        sessions.end()
    );
}


// ============================================================
// HTTP
// ============================================================

struct HttpRequest
{
    std::string method;
    std::string path;

    std::map<
        std::string,
        std::string
    > query;

    std::map<
        std::string,
        std::string
    > form;

    std::map<
        std::string,
        std::string
    > cookies;

    std::string body;
};


std::map<
    std::string,
    std::string
> parseParameters(
    const std::string& value
)
{
    std::map<
        std::string,
        std::string
    > result;

    std::vector<std::string> parts =
        split(value, '&');

    for (
        const std::string& part :
        parts
    )
    {
        size_t equals =
            part.find('=');

        if (
            equals ==
            std::string::npos
        )
        {
            continue;
        }

        std::string key =
            urlDecode(
                part.substr(
                    0,
                    equals
                )
            );

        std::string val =
            urlDecode(
                part.substr(
                    equals + 1
                )
            );

        result[key] = val;
    }

    return result;
}


HttpRequest parseRequest(
    const std::string& raw
)
{
    HttpRequest request;

    size_t firstLineEnd =
        raw.find("\r\n");

    if (
        firstLineEnd ==
        std::string::npos
    )
    {
        return request;
    }

    std::string firstLine =
        raw.substr(
            0,
            firstLineEnd
        );

    std::stringstream first(
        firstLine
    );

    std::string target;

    first
        >> request.method
        >> target;

    size_t question =
        target.find('?');

    if (
        question ==
        std::string::npos
    )
    {
        request.path =
            target;
    }
    else
    {
        request.path =
            target.substr(
                0,
                question
            );

        request.query =
            parseParameters(
                target.substr(
                    question + 1
                )
            );
    }

    size_t headerEnd =
        raw.find("\r\n\r\n");

    if (
        headerEnd ==
        std::string::npos
    )
    {
        return request;
    }

    std::string headers =
        raw.substr(
            firstLineEnd + 2,
            headerEnd -
                firstLineEnd -
                2
        );

    std::stringstream hs(headers);

    std::string header;

    while (std::getline(hs, header))
    {
        if (
            !header.empty() &&
            header.back() == '\r'
        )
        {
            header.pop_back();
        }

        size_t colon =
            header.find(':');

        if (
            colon ==
            std::string::npos
        )
        {
            continue;
        }

        std::string name =
            toLower(
                header.substr(
                    0,
                    colon
                )
            );

        std::string value =
            header.substr(
                colon + 1
            );

        while (
            !value.empty() &&
            std::isspace(
                static_cast<unsigned char>(
                    value.front()
                )
            )
        )
        {
            value.erase(
                value.begin()
            );
        }

        if (name == "cookie")
        {
            std::vector<std::string> cookies =
                split(value, ';');

            for (
                const std::string& cookie :
                cookies
            )
            {
                size_t equals =
                    cookie.find('=');

                if (
                    equals ==
                    std::string::npos
                )
                {
                    continue;
                }

                std::string key =
                    cookie.substr(
                        0,
                        equals
                    );

                while (
                    !key.empty() &&
                    std::isspace(
                        static_cast<unsigned char>(
                            key.front()
                        )
                    )
                )
                {
                    key.erase(
                        key.begin()
                    );
                }

                std::string val =
                    cookie.substr(
                        equals + 1
                    );

                request.cookies[key] =
                    val;
            }
        }
    }

    request.body =
        raw.substr(
            headerEnd + 4
        );

    if (
        request.method ==
        "POST"
    )
    {
        request.form =
            parseParameters(
                request.body
            );
    }

    return request;
}


// ============================================================
// HTTP RESPONSES
// ============================================================

void sendRaw(
    SOCKET client,
    const std::string& response
)
{
    send(
        client,
        response.c_str(),
        static_cast<int>(
            response.size()
        ),
        0
    );
}


void sendHTML(
    SOCKET client,
    const std::string& body,
    const std::string& extraHeaders = ""
)
{
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " +
        std::to_string(
            body.size()
        ) +
        "\r\n" +
        extraHeaders +
        "Connection: close\r\n"
        "\r\n" +
        body;

    sendRaw(
        client,
        response
    );
}


void redirect(
    SOCKET client,
    const std::string& location,
    const std::string& cookie = ""
)
{
    std::string headers;

    headers +=
        "Location: " +
        location +
        "\r\n";

    if (!cookie.empty())
    {
        headers +=
            "Set-Cookie: " +
            cookie +
            "\r\n";
    }

    std::string response =
        "HTTP/1.1 303 See Other\r\n" +
        headers +
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";

    sendRaw(
        client,
        response
    );
}


void send404(SOCKET client)
{
    sendHTML(
        client,
        "<h1>404</h1><p>Page not found.</p>"
    );
}


void send400(
    SOCKET client,
    const std::string& message
)
{
    std::string body =
        "<!DOCTYPE html>"
        "<html><body>"
        "<h1>Bad Request</h1><p>" +
        htmlEscape(message) +
        "</p>"
        "<a href='/'>Return home</a>"
        "</body></html>";

    std::string response =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " +
        std::to_string(body.size()) +
        "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        body;

    sendRaw(
        client,
        response
    );
}


// ============================================================
// LAYOUT
// ============================================================

std::string page(
    const std::string& title,
    const std::string& content
)
{
    return
        R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>

<meta charset="UTF-8">

<meta
    name="viewport"
    content="width=device-width, initial-scale=1.0"
>

<title>)HTML"
        +
        htmlEscape(title)
        +
        R"HTML(</title>

<style>

* {
    box-sizing: border-box;
}

body {
    margin: 0;
    font-family:
        Arial,
        Helvetica,
        sans-serif;
    background: #f5f7fb;
    color: #182033;
}

header {
    background: white;
    border-bottom:
        1px solid #e3e6ed;
}

nav {
    max-width: 1150px;
    margin: auto;
    padding: 17px 22px;
    display: flex;
    justify-content:
        space-between;
    align-items: center;
    gap: 20px;
}

.logo {
    color: #5559e8;
    font-size: 25px;
    font-weight: 900;
}

nav a {
    color: #33394a;
    text-decoration: none;
    margin-left: 15px;
    font-weight: 700;
}

.container {
    max-width: 1150px;
    margin: auto;
    padding: 35px 22px;
}

.hero {
    background:
        linear-gradient(
            135deg,
            #5559e8,
            #777af4
        );
    color: white;
    padding: 65px 22px;
}

.hero-inner {
    max-width: 1150px;
    margin: auto;
}

.hero h1 {
    font-size:
        clamp(40px, 7vw, 70px);
    line-height: 1;
    max-width: 760px;
    margin: 0 0 20px;
}

.hero p {
    max-width: 680px;
    line-height: 1.6;
    font-size: 18px;
}

.card {
    background: white;
    border:
        1px solid #e1e5ee;
    border-radius: 17px;
    padding: 25px;
    margin-bottom: 20px;
}

.grid {
    display: grid;
    grid-template-columns:
        repeat(
            auto-fit,
            minmax(270px, 1fr)
        );
    gap: 18px;
}

input,
textarea,
select {
    width: 100%;
    padding: 12px;
    border:
        1px solid #d5dae5;
    border-radius: 9px;
    font-size: 15px;
    font-family: inherit;
}

textarea {
    min-height: 120px;
    resize: vertical;
}

label {
    display: block;
    font-weight: 700;
    font-size: 13px;
    margin-bottom: 6px;
}

.form-group {
    margin-bottom: 14px;
}

button,
.button {
    display: inline-block;
    border: 0;
    border-radius: 9px;
    padding: 12px 17px;
    background: #5559e8;
    color: white;
    text-decoration: none;
    font-weight: 800;
    cursor: pointer;
}

.button.red,
button.red {
    background: #dc3545;
}

.button.green,
button.green {
    background: #198754;
}

.button.gray,
button.gray {
    background: #697386;
}

.job {
    background: white;
    border:
        1px solid #e1e5ee;
    border-radius: 16px;
    padding: 22px;
}

.job h3 {
    margin-top: 0;
}

.company {
    color: #5559e8;
    font-weight: 800;
}

.description {
    color: #626b7d;
    line-height: 1.55;
}

.tags {
    display: flex;
    flex-wrap: wrap;
    gap: 7px;
    margin: 14px 0;
}

.tag {
    background: #eef0ff;
    color: #4d51ce;
    padding: 6px 9px;
    border-radius: 7px;
    font-size: 12px;
    font-weight: 700;
}

.alert {
    background: #fff4d6;
    border:
        1px solid #f2d98b;
    border-radius: 10px;
    padding: 13px;
    margin-bottom: 15px;
}

.success {
    background: #e8f7ee;
    border:
        1px solid #b8e1c7;
    padding: 13px;
    border-radius: 10px;
}

.danger {
    background: #fde9ec;
    border:
        1px solid #f2b8c0;
    padding: 13px;
    border-radius: 10px;
}

table {
    width: 100%;
    border-collapse: collapse;
}

th,
td {
    text-align: left;
    padding: 12px 8px;
    border-bottom:
        1px solid #e5e8ee;
    vertical-align: top;
}

.stat {
    background: #5559e8;
    color: white;
    border-radius: 14px;
    padding: 20px;
}

.stat strong {
    display: block;
    font-size: 32px;
}

footer {
    background: #182033;
    color: #cdd2dc;
    text-align: center;
    padding: 30px;
    margin-top: 50px;
}

.small {
    color: #6a7282;
    font-size: 13px;
}

.nav-dropdown {
    position: relative;
    display: inline-block;
    margin-left: 15px;
}

.nav-dropdown summary {
    list-style: none;
    cursor: pointer;
    color: #33394a;
    font-weight: 700;
}

.nav-dropdown summary::-webkit-details-marker {
    display: none;
}

.nav-dropdown summary::after {
    content: " ▾";
    font-size: 11px;
}

.nav-menu {
    position: absolute;
    right: 0;
    top: calc(100% + 8px);
    min-width: 190px;
    background: white;
    border: 1px solid #e1e5ee;
    border-radius: 12px;
    box-shadow: 0 12px 30px rgba(24, 32, 51, 0.14);
    padding: 7px;
    z-index: 1000;
}

.nav-menu a {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin: 0;
    padding: 10px 11px;
    border-radius: 8px;
    white-space: nowrap;
}

.nav-menu a:hover {
    background: #f5f7fb;
}

.application-badge {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    min-width: 22px;
    height: 22px;
    padding: 0 5px;
    border-radius: 50%;
    background: #697386;
    color: white;
    font-size: 11px;
    font-weight: 900;
    margin-left: 10px;
}

.application-badge.wide {
    border-radius: 11px;
}

.modal-backdrop {
    position: fixed;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    background: rgba(24, 32, 51, 0.48);
    z-index: 2000;
    padding: 20px;
}

.modal-embed {
    width: min(430px, 100%);
    background: white;
    border: 1px solid #e1e5ee;
    border-radius: 17px;
    box-shadow: 0 20px 60px rgba(24, 32, 51, 0.25);
    padding: 25px;
}

.modal-embed h2 {
    margin-top: 0;
}

.modal-actions {
    display: flex;
    gap: 10px;
    justify-content: flex-end;
    margin-top: 20px;
}

.modal-actions form {
    margin: 0;
}

.question {
    background: #f5f7fb;
    padding: 15px;
    border-radius: 10px;
    margin-bottom: 10px;
}

@media(max-width: 700px) {

    nav {
        flex-direction: column;
    }

    nav a {
        margin: 5px;
    }

    table {
        font-size: 13px;
    }

}

</style>

</head>

<body>

<header>

<nav>

<div class="logo">
TeenJobs
</div>

<div>

<a href="/">
Home
</a>

<details class="nav-dropdown">
<summary>Teen</summary>
<div class="nav-menu">
<a href="/teen-login">Log In</a>
<a href="/teen-signup">Sign Up</a>
<a href="/teen-dashboard">Current Applications</a>
</div>
</details>

<details class="nav-dropdown">
<summary>Business</summary>
<div class="nav-menu">
<a href="/business-login">Log In</a>
<a href="/business-signup">Sign Up</a>
<a href="/business-applications">
Open Applications
<span id="business-application-badge" class="application-badge">0</span>
</a>
</div>
</details>

</div>

</nav>

</header>

)HTML"
        +
        content
        +
        R"HTML(

<footer>
TeenJobs — Opportunities built for young workers.
</footer>

<script>
fetch('/business-application-count')
    .then(function(response) {
        if (!response.ok) return null;
        return response.text();
    })
    .then(function(value) {
        if (value === null) return;
        var badge = document.getElementById('business-application-badge');
        if (!badge) return;
        var count = parseInt(value, 10);
        if (isNaN(count) || count <= 0) {
            badge.textContent = '0';
            return;
        }
        if (count >= 9) {
            badge.textContent = '9+';
            badge.classList.add('wide');
        } else {
            badge.textContent = String(count);
        }
    })
    .catch(function() {});
</script>

</body>
</html>
)HTML";
}


// ============================================================
// AUTHORIZATION
// ============================================================

Session* currentSession(
    const HttpRequest& request
)
{
    auto it =
        request.cookies.find(
            "session"
        );

    if (
        it ==
        request.cookies.end()
    )
    {
        return nullptr;
    }

    return getSession(
        it->second
    );
}


User* currentUser(
    const HttpRequest& request
)
{
    Session* session =
        currentSession(request);

    if (!session)
    {
        return nullptr;
    }

    return findUser(
        session->userId
    );
}


bool requireLogin(
    SOCKET client,
    const HttpRequest& request
)
{
    if (!currentSession(request))
    {
        redirect(
            client,
            "/business-login"
        );

        return false;
    }

    return true;
}


bool requireTeen(
    SOCKET client,
    const HttpRequest& request
)
{
    User* user =
        currentUser(request);

    if (
        !user ||
        user->role != UserRole::TEEN
    )
    {
        redirect(
            client,
            "/teen-login"
        );

        return false;
    }

    return true;
}


bool requireBusiness(
    SOCKET client,
    const HttpRequest& request
)
{
    User* user =
        currentUser(request);

    if (
        !user ||
        (
            user->role !=
                UserRole::BUSINESS &&
            user->role !=
                UserRole::ADMIN
        )
    )
    {
        redirect(
            client,
            "/business-login"
        );

        return false;
    }

    return true;
}


bool requireBusinessAccount(
    SOCKET client,
    const HttpRequest& request
)
{
    User* user =
        currentUser(request);

    if (
        !user ||
        user->role != UserRole::BUSINESS
    )
    {
        redirect(
            client,
            "/business-login"
        );

        return false;
    }

    return true;
}


bool requireAdmin(
    SOCKET client,
    const HttpRequest& request
)
{
    User* user =
        currentUser(request);

    if (
        !user ||
        user->role != UserRole::ADMIN
    )
    {
        redirect(
            client,
            "/business-login"
        );

        return false;
    }

    return true;
}


// ============================================================
// HOME PAGE
// ============================================================

std::string homePage(
    const HttpRequest& request
)
{
    User* user =
        currentUser(request);

    std::ostringstream html;

    html << R"HTML(

<section class="hero">

<div class="hero-inner">

<h1>
Find work that fits your age.
</h1>

<p>
TeenJobs helps young people find
opportunities that match their age,
schedule and location.
</p>

</div>

</section>

<div class="container">

<div class="card">

<h2>
Find a Job
</h2>

<form action="/search" method="GET">

<div class="grid">

<div class="form-group">

<label>Age</label>

<input
    type="number"
    name="age"
    min="13"
    max="18"
    placeholder="16"
    required
>

</div>

<div class="form-group">

<label>Job</label>

<input
    name="search"
    placeholder="Cashier, restaurant..."
>

</div>

<div class="form-group">

<label>Location</label>

<input
    name="location"
    placeholder="Hyannis"
>

</div>

<div
    class="form-group"
    style="align-self:end"
>

<button type="submit">
Search Jobs
</button>

</div>

</div>

</form>

</div>

)HTML";

    if (user)
    {
        html
            << R"HTML(

<div class="success">

<strong>
Welcome,
)HTML"
            << htmlEscape(user->name)
            << R"HTML(!
</strong>

<br><br>

)HTML";

        if (
            user->role ==
            UserRole::TEEN
        )
        {
            html
                << R"HTML(
<a class="button" href="/teen-dashboard">
Teen Dashboard
</a>
)HTML";
        }
        else if (
            user->role ==
            UserRole::ADMIN
        )
        {
            html
                << R"HTML(
<a class="button" href="/admin">
Admin Dashboard
</a>
)HTML";
        }
        else
        {
            html
                << R"HTML(
<a class="button" href="/business-dashboard">
Business Dashboard
</a>
)HTML";
        }

        html
            << R"HTML(
&nbsp;

<a class="button gray" href="/logout">
Log Out
</a>

</div>

)HTML";
    }
    else
    {
        html
            << R"HTML(

<div class="grid">

<div class="card">

<h2>
I'm a Teen
</h2>

<p>
Create an account and find
age-compatible opportunities.
</p>

<a
    class="button"
    href="/teen-signup"
>
Create Teen Account
</a>

<a
    class="button gray"
    href="/teen-login"
>
Log In
</a>

</div>

<div class="card">

<h2>
I'm a Business
</h2>

<p>
Create an employer account and
post opportunities for young workers.
</p>

<a
    class="button"
    href="/business-signup"
>
Create Business Account
</a>

<a
    class="button gray"
    href="/business-login"
>
Log In
</a>

</div>

</div>

)HTML";
    }

    html
        << R"HTML(

<h2>
Featured Opportunities
</h2>

<div class="grid">

)HTML";

    int shown = 0;

    for (const Job& job : jobs)
    {
        if (job.removed)
        {
            continue;
        }

        long long current =
            now();

        if (
            job.publishAt > 0 &&
            current < job.publishAt
        )
        {
            continue;
        }

        if (
            job.expireAt > 0 &&
            current >= job.expireAt
        )
        {
            continue;
        }

        html
            << R"HTML(

<div class="job">

<h3>)HTML"
            << htmlEscape(job.title)
            << R"HTML(</h3>

<div class="company">
)HTML"
            << htmlEscape(job.company)
            << R"HTML(
</div>

<p class="description">
)HTML"
            << htmlEscape(job.description)
            << R"HTML(
</p>

<div class="tags">

<span class="tag">
)HTML"
            << htmlEscape(job.location)
            << R"HTML(
</span>

<span class="tag">
Ages )HTML"
            << job.minAge
            << "–"
            << job.maxAge
            << R"HTML(
</span>

<span class="tag">
)HTML"
            << htmlEscape(job.jobType)
            << R"HTML(
</span>

<span class="tag">
)HTML"
            << htmlEscape(job.schedule)
            << R"HTML(
</span>

</div>

<strong>
)HTML"
            << htmlEscape(job.pay)
            << R"HTML(
</strong>

<br><br>

<a
    class="button"
    href="/job?id=)HTML"
            << job.id
            << R"HTML("
>
View Job
</a>

</div>

)HTML";

        shown++;

        if (shown >= 6)
        {
            break;
        }
    }

    if (shown == 0)
    {
        html
            << R"HTML(

<div class="card">
No jobs are currently available.
</div>

)HTML";
    }

    html
        << R"HTML(

</div>

</div>

)HTML";

    return page(
        "TeenJobs",
        html.str()
    );
}


// ============================================================
// AUTH FORMS
// ============================================================

std::string teenSignupPage(
    const std::string& error = ""
)
{
    std::ostringstream html;

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>
Create Teen Account
</h1>

)HTML";

    if (!error.empty())
    {
        html
            << "<div class='danger'>"
            << htmlEscape(error)
            << "</div>";
    }

    html
        << R"HTML(

<form
    method="POST"
    action="/teen-signup"
>

<div class="form-group">

<label>Name</label>

<input
    name="name"
    required
>

</div>

<div class="form-group">

<label>Email</label>

<input
    name="email"
    type="email"
    required
>

</div>

<div class="form-group">

<label>Age</label>

<input
    name="age"
    type="number"
    min="13"
    max="18"
    required
>

</div>

<div class="form-group">

<label>Password</label>

<input
    name="password"
    type="password"
    required
>

</div>

<button>
Create Account
</button>

</form>

<br>

<a href="/teen-login">
Already have an account?
Log in
</a>

</div>

</div>

)HTML";

    return page(
        "Teen Signup",
        html.str()
    );
}


std::string teenLoginPage(
    const std::string& error = ""
)
{
    std::ostringstream html;

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>
Teen Login
</h1>

)HTML";

    if (!error.empty())
    {
        html
            << "<div class='danger'>"
            << htmlEscape(error)
            << "</div>";
    }

    html
        << R"HTML(

<form
    method="POST"
    action="/teen-login"
>

<div class="form-group">

<label>Email</label>

<input
    name="email"
    type="email"
    required
>

</div>

<div class="form-group">

<label>Password</label>

<input
    name="password"
    type="password"
    required
>

</div>

<button>
Log In
</button>

</form>

<br>

<a href="/teen-signup">
Create an account
</a>

</div>

</div>

)HTML";

    return page(
        "Teen Login",
        html.str()
    );
}


std::string businessSignupPage(
    const std::string& error = ""
)
{
    std::ostringstream html;

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>
Business Signup
</h1>

<p class="small">
Create an employer account to post jobs.
</p>

)HTML";

    if (!error.empty())
    {
        html
            << "<div class='danger'>"
            << htmlEscape(error)
            << "</div>";
    }

    html
        << R"HTML(

<form
    method="POST"
    action="/business-signup"
>

<div class="form-group">

<label>Business / Contact Name</label>

<input
    name="name"
    required
>

</div>

<div class="form-group">

<label>Email</label>

<input
    name="email"
    type="email"
    required
>

</div>

<div class="form-group">

<label>Password</label>

<input
    name="password"
    type="password"
    required
>

</div>

<button>
Create Business Account
</button>

</form>

<br>

<a href="/business-login">
Already have an account?
Log in
</a>

</div>

</div>

)HTML";

    return page(
        "Business Signup",
        html.str()
    );
}


std::string businessLoginPage(
    const std::string& error = ""
)
{
    std::ostringstream html;

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>
Business Login
</h1>

<p class="small">
Business accounts sign in here. Site administration also uses the Business login.
</p>

)HTML";

    if (!error.empty())
    {
        html
            << "<div class='danger'>"
            << htmlEscape(error)
            << "</div>";
    }

    html
        << R"HTML(

<form
    method="POST"
    action="/business-login"
>

<div class="form-group">

<label>Email</label>

<input
    name="email"
    type="email"
    required
>

</div>

<div class="form-group">

<label>Password</label>

<input
    name="password"
    type="password"
    required
>

</div>

<button>
Log In
</button>

</form>

<br>

<a href="/business-signup">
Create Business Account
</a>

</div>

</div>

)HTML";

    return page(
        "Business Login",
        html.str()
    );
}


// ============================================================
// JOB PAGE
// ============================================================

std::string jobPage(
    int jobId,
    const HttpRequest& request
)
{
    Job* job =
        findJob(jobId);

    if (!job || job->removed)
    {
        return page(
            "Job Not Found",
            "<div class='container'>"
            "<div class='card'>"
            "<h1>Job not found</h1>"
            "<a href='/'>Go home</a>"
            "</div></div>"
        );
    }

    std::ostringstream html;

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>)HTML"
        << htmlEscape(job->title)
        << R"HTML(</h1>

<div class="company">
)HTML"
        << htmlEscape(job->company)
        << R"HTML(
</div>

<div class="tags">

<span class="tag">
)HTML"
        << htmlEscape(job->location)
        << R"HTML(
</span>

<span class="tag">
Ages )HTML"
        << job->minAge
        << "–"
        << job->maxAge
        << R"HTML(
</span>

<span class="tag">
)HTML"
        << htmlEscape(job->jobType)
        << R"HTML(
</span>

<span class="tag">
)HTML"
        << htmlEscape(job->schedule)
        << R"HTML(
</span>

<span class="tag">
)HTML"
        << htmlEscape(job->pay)
        << R"HTML(
</span>

</div>

<h2>
About the Job
</h2>

<p>
)HTML"
        << htmlEscape(job->description)
        << R"HTML(
</p>

)HTML";

    if (!job->applicationsOpen)
    {
        html
            << R"HTML(

<div class="alert">

<strong>
Applications are currently closed.
</strong>

<p>
This job can still be viewed, but
new applications are not being accepted.
</p>

</div>

)HTML";
    }
    else
    {
        User* user =
            currentUser(request);

        if (
            user &&
            user->role ==
                UserRole::TEEN
        )
        {
            if (
                user->age >=
                    job->minAge &&
                user->age <=
                    job->maxAge
            )
            {
                html
                    << R"HTML(

<h2>
Apply
</h2>

<form
    method="POST"
    action="/apply"
>

<input
    type="hidden"
    name="jobId"
    value=")HTML"
                    << job->id
                    << R"HTML("
>

)HTML";

                for (
                    const Question& question :
                    job->questions
                )
                {
                    html
                        << R"HTML(

<div class="question">

<label>
)HTML"
                        << htmlEscape(
                            question.text
                        )
                        << R"HTML(
</label>

<textarea
    name="answer_)HTML"
                        << question.id
                        << R"HTML("
    required
></textarea>

</div>

)HTML";
                }

                html
                    << R"HTML(

<button>
Submit Application
</button>

</form>

)HTML";
            }
            else
            {
                html
                    << R"HTML(

<div class="danger">

This job is not available for
your age.

</div>

)HTML";
            }
        }
        else
        {
            html
                << R"HTML(

<div class="alert">

<a href="/teen-login">
Log in as a teen
</a>
to apply for this job.

</div>

)HTML";
        }
    }

    html
        << R"HTML(

</div>

</div>

)HTML";

    return page(
        job->title,
        html.str()
    );
}


// ============================================================
// TEEN DASHBOARD
// ============================================================

std::string teenDashboard(
    const HttpRequest& request
)
{
    User* user =
        currentUser(request);

    std::ostringstream html;

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>
Teen Dashboard
</h1>

<p>
Welcome,
<strong>)HTML"
        << htmlEscape(user->name)
        << R"HTML(</strong>.
</p>

<p>
Age:
<strong>)HTML"
        << user->age
        << R"HTML(</strong>
</p>

<a class="button" href="/">
Find Jobs
</a>

<a
    class="button gray"
    href="/logout"
>
Log Out
</a>

</div>

<h2>
My Applications
</h2>

)HTML";

    bool found = false;

    for (
        const Application& application :
        applications
    )
    {
        if (
            application.teenId !=
                user->id ||
            application.removed
        )
        {
            continue;
        }

        Job* job =
            findJob(
                application.jobId
            );

        if (!job)
        {
            continue;
        }

        found = true;

        html
            << R"HTML(

<div class="card">

<h3>
)HTML"
            << htmlEscape(job->title)
            << R"HTML(
</h3>

<p>
)HTML"
            << htmlEscape(job->company)
            << R"HTML(
</p>

<strong>
Status:
)HTML"
            << htmlEscape(
                application.status
            )
            << R"HTML(
</strong>

</div>

)HTML";
    }

    if (!found)
    {
        html
            << R"HTML(

<div class="card">
You haven't applied to any jobs yet.
</div>

)HTML";
    }

    html
        << R"HTML(

</div>

)HTML";

    return page(
        "Teen Dashboard",
        html.str()
    );
}


// ============================================================
// BUSINESS DASHBOARD
// ============================================================

std::string businessDashboard(
    const HttpRequest& request
)
{
    User* user =
        currentUser(request);

    std::ostringstream html;

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>
Business Dashboard
</h1>

<p>
Welcome,
<strong>)HTML"
        << htmlEscape(user->name)
        << R"HTML(</strong>.
</p>

<a
    class="button"
    href="/post-job"
>
Post a New Job
</a>

<a
    class="button gray"
    href="/logout"
>
Log Out
</a>

</div>

<h2>
Your Jobs
</h2>

<div class="grid">

)HTML";

    bool found = false;

    for (const Job& job : jobs)
    {
        if (
            job.businessId !=
                user->id ||
            job.removed
        )
        {
            continue;
        }

        found = true;

        std::string state =
            "Published";

        if (
            job.publishAt > now()
        )
        {
            state =
                "Scheduled";
        }
        else if (
            job.expireAt > 0 &&
            job.expireAt <= now()
        )
        {
            state =
                "Expired";
        }
        else if (!job.applicationsOpen)
        {
            state =
                "Applications Closed";
        }

        html
            << R"HTML(

<div class="job">

<h3>
)HTML"
            << htmlEscape(job.title)
            << R"HTML(
</h3>

<div class="tags">

<span class="tag">
)HTML"
            << state
            << R"HTML(
</span>

<span class="tag">
Ages )HTML"
            << job.minAge
            << "–"
            << job.maxAge
            << R"HTML(
</span>

<span class="tag">
Questions: )HTML"
            << job.questions.size()
            << R"HTML(
</span>

</div>

<a
    class="button"
    href="/business-job?id=)HTML"
            << job.id
            << R"HTML("
>
Manage
</a>

</div>

)HTML";
    }

    if (!found)
    {
        html
            << R"HTML(

<div class="card">
You haven't posted any jobs.
</div>

)HTML";
    }

    html
        << R"HTML(

</div>

</div>

)HTML";

    return page(
        "Business Dashboard",
        html.str()
    );
}


// ============================================================
// POST JOB PAGE
// ============================================================

std::string postJobPage(
    const std::string& error = ""
)
{
    std::ostringstream html;

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>
Create a Job
</h1>

<p>
Every job must have at least one
application question.
</p>

)HTML";

    if (!error.empty())
    {
        html
            << "<div class='danger'>"
            << htmlEscape(error)
            << "</div>";
    }

    html
        << R"HTML(

<form
    method="POST"
    action="/post-job"
>

<div class="form-group">

<label>Job Title</label>

<input
    name="title"
    required
>

</div>

<div class="form-group">

<label>Company</label>

<input
    name="company"
    required
>

</div>

<div class="form-group">

<label>Location</label>

<input
    name="location"
    required
>

</div>

<div class="form-group">

<label>Description</label>

<textarea
    name="description"
    required
></textarea>

</div>

<div class="grid">

<div class="form-group">

<label>Minimum Age</label>

<input
    name="minAge"
    type="number"
    min="13"
    max="18"
    value="13"
    required
>

</div>

<div class="form-group">

<label>Maximum Age</label>

<input
    name="maxAge"
    type="number"
    min="13"
    max="18"
    value="18"
    required
>

</div>

</div>

<div class="grid">

<div class="form-group">

<label>Job Type</label>

<select name="jobType">

<option>Part-time</option>
<option>Seasonal</option>
<option>Temporary</option>
<option>Internship</option>

</select>

</div>

<div class="form-group">

<label>Schedule</label>

<select name="schedule">

<option>After school</option>
<option>Weekends</option>
<option>Summer</option>
<option>Flexible</option>

</select>

</div>

</div>

<div class="form-group">

<label>Pay</label>

<input
    name="pay"
    placeholder="$16/hour"
    required
>

</div>

<hr>

<h2>
Application Settings
</h2>

<div class="form-group">

<label>
Applications
</label>

<select name="applicationsOpen">

<option value="1">
ON — Accept applications
</option>

<option value="0">
OFF — Do not accept applications
</option>

</select>

</div>

<h3>
Publishing
</h3>

<p class="small">
Leave the schedule blank to publish immediately.
Use Unix timestamps for scheduled publishing
in this prototype.
</p>

<div class="grid">

<div class="form-group">

<label>
Publish timestamp
</label>

<input
    name="publishAt"
    type="number"
    placeholder="0 = immediately"
>

</div>

<div class="form-group">

<label>
Expiration timestamp
</label>

<input
    name="expireAt"
    type="number"
    placeholder="0 = never"
>

</div>

</div>

<hr>

<h2>
Application Questions
</h2>

<p>
Add at least one question.
Each question will be shown to applicants.
</p>

<div class="form-group">

<label>
Question 1
</label>

<input
    name="question1"
    placeholder="Why are you interested in this job?"
    required
>

</div>

<div class="form-group">

<label>
Question 2
</label>

<input
    name="question2"
    placeholder="What days are you available?"
>

</div>

<div class="form-group">

<label>
Question 3
</label>

<input
    name="question3"
    placeholder="Do you have reliable transportation?"
>

</div>

<div class="form-group">

<label>
Question 4
</label>

<input
    name="question4"
>

</div>

<div class="form-group">

<label>
Question 5
</label>

<input
    name="question5"
>

</div>

<button>
Create Job
</button>

</form>

</div>

</div>

)HTML";

    return page(
        "Post a Job",
        html.str()
    );
}


// ============================================================
// BUSINESS JOB MANAGEMENT
// ============================================================

std::string businessJobPage(
    int jobId,
    const HttpRequest& request
)
{
    User* user =
        currentUser(request);

    Job* job =
        findJob(jobId);

    if (
        !job ||
        job->businessId != user->id
    )
    {
        return page(
            "Not Found",
            "<div class='container'>"
            "<div class='card'>"
            "<h1>Job not found.</h1>"
            "</div></div>"
        );
    }

    std::ostringstream html;

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>
)HTML"
        << htmlEscape(job->title)
        << R"HTML(
</h1>

<div class="tags">

<span class="tag">
)HTML"
        << (
            job->applicationsOpen
            ? "Applications ON"
            : "Applications OFF"
        )
        << R"HTML(
</span>

<span class="tag">
)HTML"
        << (
            job->publishAt > now()
            ? "Scheduled"
            : "Published"
        )
        << R"HTML(
</span>

</div>

<h2>
Application Questions
</h2>

)HTML";

    for (
        const Question& question :
        job->questions
    )
    {
        html
            << "<div class='question'>"
            << htmlEscape(
                question.text
            )
            << "</div>";
    }

    html
        << R"HTML(

<hr>

<h2>
Applications
</h2>

)HTML";

    bool found = false;

    for (
        const Application& application :
        applications
    )
    {
        if (
            application.jobId !=
                job->id ||
            application.removed
        )
        {
            continue;
        }

        found = true;

        const User* teen =
            findUserConst(
                application.teenId
            );

        html
            << R"HTML(

<div class="card">

<h3>
Applicant:
)HTML"
            << (
                teen
                ? htmlEscape(
                    teen->name
                )
                : "Unknown"
            )
            << R"HTML(
</h3>

<p>
Status:
<strong>
)HTML"
            << htmlEscape(
                application.status
            )
            << R"HTML(
</strong>
</p>

<pre style="white-space:pre-wrap">)HTML"
            << htmlEscape(
                application.answers
            )
            << R"HTML(
</pre>

<form
    method="POST"
    action="/application-status"
>

<input
    type="hidden"
    name="applicationId"
    value=")HTML"
            << application.id
            << R"HTML("
>

<select name="status">

<option
    value="Submitted"
>
Submitted
</option>

<option
    value="Reviewing"
>
Reviewing
</option>

<option
    value="Interview"
>
Interview
</option>

<option
    value="Accepted"
>
Accepted
</option>

<option
    value="Rejected"
>
Rejected
</option>

</select>

<button>
Update Status
</button>

</form>

</div>

)HTML";
    }

    if (!found)
    {
        html
            << R"HTML(

<div class="card">
No applications yet.
</div>

)HTML";
    }

    html
        << R"HTML(

<br>

<a
    class="button"
    href="/business-dashboard"
>
Back
</a>

</div>

)HTML";

    return page(
        "Manage Job",
        html.str()
    );
}


// ============================================================
// ADMIN DASHBOARD
// ============================================================

std::string adminDashboard(
    const HttpRequest& request
)
{
    std::ostringstream html;

    int activeUsers = 0;
    int activeJobs = 0;
    int totalApplications = 0;

    for (const User& user : users)
    {
        if (!user.removed)
        {
            activeUsers++;
        }
    }

    for (const Job& job : jobs)
    {
        if (!job.removed)
        {
            activeJobs++;
        }
    }

    for (
        const Application& application :
        applications
    )
    {
        if (!application.removed)
        {
            totalApplications++;
        }
    }

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>
Admin Panel
</h1>

<p>
You are signed in with administrator access.
</p>

<a
    class="button gray"
    href="/logout"
>
Log Out
</a>

</div>

<div class="grid">

<div class="stat">
<strong>)HTML"
        << activeUsers
        << R"HTML(</strong>
Active Users
</div>

<div class="stat">
<strong>)HTML"
        << activeJobs
        << R"HTML(</strong>
Active Jobs
</div>

<div class="stat">
<strong>)HTML"
        << totalApplications
        << R"HTML(</strong>
Applications
</div>

</div>

<h2>
Manage Jobs
</h2>

<div class="card">

<table>

<tr>
<th>Job</th>
<th>Business</th>
<th>Status</th>
<th>Action</th>
</tr>

)HTML";

    for (const Job& job : jobs)
    {
        if (job.removed)
        {
            continue;
        }

        std::string status =
            "Published";

        if (
            job.publishAt > now()
        )
        {
            status =
                "Scheduled";
        }
        else if (
            job.expireAt > 0 &&
            job.expireAt <= now()
        )
        {
            status =
                "Expired";
        }
        else if (
            !job.applicationsOpen
        )
        {
            status =
                "Applications Closed";
        }

        html
            << "<tr>"
            << "<td>"
            << htmlEscape(job.title)
            << "</td>"
            << "<td>"
            << htmlEscape(job.company)
            << "</td>"
            << "<td>"
            << status
            << "</td>"
            << "<td>"

            << "<a class='button gray' href='/admin-job?id="
            << job.id
            << "'>Manage</a> "

            << "<button class='red' type='button' onclick=\"openRemoveJobModal("
            << job.id
            << ")\">Remove</button>"

            << "</td>"
            << "</tr>";
    }

    html
        << R"HTML(

</table>

</div>

<h2>
Manage Users
</h2>

<div class="card">

<table>

<tr>
<th>Name</th>
<th>Email</th>
<th>Role</th>
<th>Age</th>
<th>Action</th>
</tr>

)HTML";

    for (const User& user : users)
    {
        if (user.removed)
        {
            continue;
        }

        html
            << "<tr>"
            << "<td>"
            << htmlEscape(user.name)
            << "</td>"
            << "<td>"
            << htmlEscape(user.email)
            << "</td>"
            << "<td>"
            << roleToString(user.role)
            << "</td>"
            << "<td>"
            << user.age
            << "</td>"
            << "<td>";

        if (
            user.role !=
            UserRole::ADMIN
        )
        {
            html
                << "<form method='POST' action='/admin-remove-user'>"
                << "<input type='hidden' name='userId' value='"
                << user.id
                << "'>"
                << "<button class='red' onclick=\"return confirm('Remove this user?')\">Remove</button>"
                << "</form>";
        }
        else
        {
            html
                << "<span class='small'>Owner/Admin</span>";
        }

        html
            << "</td>"
            << "</tr>";
    }

    html
        << R"HTML(

</table>

</div>

<h2>
Applications
</h2>

<div class="card">

<table>

<tr>
<th>Applicant</th>
<th>Job</th>
<th>Status</th>
<th>Action</th>
</tr>

)HTML";

    for (
        const Application& application :
        applications
    )
    {
        if (application.removed)
        {
            continue;
        }

        const User* teen =
            findUserConst(
                application.teenId
            );

        Job* job =
            findJob(
                application.jobId
            );

        html
            << "<tr>"
            << "<td>"
            << (
                teen
                ? htmlEscape(
                    teen->name
                )
                : "Unknown"
            )
            << "</td>"
            << "<td>"
            << (
                job
                ? htmlEscape(
                    job->title
                )
                : "Unknown"
            )
            << "</td>"
            << "<td>"
            << htmlEscape(
                application.status
            )
            << "</td>"
            << "<td>"

            << "<form method='POST' action='/admin-remove-application'>"
            << "<input type='hidden' name='applicationId' value='"
            << application.id
            << "'>"
            << "<button class='red'>Remove</button>"
            << "</form>"

            << "</td>"
            << "</tr>";
    }

    html
        << R"HTML(

</table>

</div>

</div>

)HTML";

    html
        << R"HTML(

<div id="remove-job-modal" class="modal-backdrop" style="display:none">

<div class="modal-embed">

<h2>
Are you sure?
</h2>

<p>
This will remove the job listing from the site.
</p>

<div class="modal-actions">

<button
    class="green"
    type="button"
    onclick="closeRemoveJobModal()"
>
No
</button>

<form
    id="remove-job-form"
    method="POST"
    action="/admin-remove-job"
>
<input
    id="remove-job-id"
    type="hidden"
    name="jobId"
    value=""
>

<button
    class="red"
    type="submit"
>
Yes
</button>

</form>

</div>

</div>

</div>

<script>
function openRemoveJobModal(jobId) {
    document.getElementById('remove-job-id').value = jobId;
    document.getElementById('remove-job-modal').style.display = 'flex';
}

function closeRemoveJobModal() {
    document.getElementById('remove-job-modal').style.display = 'none';
}
</script>

)HTML";

    return page(
        "Admin Panel",
        html.str()
    );
}


// ============================================================
// ADMIN JOB PAGE
// ============================================================

std::string adminJobPage(
    int jobId
)
{
    Job* job =
        findJob(jobId);

    if (!job)
    {
        return page(
            "Not Found",
            "<div class='container'><div class='card'>"
            "Job not found."
            "</div></div>"
        );
    }

    std::ostringstream html;

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>
Admin: )HTML"
        << htmlEscape(job->title)
        << R"HTML(
</h1>

<p>
Company:
<strong>
)HTML"
        << htmlEscape(job->company)
        << R"HTML(
</strong>
</p>

<p>
Applications:
<strong>
)HTML"
        << (
            job->applicationsOpen
            ? "ON"
            : "OFF"
        )
        << R"HTML(
</strong>
</p>

<p>
Questions:
<strong>
)HTML"
        << job->questions.size()
        << R"HTML(
</strong>
</p>

<form
    method="POST"
    action="/admin-toggle-applications"
>

<input
    type="hidden"
    name="jobId"
    value=")HTML"
        << job->id
        << R"HTML("
>

<button>
Toggle Applications
</button>

</form>

<br>

<form
    method="POST"
    action="/admin-remove-job"
>

<input
    type="hidden"
    name="jobId"
    value=")HTML"
        << job->id
        << R"HTML("
>

<button
    class="red"
    type="button"
    onclick="openRemoveJobModal()"
>
Remove Job
</button>

<div id="remove-job-modal" class="modal-backdrop" style="display:none">

<div class="modal-embed">

<h2>
Are you sure?
</h2>

<p>
This will remove the job listing from the site.
</p>

<div class="modal-actions">

<button
    class="green"
    type="button"
    onclick="closeRemoveJobModal()"
>
No
</button>

<form
    method="POST"
    action="/admin-remove-job"
>
<input
    type="hidden"
    name="jobId"
    value=")HTML"
        << job->id
        << R"HTML("
>

<button
    class="red"
    type="submit"
>
Yes
</button>

</form>

</div>

</div>

</div>

<script>
function openRemoveJobModal() {
    document.getElementById('remove-job-modal').style.display = 'flex';
}

function closeRemoveJobModal() {
    document.getElementById('remove-job-modal').style.display = 'none';
}
</script>

</form>

<br>

<a
    class="button gray"
    href="/admin"
>
Back to Admin
</a>

</div>

</div>

)HTML";

    return page(
        "Admin Job",
        html.str()
    );
}


// ============================================================
// SEARCH
// ============================================================

std::string searchPage(
    int age,
    const std::string& search,
    const std::string& location
)
{
    std::ostringstream html;

    html
        << R"HTML(

<div class="container">

<div class="card">

<h1>
Search Results
</h1>

<p>
Jobs compatible with age
<strong>)HTML"
        << age
        << R"HTML(
</strong>
</p>

</div>

<div class="grid">

)HTML";

    int count = 0;

    for (const Job& job : jobs)
    {
        if (job.removed)
        {
            continue;
        }

        long long current =
            now();

        if (
            job.publishAt > 0 &&
            current < job.publishAt
        )
        {
            continue;
        }

        if (
            job.expireAt > 0 &&
            current >= job.expireAt
        )
        {
            continue;
        }

        bool ageMatch =
            age >= job.minAge &&
            age <= job.maxAge;

        bool searchMatch =
            search.empty() ||
            containsIgnoreCase(
                job.title,
                search
            ) ||
            containsIgnoreCase(
                job.company,
                search
            ) ||
            containsIgnoreCase(
                job.description,
                search
            );

        bool locationMatch =
            location.empty() ||
            containsIgnoreCase(
                job.location,
                location
            );

        if (
            !ageMatch ||
            !searchMatch ||
            !locationMatch
        )
        {
            continue;
        }

        count++;

        html
            << R"HTML(

<div class="job">

<h3>
)HTML"
            << htmlEscape(job.title)
            << R"HTML(
</h3>

<div class="company">
)HTML"
            << htmlEscape(job.company)
            << R"HTML(
</div>

<p class="description">
)HTML"
            << htmlEscape(job.description)
            << R"HTML(
</p>

<div class="tags">

<span class="tag">
)HTML"
            << htmlEscape(job.location)
            << R"HTML(
</span>

<span class="tag">
Ages )HTML"
            << job.minAge
            << "–"
            << job.maxAge
            << R"HTML(
</span>

<span class="tag">
)HTML"
            << htmlEscape(job.jobType)
            << R"HTML(
</span>

</div>

<strong>
)HTML"
            << htmlEscape(job.pay)
            << R"HTML(
</strong>

<br><br>

<a
    class="button"
    href="/job?id=)HTML"
            << job.id
            << R"HTML("
>
View Job
</a>

</div>

)HTML";
    }

    if (count == 0)
    {
        html
            << R"HTML(

<div class="card">

<h2>
No matching jobs
</h2>

<p>
Try another age, location, or keyword.
</p>

</div>

)HTML";
    }

    html
        << R"HTML(

</div>

</div>

)HTML";

    return page(
        "Search",
        html.str()
    );
}


// ============================================================
// HTTP HANDLERS
// ============================================================

void handleRequest(
    SOCKET client,
    const HttpRequest& request
)
{
    // --------------------------------------------------------
    // HOME
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/"
    )
    {
        sendHTML(
            client,
            homePage(request)
        );

        return;
    }


    // --------------------------------------------------------
    // TEEN SIGNUP
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/teen-signup"
    )
    {
        sendHTML(
            client,
            teenSignupPage()
        );

        return;
    }


    if (
        request.method == "POST" &&
        request.path == "/teen-signup"
    )
    {
        std::string name =
            request.form.at("name");

        std::string email =
            toLower(
                request.form.at("email")
            );

        std::string password =
            request.form.at("password");

        int age = 0;

        try
        {
            age =
                std::stoi(
                    request.form.at("age")
                );
        }
        catch (...)
        {
            age = 0;
        }

        if (
            name.empty() ||
            email.empty() ||
            password.empty()
        )
        {
            sendHTML(
                client,
                teenSignupPage(
                    "All fields are required."
                )
            );

            return;
        }

        if (
            age < 13 ||
            age > 18
        )
        {
            sendHTML(
                client,
                teenSignupPage(
                    "Teen accounts must be between ages 13 and 18."
                )
            );

            return;
        }

        for (const User& user : users)
        {
            if (
                !user.removed &&
                user.email == email
            )
            {
                sendHTML(
                    client,
                    teenSignupPage(
                        "That email is already registered."
                    )
                );

                return;
            }
        }

        User user;

        user.id =
            nextUserId();

        user.name =
            name;

        user.email =
            email;

        user.password =
            password;

        user.age =
            age;

        user.role =
            UserRole::TEEN;

        users.push_back(
            user
        );

        saveUsers();

        std::string token =
            createSession(
                user.id,
                user.role
            );

        redirect(
            client,
            "/teen-dashboard",
            "session=" +
                token +
                "; Path=/; HttpOnly"
        );

        return;
    }


    // --------------------------------------------------------
    // TEEN LOGIN
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/teen-login"
    )
    {
        sendHTML(
            client,
            teenLoginPage()
        );

        return;
    }


    if (
        request.method == "POST" &&
        request.path == "/teen-login"
    )
    {
        std::string email =
            toLower(
                request.form.at("email")
            );

        std::string password =
            request.form.at("password");

        for (User& user : users)
        {
            if (
                !user.removed &&
                user.role ==
                    UserRole::TEEN &&
                user.email == email &&
                user.password == password
            )
            {
                std::string token =
                    createSession(
                        user.id,
                        user.role
                    );

                redirect(
                    client,
                    "/teen-dashboard",
                    "session=" +
                        token +
                        "; Path=/; HttpOnly"
                );

                return;
            }
        }

        sendHTML(
            client,
            teenLoginPage(
                "Invalid email or password."
            )
        );

        return;
    }


    // --------------------------------------------------------
    // BUSINESS SIGNUP
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/business-signup"
    )
    {
        sendHTML(
            client,
            businessSignupPage()
        );

        return;
    }


    if (
        request.method == "POST" &&
        request.path == "/business-signup"
    )
    {
        std::string name =
            request.form.at("name");

        std::string email =
            toLower(
                request.form.at("email")
            );

        std::string password =
            request.form.at("password");

        if (
            name.empty() ||
            email.empty() ||
            password.empty()
        )
        {
            sendHTML(
                client,
                businessSignupPage(
                    "All fields are required."
                )
            );

            return;
        }

        // Never allow signup to create an admin.
        if (
            email ==
            ADMIN_EMAIL
        )
        {
            sendHTML(
                client,
                businessSignupPage(
                    "That email is reserved for site administration."
                )
            );

            return;
        }

        for (const User& user : users)
        {
            if (
                !user.removed &&
                user.email == email
            )
            {
                sendHTML(
                    client,
                    businessSignupPage(
                        "That email is already registered."
                    )
                );

                return;
            }
        }

        User user;

        user.id =
            nextUserId();

        user.name =
            name;

        user.email =
            email;

        user.password =
            password;

        user.role =
            UserRole::BUSINESS;

        users.push_back(
            user
        );

        saveUsers();

        std::string token =
            createSession(
                user.id,
                user.role
            );

        redirect(
            client,
            "/business-dashboard",
            "session=" +
                token +
                "; Path=/; HttpOnly"
        );

        return;
    }


    // --------------------------------------------------------
    // BUSINESS LOGIN
    //
    // ADMIN ALSO LOGS IN HERE.
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        (
            request.path ==
                "/business-login" ||
            request.path ==
                "/admin-login"
        )
    )
    {
        sendHTML(
            client,
            businessLoginPage()
        );

        return;
    }


    if (
        request.method == "POST" &&
        request.path == "/business-login"
    )
    {
        std::string email =
            toLower(
                request.form.at("email")
            );

        std::string password =
            request.form.at("password");


        // First check owner/admin credentials.

        if (
            isConfiguredAdmin(
                email,
                password
            )
        )
        {
            User* admin =
                nullptr;

            for (User& user : users)
            {
                if (
                    user.email ==
                        ADMIN_EMAIL &&
                    user.role ==
                        UserRole::ADMIN
                )
                {
                    admin =
                        &user;

                    break;
                }
            }

            if (!admin)
            {
                User newAdmin;

                newAdmin.id =
                    nextUserId();

                newAdmin.name =
                    "Site Administrator";

                newAdmin.email =
                    ADMIN_EMAIL;

                newAdmin.password =
                    ADMIN_PASSWORD;

                newAdmin.age = 0;

                newAdmin.role =
                    UserRole::ADMIN;

                users.push_back(
                    newAdmin
                );

                saveUsers();

                admin =
                    &users.back();
            }

            std::string token =
                createSession(
                    admin->id,
                    UserRole::ADMIN
                );

            redirect(
                client,
                "/admin",
                "session=" +
                    token +
                    "; Path=/; HttpOnly"
            );

            return;
        }


        // Otherwise, find a normal business.

        for (User& user : users)
        {
            if (
                !user.removed &&
                user.role ==
                    UserRole::BUSINESS &&
                user.email == email &&
                user.password == password
            )
            {
                std::string token =
                    createSession(
                        user.id,
                        user.role
                    );

                redirect(
                    client,
                    "/business-dashboard",
                    "session=" +
                        token +
                        "; Path=/; HttpOnly"
                );

                return;
            }
        }

        sendHTML(
            client,
            businessLoginPage(
                "Invalid business/admin credentials."
            )
        );

        return;
    }


    // --------------------------------------------------------
    // LOGOUT
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/logout"
    )
    {
        auto it =
            request.cookies.find(
                "session"
            );

        if (
            it !=
            request.cookies.end()
        )
        {
            deleteSession(
                it->second
            );
        }

        redirect(
            client,
            "/",
            "session=deleted; Path=/; Max-Age=0"
        );

        return;
    }


    // --------------------------------------------------------
    // TEEN DASHBOARD
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/teen-dashboard"
    )
    {
        if (
            !requireTeen(
                client,
                request
            )
        )
        {
            return;
        }

        sendHTML(
            client,
            teenDashboard(request)
        );

        return;
    }


    // --------------------------------------------------------
    // BUSINESS DASHBOARD
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/business-dashboard"
    )
    {
        if (
            !requireBusiness(
                client,
                request
            )
        )
        {
            return;
        }

        User* user =
            currentUser(request);

        if (
            user->role ==
            UserRole::ADMIN
        )
        {
            redirect(
                client,
                "/admin"
            );

            return;
        }

        sendHTML(
            client,
            businessDashboard(request)
        );

        return;
    }


    // --------------------------------------------------------
    // POST JOB
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/post-job"
    )
    {
        if (
            !requireBusinessAccount(
                client,
                request
            )
        )
        {
            return;
        }

        sendHTML(
            client,
            postJobPage()
        );

        return;
    }


    if (
        request.method == "POST" &&
        request.path == "/post-job"
    )
    {
        if (
            !requireBusinessAccount(
                client,
                request
            )
        )
        {
            return;
        }

        User* user =
            currentUser(request);

        std::string title =
            request.form.at("title");

        std::string company =
            request.form.at("company");

        std::string location =
            request.form.at("location");

        std::string description =
            request.form.at("description");

        std::string jobType =
            request.form.at("jobType");

        std::string schedule =
            request.form.at("schedule");

        std::string pay =
            request.form.at("pay");

        int minAge = 13;
        int maxAge = 18;

        try
        {
            minAge =
                std::stoi(
                    request.form.at("minAge")
                );

            maxAge =
                std::stoi(
                    request.form.at("maxAge")
                );
        }
        catch (...)
        {
            sendHTML(
                client,
                postJobPage(
                    "Age values must be numbers."
                )
            );

            return;
        }

        if (
            minAge < 13 ||
            maxAge > 18 ||
            minAge > maxAge
        )
        {
            sendHTML(
                client,
                postJobPage(
                    "Age range must be between 13 and 18."
                )
            );

            return;
        }


        bool applicationsOpen =
            request.form.at(
                "applicationsOpen"
            ) != "0";


        long long publishAt = 0;
        long long expireAt = 0;

        try
        {
            std::string p =
                request.form.at("publishAt");

            if (!p.empty())
            {
                publishAt =
                    std::stoll(p);
            }

            std::string e =
                request.form.at("expireAt");

            if (!e.empty())
            {
                expireAt =
                    std::stoll(e);
            }
        }
        catch (...)
        {
            sendHTML(
                client,
                postJobPage(
                    "Publish/expiration timestamps must be numbers."
                )
            );

            return;
        }


        std::vector<std::string> questionTexts;

        for (int i = 1; i <= 5; ++i)
        {
            std::string key =
                "question" +
                std::to_string(i);

            std::string value =
                request.form.at(key);

            if (!value.empty())
            {
                questionTexts.push_back(
                    value
                );
            }
        }


        if (
            questionTexts.empty()
        )
        {
            sendHTML(
                client,
                postJobPage(
                    "You must create at least one application question."
                )
            );

            return;
        }


        Job job;

        job.id =
            nextJobId();

        job.businessId =
            user->id;

        job.title =
            title;

        job.company =
            company;

        job.location =
            location;

        job.description =
            description;

        job.minAge =
            minAge;

        job.maxAge =
            maxAge;

        job.jobType =
            jobType;

        job.schedule =
            schedule;

        job.pay =
            pay;

        job.applicationsOpen =
            applicationsOpen;

        job.publishAt =
            publishAt;

        job.expireAt =
            expireAt;


        for (
            const std::string& questionText :
            questionTexts
        )
        {
            Question question;

            question.id =
                nextQuestionId();

            question.text =
                questionText;

            job.questions.push_back(
                question
            );
        }


        jobs.push_back(
            job
        );

        saveJobs();

        redirect(
            client,
            "/business-dashboard"
        );

        return;
    }


    // --------------------------------------------------------
    // BUSINESS APPLICATION COUNT
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/business-application-count"
    )
    {
        User* user =
            currentUser(request);

        if (
            !user ||
            (
                user->role != UserRole::BUSINESS &&
                user->role != UserRole::ADMIN
            )
        )
        {
            sendHTML(client, "0");
            return;
        }

        int count = 0;

        for (const Application& application : applications)
        {
            if (application.removed)
            {
                continue;
            }

            Job* job =
                findJob(application.jobId);

            if (
                !job ||
                job->removed
            )
            {
                continue;
            }

            if (
                user->role == UserRole::ADMIN ||
                job->businessId == user->id
            )
            {
                count++;
            }
        }

        sendHTML(
            client,
            std::to_string(count)
        );

        return;
    }


    // --------------------------------------------------------
    // BUSINESS APPLICATIONS
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/business-applications"
    )
    {
        if (
            !requireBusiness(
                client,
                request
            )
        )
        {
            return;
        }

        User* user =
            currentUser(request);

        std::ostringstream html;

        html
            << R"HTML(

<div class="container">

<div class="card">

<h1>
Open Applications
</h1>

<p>
Applications submitted to your active job listings.
</p>

</div>

)HTML";

        bool found = false;

        for (
            const Application& application :
            applications
        )
        {
            if (application.removed)
            {
                continue;
            }

            Job* job =
                findJob(application.jobId);

            if (
                !job ||
                job->removed
            )
            {
                continue;
            }

            if (
                user->role != UserRole::ADMIN &&
                job->businessId != user->id
            )
            {
                continue;
            }

            const User* teen =
                findUserConst(application.teenId);

            found = true;

            html
                << R"HTML(

<div class="card">

<h2>
)HTML"
                << htmlEscape(job->title)
                << R"HTML(
</h2>

<p>
Applicant:
<strong>
)HTML"
                << (
                    teen
                    ? htmlEscape(teen->name)
                    : "Unknown"
                )
                << R"HTML(
</strong>
</p>

<p>
Status:
<strong>
)HTML"
                << htmlEscape(application.status)
                << R"HTML(
</strong>
</p>

<a
    class="button"
    href="/business-job?id=)HTML"
                << job->id
                << R"HTML("
>
View Application
</a>

</div>

)HTML";
        }

        if (!found)
        {
            html
                << R"HTML(

<div class="card">
No open applications right now.
</div>

)HTML";
        }

        html
            << R"HTML(

</div>

)HTML";

        sendHTML(
            client,
            page(
                "Open Applications",
                html.str()
            )
        );

        return;
    }


    // --------------------------------------------------------
    // BUSINESS JOB
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/business-job"
    )
    {
        if (
            !requireBusiness(
                client,
                request
            )
        )
        {
            return;
        }

        int id = 0;

        try
        {
            id =
                std::stoi(
                    request.query.at(
                        "id"
                    )
                );
        }
        catch (...)
        {
            send400(
                client,
                "Invalid job ID."
            );

            return;
        }

        sendHTML(
            client,
            businessJobPage(
                id,
                request
            )
        );

        return;
    }


    // --------------------------------------------------------
    // PUBLIC JOB
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/job"
    )
    {
        int id = 0;

        try
        {
            id =
                std::stoi(
                    request.query.at(
                        "id"
                    )
                );
        }
        catch (...)
        {
            send400(
                client,
                "Invalid job ID."
            );

            return;
        }

        sendHTML(
            client,
            jobPage(
                id,
                request
            )
        );

        return;
    }


    // --------------------------------------------------------
    // APPLY
    // --------------------------------------------------------

    if (
        request.method == "POST" &&
        request.path == "/apply"
    )
    {
        if (
            !requireTeen(
                client,
                request
            )
        )
        {
            return;
        }

        User* teen =
            currentUser(request);

        int jobId = 0;

        try
        {
            jobId =
                std::stoi(
                    request.form.at("jobId")
                );
        }
        catch (...)
        {
            send400(
                client,
                "Invalid job."
            );

            return;
        }

        Job* job =
            findJob(jobId);

        if (!job || job->removed)
        {
            send400(
                client,
                "Job does not exist."
            );

            return;
        }

        if (
            !job->applicationsOpen
        )
        {
            send400(
                client,
                "Applications are closed."
            );

            return;
        }

        if (
            teen->age <
                job->minAge ||
            teen->age >
                job->maxAge
        )
        {
            send400(
                client,
                "You are not within the required age range."
            );

            return;
        }


        // Prevent duplicate applications.

        for (
            const Application& existing :
            applications
        )
        {
            if (
                !existing.removed &&
                existing.jobId ==
                    jobId &&
                existing.teenId ==
                    teen->id
            )
            {
                sendHTML(
                    client,
                    page(
                        "Already Applied",
                        "<div class='container'>"
                        "<div class='card'>"
                        "<h1>You've already applied.</h1>"
                        "<a class='button' href='/teen-dashboard'>"
                        "View Dashboard"
                        "</a>"
                        "</div></div>"
                    )
                );

                return;
            }
        }


        std::ostringstream answers;

        for (
            const Question& question :
            job->questions
        )
        {
            std::string key =
                "answer_" +
                std::to_string(
                    question.id
                );

            std::string answer =
                request.form.at(key);

            if (answer.empty())
            {
                send400(
                    client,
                    "Every application question must be answered."
                );

                return;
            }

            answers
                << question.text
                << "\nAnswer: "
                << answer
                << "\n\n";
        }


        Application application;

        application.id =
            nextApplicationId();

        application.jobId =
            jobId;

        application.teenId =
            teen->id;

        application.answers =
            answers.str();

        application.status =
            "Submitted";

        applications.push_back(
            application
        );

        saveApplications();

        sendHTML(
            client,
            page(
                "Application Submitted",
                "<div class='container'>"
                "<div class='success'>"
                "<h1>Application submitted!</h1>"
                "<p>Your application was sent to the employer.</p>"
                "<a class='button' href='/teen-dashboard'>"
                "View Dashboard"
                "</a>"
                "</div>"
                "</div>"
            )
        );

        return;
    }


    // --------------------------------------------------------
    // APPLICATION STATUS
    // --------------------------------------------------------

    if (
        request.method == "POST" &&
        request.path ==
            "/application-status"
    )
    {
        if (
            !requireBusiness(
                client,
                request
            )
        )
        {
            return;
        }

        User* user =
            currentUser(request);

        int applicationId = 0;

        try
        {
            applicationId =
                std::stoi(
                    request.form.at(
                        "applicationId"
                    )
                );
        }
        catch (...)
        {
            send400(
                client,
                "Invalid application."
            );

            return;
        }

        Application* application =
            findApplication(
                applicationId
            );

        if (!application)
        {
            send400(
                client,
                "Application not found."
            );

            return;
        }

        Job* job =
            findJob(
                application->jobId
            );

        if (!job)
        {
            send400(
                client,
                "Job not found."
            );

            return;
        }

        if (
            user->role !=
                UserRole::ADMIN &&
            job->businessId !=
                user->id
        )
        {
            send400(
                client,
                "You do not have permission."
            );

            return;
        }

        application->status =
            request.form.at("status");

        saveApplications();

        redirect(
            client,
            "/business-job?id=" +
                std::to_string(
                    job->id
                )
        );

        return;
    }


    // --------------------------------------------------------
    // SEARCH
    // --------------------------------------------------------

    if (
        request.method == "GET" &&
        request.path == "/search"
    )
    {
        int age = 0;

        try
        {
            age =
                std::stoi(
                    request.query.at(
                        "age"
                    )
                );
        }
        catch (...)
        {
            sendHTML(
                client,
                page(
                    "Search",
                    "<div class='container'>"
                    "<div class='danger'>"
                    "Please enter an age from 13 to 18."
                    "</div>"
                    "</div>"
                )
            );

            return;
        }

        if (
            age < 13 ||
            age > 18
        )
        {
            send400(
                client,
                "Age must be between 13 and 18."
            );

            return;
        }

        std::string search;

        std::string location;

        auto searchIt =
            request.query.find(
                "search"
            );

        if (
            searchIt !=
            request.query.end()
        )
        {
            search =
                searchIt->second;
        }

        auto locationIt =
            request.query.find(
                "location"
            );

        if (
            locationIt !=
            request.query.end()
        )
        {
            location =
                locationIt->second;
        }

        sendHTML(
            client,
            searchPage(
                age,
                search,
                location
            )
        );

        return;
    }


    // ========================================================
    // ADMIN
    // ========================================================

    if (
        request.method == "GET" &&
        request.path == "/admin"
    )
    {
        if (
            !requireAdmin(
                client,
                request
            )
        )
        {
            return;
        }

        sendHTML(
            client,
            adminDashboard(request)
        );

        return;
    }


    if (
        request.method == "GET" &&
        request.path == "/admin-job"
    )
    {
        if (
            !requireAdmin(
                client,
                request
            )
        )
        {
            return;
        }

        int id = 0;

        try
        {
            id =
                std::stoi(
                    request.query.at(
                        "id"
                    )
                );
        }
        catch (...)
        {
            send400(
                client,
                "Invalid job ID."
            );

            return;
        }

        sendHTML(
            client,
            adminJobPage(id)
        );

        return;
    }


    // --------------------------------------------------------
    // ADMIN REMOVE JOB
    // --------------------------------------------------------

    if (
        request.method == "POST" &&
        request.path ==
            "/admin-remove-job"
    )
    {
        if (
            !requireAdmin(
                client,
                request
            )
        )
        {
            return;
        }

        int jobId = 0;

        try
        {
            jobId =
                std::stoi(
                    request.form.at("jobId")
                );
        }
        catch (...)
        {
            send400(
                client,
                "Invalid job."
            );

            return;
        }

        Job* job =
            findJob(jobId);

        if (job)
        {
            job->removed =
                true;

            saveJobs();
        }

        redirect(
            client,
            "/admin"
        );

        return;
    }


    // --------------------------------------------------------
    // ADMIN TOGGLE APPLICATIONS
    // --------------------------------------------------------

    if (
        request.method == "POST" &&
        request.path ==
            "/admin-toggle-applications"
    )
    {
        if (
            !requireAdmin(
                client,
                request
            )
        )
        {
            return;
        }

        int jobId = 0;

        try
        {
            jobId =
                std::stoi(
                    request.form.at("jobId")
                );
        }
        catch (...)
        {
            send400(
                client,
                "Invalid job."
            );

            return;
        }

        Job* job =
            findJob(jobId);

        if (job)
        {
            job->applicationsOpen =
                !job->applicationsOpen;

            saveJobs();
        }

        redirect(
            client,
            "/admin-job?id=" +
                std::to_string(jobId)
        );

        return;
    }


    // --------------------------------------------------------
    // ADMIN REMOVE USER
    // --------------------------------------------------------

    if (
        request.method == "POST" &&
        request.path ==
            "/admin-remove-user"
    )
    {
        if (
            !requireAdmin(
                client,
                request
            )
        )
        {
            return;
        }

        int userId = 0;

        try
        {
            userId =
                std::stoi(
                    request.form.at("userId")
                );
        }
        catch (...)
        {
            send400(
                client,
                "Invalid user."
            );

            return;
        }

        User* user =
            findUser(userId);

        if (
            user &&
            user->role !=
                UserRole::ADMIN
        )
        {
            user->removed =
                true;

            saveUsers();
        }

        redirect(
            client,
            "/admin"
        );

        return;
    }


    // --------------------------------------------------------
    // ADMIN REMOVE APPLICATION
    // --------------------------------------------------------

    if (
        request.method == "POST" &&
        request.path ==
            "/admin-remove-application"
    )
    {
        if (
            !requireAdmin(
                client,
                request
            )
        )
        {
            return;
        }

        int applicationId = 0;

        try
        {
            applicationId =
                std::stoi(
                    request.form.at(
                        "applicationId"
                    )
                );
        }
        catch (...)
        {
            send400(
                client,
                "Invalid application."
            );

            return;
        }

        Application* application =
            findApplication(
                applicationId
            );

        if (application)
        {
            application->removed =
                true;

            saveApplications();
        }

        redirect(
            client,
            "/admin"
        );

        return;
    }


    send404(client);
}


// ============================================================
// RECEIVE HTTP REQUEST
// ============================================================

std::string receiveRequest(
    SOCKET client
)
{
    std::string request;

    char buffer[8192];

    int received = 0;

    size_t headerEnd =
        std::string::npos;

    int contentLength = 0;

    do
    {
        received =
            recv(
                client,
                buffer,
                sizeof(buffer),
                0
            );

        if (received <= 0)
        {
            break;
        }

        request.append(
            buffer,
            received
        );

        headerEnd =
            request.find(
                "\r\n\r\n"
            );

        if (
            headerEnd !=
            std::string::npos
        )
        {
            std::string headers =
                request.substr(
                    0,
                    headerEnd
                );

            std::string lower =
                toLower(headers);

            size_t pos =
                lower.find(
                    "content-length:"
                );

            if (
                pos !=
                std::string::npos
            )
            {
                pos +=
                    std::string(
                        "content-length:"
                    ).size();

                while (
                    pos <
                        lower.size() &&
                    std::isspace(
                        static_cast<unsigned char>(
                            lower[pos]
                        )
                    )
                )
                {
                    pos++;
                }

                std::string number;

                while (
                    pos <
                        lower.size() &&
                    std::isdigit(
                        static_cast<unsigned char>(
                            lower[pos]
                        )
                    )
                )
                {
                    number +=
                        lower[pos];

                    pos++;
                }

                if (!number.empty())
                {
                    contentLength =
                        std::stoi(
                            number
                        );
                }
            }

            size_t bodyStart =
                headerEnd + 4;

            if (
                request.size() >=
                    bodyStart +
                        static_cast<size_t>(
                            contentLength
                        )
            )
            {
                break;
            }
        }

    } while (
        received ==
        sizeof(buffer)
    );

    return request;
}


// ============================================================
// CLIENT
// ============================================================

void handleClient(
    SOCKET client
)
{
    std::string raw =
        receiveRequest(client);

    if (raw.empty())
    {
        closesocket(client);

        return;
    }

    HttpRequest request =
        parseRequest(raw);

    handleRequest(
        client,
        request
    );

    closesocket(client);
}


// ============================================================
// SAMPLE JOBS
// ============================================================

void createSampleJobs()
{
    if (!jobs.empty())
    {
        return;
    }

    Job cashier;

    cashier.id = 1;
    cashier.businessId = 0;
    cashier.title = "Cashier";
    cashier.company = "Sunny Market";
    cashier.location = "Hyannis";
    cashier.description =
        "Help customers, operate the register, "
        "and keep the checkout area organized.";
    cashier.minAge = 16;
    cashier.maxAge = 18;
    cashier.jobType = "Part-time";
    cashier.schedule = "After school";
    cashier.pay = "$16/hour";
    cashier.applicationsOpen = true;

    cashier.questions.push_back({
        1,
        "Why are you interested in working here?"
    });

    cashier.questions.push_back({
        2,
        "What days are you available?"
    });

    jobs.push_back(
        cashier
    );


    Job iceCream;

    iceCream.id = 2;
    iceCream.businessId = 0;
    iceCream.title =
        "Ice Cream Shop Worker";
    iceCream.company =
        "Cape Scoops";
    iceCream.location =
        "Hyannis";
    iceCream.description =
        "Serve customers, prepare orders, "
        "and keep the shop clean.";
    iceCream.minAge = 14;
    iceCream.maxAge = 18;
    iceCream.jobType =
        "Part-time";
    iceCream.schedule =
        "After school";
    iceCream.pay =
        "$16/hour";
    iceCream.applicationsOpen =
        true;

    iceCream.questions.push_back({
        3,
        "Why would you like to work at an ice cream shop?"
    });

    jobs.push_back(
        iceCream
    );


    saveJobs();
}


// ============================================================
// MAIN
// ============================================================

int getPort()
{
    const char* value = std::getenv("PORT");

    if (!value || !*value)
    {
        return 18080;
    }

    try
    {
        int port = std::stoi(value);

        if (port >= 1 && port <= 65535)
        {
            return port;
        }
    }
    catch (...)
    {
    }

    return 18080;
}


int main()
{
    std::cout
        << "Starting TeenJobs...\n";

    loadUsers();
    loadJobs();
    loadApplications();

    // --------------------------------------------------------
    // ADMIN CONFIGURATION
    // --------------------------------------------------------
    // Public deployments should provide these as environment
    // variables rather than committing credentials to source.
    // --------------------------------------------------------

    const char* adminEmailEnv =
        std::getenv("TEENJOBS_ADMIN_EMAIL");

    const char* adminPasswordEnv =
        std::getenv("TEENJOBS_ADMIN_PASSWORD");

    if (adminEmailEnv && *adminEmailEnv)
    {
        // These const globals are retained for compatibility with
        // the existing application logic. The deployment values
        // are copied into them before user initialization.
        const_cast<std::string&>(ADMIN_EMAIL) = adminEmailEnv;
    }

    if (adminPasswordEnv && *adminPasswordEnv)
    {
        const_cast<std::string&>(ADMIN_PASSWORD) = adminPasswordEnv;
    }

    if (ADMIN_EMAIL.empty() || ADMIN_PASSWORD.empty())
    {
        std::cerr
            << "ERROR: Set TEENJOBS_ADMIN_EMAIL and "
               "TEENJOBS_ADMIN_PASSWORD before starting TeenJobs.\n";

        return 1;
    }

    bool adminExists = false;

    for (const User& user : users)
    {
        if (
            user.role == UserRole::ADMIN &&
            user.email == ADMIN_EMAIL
        )
        {
            adminExists = true;
            break;
        }
    }

    if (!adminExists)
    {
        User admin;

        admin.id = nextUserId();
        admin.name = "Site Administrator";
        admin.email = ADMIN_EMAIL;
        admin.password = ADMIN_PASSWORD;
        admin.role = UserRole::ADMIN;

        users.push_back(admin);
        saveUsers();
    }

    createSampleJobs();

#ifdef _WIN32
    WSADATA wsaData;

    int result =
        WSAStartup(
            MAKEWORD(2, 2),
            &wsaData
        );

    if (result != 0)
    {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }
#endif

    SOCKET serverSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (serverSocket == INVALID_SOCKET)
    {
        std::cerr << "Could not create server socket.\n";

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

    int reuse = 1;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuse),
        sizeof(reuse)
    );

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(
        static_cast<unsigned short>(getPort())
    );

    if (
        bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)
        ) == SOCKET_ERROR
    )
    {
        std::cerr
            << "Could not bind port "
            << getPort()
            << ".\n";

        closesocket(serverSocket);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

    if (listen(serverSocket, 128) == SOCKET_ERROR)
    {
        std::cerr << "Could not listen.\n";

        closesocket(serverSocket);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

    const int port = getPort();

    std::cout
        << "\n=====================================\n"
        << " TeenJobs is running!\n"
        << " Port: " << port << "\n"
        << " Local: http://localhost:" << port << "\n"
        << "=====================================\n\n";

    while (true)
    {
        sockaddr_in clientAddress{};

#ifdef _WIN32
        int clientSize = sizeof(clientAddress);
#else
        socklen_t clientSize = sizeof(clientAddress);
#endif

        SOCKET client =
            accept(
                serverSocket,
                reinterpret_cast<sockaddr*>(&clientAddress),
                &clientSize
            );

        if (client == INVALID_SOCKET)
        {
            continue;
        }

        // Handle each connection independently so multiple users
        // can access the site without waiting on one another.
        std::thread(
            [client]()
            {
                handleClient(client);
            }
        ).detach();
    }

    closesocket(serverSocket);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
