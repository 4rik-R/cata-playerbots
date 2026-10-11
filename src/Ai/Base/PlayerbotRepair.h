/* GPL v2 or later. RepairAllAction donor adaptation; see PORTING.md. */
#ifndef PLAYERBOT_REPAIR_H
#define PLAYERBOT_REPAIR_H
#include "Define.h"
#include <mutex>
#include <optional>
class WorldSession;
namespace PlayerbotRepair
{
struct Request
{
    uint32 Requester, Map, Instance, Created;
    uint64 Npc, Serial;
};
inline bool Fresh(Request const& request, uint32 now) { return uint32(now - request.Created) < 5000; }
class Mailbox
{
public:
    bool Post(uint32 requester, uint64 npc, uint32 map, uint32 instance, uint32 now)
    {
        if (!requester || !npc) return false;
        std::lock_guard<std::mutex> guard(_mutex);
        if (_busy) return false;
        _busy = true;
        _pending = Request{requester, map, instance, now, npc, ++_serial};
        return true;
    }
    std::optional<Request> Take()
    {
        std::lock_guard<std::mutex> guard(_mutex);
        auto request = _pending; _pending.reset(); return request;
    }
    void Finish(uint64 serial)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        if (serial == _serial) _busy = false;
    }
private:
    std::mutex _mutex;
    bool _busy = false;
    uint64 _serial = 0;
    std::optional<Request> _pending;
};
void Update(WorldSession& session, Mailbox& mailbox);
}
#endif
