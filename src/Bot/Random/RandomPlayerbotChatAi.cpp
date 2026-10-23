/*
 * Local AI client for random-bot chat: plain HTTP/1.0 POST to Ollama's
 * /api/chat on a background thread. Written for this port (the idea of a
 * queued, rate-limited AI chat follows mod-ollama-chat, no code taken).
 * Released under GNU GPL v2 or any later version.
 */
#include "RandomPlayerbotChatAi.h"
#include "Log.h"
#include "Timer.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#ifndef _WIN32
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace RandomPlayerbotChatPolicy;

namespace
{
// Requests waiting longer than this are dropped; the bot has answered by itself by then.
constexpr std::size_t MaxQueued = 4;
constexpr uint32_t PauseAfterFailures = 3;
constexpr uint32_t PauseMs = 60000;
constexpr uint32_t WarmUpTimeoutMs = 120000;

struct Job
{
    RandomPlayerbotChatAi::Request Request;
    uint32_t Queued = 0;
    bool WarmUp = false;
};

// Never destroyed: the worker is detached and may outlive static destruction.
struct State
{
    std::mutex Lock;
    std::condition_variable Wake;
    std::deque<Job> Jobs;
    std::vector<RandomPlayerbotChatAi::Answer> Done;
    RandomPlayerbotChatAi::Settings Config;
    bool Started = false;
    bool Busy = false;
    RateWindow Budget;
    uint32_t Failures = 0;
    uint32_t PausedUntil = 0;
    bool Paused = false;
    bool WarnedUnreachable = false;
    std::string LastError;
};

State& Shared()
{
    static State* state = new State();
    return *state;
}

std::atomic<bool> IsEnabled { false };
std::atomic<uint32_t> Answers { 0 }, Failed { 0 }, Fallbacks { 0 };

#ifndef _WIN32
// One blocking HTTP exchange with an overall deadline. Returns the raw response or an error text.
bool Exchange(std::string const& host, uint32_t port, std::string const& request, uint32_t timeoutMs,
    std::string& response, std::string& error)
{
    uint32_t start = getMSTime();
    auto left = [&]() -> int
    {
        uint32_t spent = getMSTimeDiff(start, getMSTime());
        return spent >= timeoutMs ? 0 : int(timeoutMs - spent);
    };
    addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    std::string service = std::to_string(port);
    if (int rc = getaddrinfo(host.c_str(), service.c_str(), &hints, &found); rc != 0 || !found)
    {
        error = "cannot resolve " + host + " (" + gai_strerror(rc) + ")";
        return false;
    }
    int fd = -1;
    for (addrinfo* ai = found; ai && fd < 0; ai = ai->ai_next)
    {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
            break;
        if (errno == EINPROGRESS)
        {
            pollfd p { fd, POLLOUT, 0 };
            int soError = 0;
            socklen_t len = sizeof(soError);
            if (poll(&p, 1, left()) == 1 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &soError, &len) == 0 && soError == 0)
                break;
            error = soError ? std::strerror(soError) : "connect timeout";
        }
        else
            error = std::strerror(errno);
        close(fd);
        fd = -1;
    }
    freeaddrinfo(found);
    if (fd < 0)
    {
        error = "cannot connect to " + host + ":" + service + " (" + error + ")";
        return false;
    }
    std::size_t sent = 0;
    while (sent < request.size())
    {
        pollfd p { fd, POLLOUT, 0 };
        if (poll(&p, 1, left()) != 1)
        {
            error = "send timeout";
            close(fd);
            return false;
        }
        ssize_t n = send(fd, request.data() + sent, request.size() - sent, MSG_NOSIGNAL);
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            error = std::string("send failed (") + std::strerror(errno) + ")";
            close(fd);
            return false;
        }
        if (n > 0)
            sent += std::size_t(n);
    }
    char buffer[4096];
    while (true)
    {
        int wait = left();
        pollfd p { fd, POLLIN, 0 };
        if (wait <= 0 || poll(&p, 1, wait) != 1)
        {
            error = "no answer within " + std::to_string(timeoutMs / 1000) + " s";
            close(fd);
            return false;
        }
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
        if (n == 0)
            break;
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                continue;
            error = std::string("receive failed (") + std::strerror(errno) + ")";
            close(fd);
            return false;
        }
        response.append(buffer, std::size_t(n));
        if (response.size() > 256 * 1024)
            break;
    }
    close(fd);
    return true;
}
#endif

RandomPlayerbotChatAi::Answer Run(Job const& job, RandomPlayerbotChatAi::Settings const& config, std::string& error)
{
    RandomPlayerbotChatAi::Answer answer;
    answer.Id = job.Request.Id;
#ifdef _WIN32
    error = "not supported on Windows builds";
#else
    std::string body = ChatRequestJson(config.Model, job.Request.System, job.Request.Turns, config.MaxTokens,
        config.Temperature, config.KeepAlive);
    std::string request = "POST /api/chat HTTP/1.0\r\nHost: " + config.Host + ":" + std::to_string(config.Port) +
        "\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) +
        "\r\nConnection: close\r\n\r\n" + body;
    std::string raw;
    uint32_t timeout = job.WarmUp ? WarmUpTimeoutMs : config.TimeoutMs;
    // Time already spent waiting in the queue counts.
    uint32_t waited = getMSTimeDiff(job.Queued, getMSTime());
    if (!job.WarmUp && waited >= timeout)
    {
        error.clear();
        return answer;
    }
    if (!Exchange(config.Host, config.Port, request, job.WarmUp ? timeout : timeout - waited, raw, error))
        return answer;
    auto http = ParseHttp(raw);
    if (!http)
    {
        error = "unreadable HTTP answer";
        return answer;
    }
    if (http->Status != 200)
    {
        auto message = JsonField(http->Body, "error");
        error = "HTTP " + std::to_string(http->Status) + (message ? " (" + *message + ")" : std::string());
        return answer;
    }
    auto content = OllamaContent(http->Body);
    if (!content)
    {
        error = "answer without message content";
        return answer;
    }
    answer.Text = CleanReply(*content, job.Request.BotName);
    answer.Ok = !answer.Text.empty();
    if (!answer.Ok && !job.WarmUp)
        error.clear(); // refused or empty line: not a connection problem
#endif
    return answer;
}

void Worker()
{
    State& state = Shared();
    while (true)
    {
        Job job;
        RandomPlayerbotChatAi::Settings config;
        {
            std::unique_lock lock(state.Lock);
            state.Wake.wait(lock, [&] { return !state.Jobs.empty(); });
            job = std::move(state.Jobs.front());
            state.Jobs.pop_front();
            config = state.Config;
            state.Busy = true;
        }
        std::string error;
        RandomPlayerbotChatAi::Answer answer = Run(job, config, error);
        std::lock_guard lock(state.Lock);
        state.Busy = false;
        if (!error.empty())
        {
            ++state.Failures;
            Failed.fetch_add(1, std::memory_order_relaxed);
            state.LastError = error;
            if (!state.WarnedUnreachable)
                TC_LOG_ERROR("module.playerbots", "Playerbots chat AI: no answer from %s:%u, model %s: %s. "
                    "Bots answer with their own lines; check that Ollama runs (tools\\ki-einrichten.ps1).",
                    config.Host.c_str(), config.Port, config.Model.c_str(), error.c_str());
            state.WarnedUnreachable = true;
            if (state.Failures >= PauseAfterFailures)
            {
                state.Paused = true;
                state.PausedUntil = getMSTime() + PauseMs;
            }
        }
        else
        {
            if (state.WarnedUnreachable && answer.Ok)
                TC_LOG_ERROR("module.playerbots", "Playerbots chat AI: %s answers again.", config.Model.c_str());
            if (answer.Ok)
                state.WarnedUnreachable = false;
            state.Failures = 0;
            state.LastError.clear();
        }
        if (job.WarmUp)
            continue;
        if (answer.Ok)
            Answers.fetch_add(1, std::memory_order_relaxed);
        state.Done.push_back(std::move(answer));
    }
}
}

void RandomPlayerbotChatAi::Configure(Settings const& settings)
{
    State& state = Shared();
    bool start = false;
    {
        std::lock_guard lock(state.Lock);
        state.Config = settings;
        state.Config.TimeoutMs = std::max<uint32_t>(1000, settings.TimeoutMs);
        state.Config.MaxTokens = std::clamp<uint32_t>(settings.MaxTokens, 16, 200);
        state.Failures = 0;
        state.Paused = false;
        IsEnabled.store(settings.Enabled, std::memory_order_relaxed);
        if (settings.Enabled && !state.Started)
        {
            state.Started = true;
            start = true;
        }
        if (settings.Enabled)
        {
            Job warmUp;
            warmUp.WarmUp = true;
            warmUp.Queued = getMSTime();
            warmUp.Request.System = "Antworte mit einem Wort.";
            warmUp.Request.Turns.push_back({ false, "hallo" });
            state.Jobs.push_back(std::move(warmUp));
        }
    }
    if (start)
        std::thread(Worker).detach();
    state.Wake.notify_one();
    if (settings.Enabled)
        TC_LOG_INFO("server.loading", "Playerbots chat AI: model %s at %s:%u, timeout %u ms, at most %u answers per minute.",
            settings.Model.c_str(), settings.Host.c_str(), settings.Port, settings.TimeoutMs, settings.MaxPerMinute);
}

bool RandomPlayerbotChatAi::Enabled() { return IsEnabled.load(std::memory_order_relaxed); }

uint32_t RandomPlayerbotChatAi::TimeoutMs()
{
    State& state = Shared();
    std::lock_guard lock(state.Lock);
    return state.Config.TimeoutMs;
}

bool RandomPlayerbotChatAi::Submit(Request request, uint32_t now)
{
    if (!Enabled())
        return false;
    State& state = Shared();
    {
        std::lock_guard lock(state.Lock);
        if (state.Paused && int32_t(state.PausedUntil - now) > 0)
        {
            Fallbacks.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        if (state.Paused)
        {
            // One probe after the pause; failures pause again right away.
            state.Paused = false;
            state.Failures = PauseAfterFailures - 1;
        }
        if (state.Jobs.size() >= MaxQueued || !state.Budget.Allow(now, std::max<uint32_t>(1, state.Config.MaxPerMinute)))
        {
            Fallbacks.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        Job job;
        job.Request = std::move(request);
        job.Queued = now;
        state.Jobs.push_back(std::move(job));
    }
    state.Wake.notify_one();
    return true;
}

std::vector<RandomPlayerbotChatAi::Answer> RandomPlayerbotChatAi::Collect()
{
    State& state = Shared();
    std::lock_guard lock(state.Lock);
    std::vector<Answer> done;
    done.swap(state.Done);
    return done;
}

std::string RandomPlayerbotChatAi::Describe()
{
    if (!Enabled())
        return "ai off";
    State& state = Shared();
    std::lock_guard lock(state.Lock);
    std::string text = "ai " + state.Config.Model + " answers " + std::to_string(Answers.load()) + ", ai failed " +
        std::to_string(Failed.load()) + ", own lines instead " + std::to_string(Fallbacks.load());
    if (state.Paused)
        text += ", ai paused";
    if (!state.LastError.empty())
        text += " (" + state.LastError + ")";
    return text;
}
