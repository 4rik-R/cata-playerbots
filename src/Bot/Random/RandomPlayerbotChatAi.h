/*
 * Optional chat answers from a local Ollama server. Requests run on one
 * background thread; the world thread only queues them and collects the
 * finished answers, so a slow or missing AI never stalls the server.
 * Written for this port; released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_CHAT_AI_H
#define CATA_RANDOM_PLAYERBOT_CHAT_AI_H

#include "RandomPlayerbotChatPolicy.h"
#include <cstdint>
#include <string>
#include <vector>

namespace RandomPlayerbotChatAi
{
struct Settings
{
    bool Enabled = false;
    std::string Host = "host.docker.internal";
    uint32_t Port = 11434;
    std::string Model = "gemma3:4b";
    uint32_t TimeoutMs = 8000;
    uint32_t MaxPerMinute = 10;
    uint32_t MaxTokens = 60;
    float Temperature = 0.8f;
    std::string KeepAlive = "30m";
};

struct Request
{
    uint64_t Id = 0;
    std::string BotName;
    std::string System;
    std::vector<RandomPlayerbotChatPolicy::ChatTurn> Turns;
};

struct Answer
{
    uint64_t Id = 0;
    bool Ok = false;
    std::string Text;
};

// World thread. Starts the worker on first use and sends a warm-up request so the model is loaded.
void Configure(Settings const& settings);
bool Enabled();
uint32_t TimeoutMs();
// World thread. False if the AI is off, paused after errors, busy or over its budget; the caller answers itself then.
bool Submit(Request request, uint32_t now);
// World thread. Answers finished since the last call.
std::vector<Answer> Collect();
std::string Describe();
}

#endif
