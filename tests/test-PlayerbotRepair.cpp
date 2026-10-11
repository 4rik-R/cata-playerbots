/* GPL v2 or later. See AUTHORS.md and PORTING.md. */
#include "../src/Ai/Base/PlayerbotRepair.h"
#include <catch2/catch.hpp>
TEST_CASE("Repair requests remain busy while in flight and reject stale completion", "[playerbot][repair]")
{
    PlayerbotRepair::Mailbox mailbox;
    REQUIRE_FALSE(mailbox.Post(0, 5, 530, 0, 10));
    REQUIRE_FALSE(mailbox.Post(3, 0, 530, 0, 10));
    REQUIRE(mailbox.Post(3, 5, 530, 0, 10));
    auto first = mailbox.Take(); REQUIRE(first);
    REQUIRE_FALSE(mailbox.Post(3, 5, 530, 0, 11));
    mailbox.Finish(first->Serial + 1);
    REQUIRE_FALSE(mailbox.Post(3, 5, 530, 0, 12));
    mailbox.Finish(first->Serial);
    REQUIRE(mailbox.Post(3, 6, 530, 0, 13));
    mailbox.Finish(first->Serial);
    REQUIRE_FALSE(mailbox.Post(3, 6, 530, 0, 14));
    auto second = mailbox.Take(); REQUIRE(second);
    REQUIRE(second->Npc == 6);
}
TEST_CASE("Repair request expiry handles the native millisecond timer wrapping", "[playerbot][repair]")
{
    PlayerbotRepair::Request request{3, 530, 0, 0xfffffff0u, 5, 1};
    REQUIRE(PlayerbotRepair::Fresh(request, 0x10u));
    REQUIRE_FALSE(PlayerbotRepair::Fresh(request, uint32(request.Created + 5000)));
}
