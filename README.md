# Cataclysm Playerbots

An experimental port of [AzerothCore mod-playerbots](https://github.com/mod-playerbots/mod-playerbots)
to TrinityCore Cataclysm 4.3.4. The goal is the familiar Playerbots feature set
on Cata, using upstream behavior wherever it fits the native server.

Use the matching [iotadev TrinityCore fork](https://github.com/iotadev/TrinityCore).
The module depends on that fork's session and module hooks; stock TrinityCore
does not provide them. The core README records its corresponding module revision.
Building this repository alone does not produce a server.

## Current capabilities

The current development target is a human-led Warrior/Mage/Priest party.

- Native bot sessions, bounded character creation, configured managed rosters
  and authorized connection/disconnection are implemented. Ordinary party
  invitations adopt the controller; lifecycle access has separate account-link
  and faction checks.
- Separate combat, noncombat and dead engines follow the donor state model.
  Control changes, target loss and state transitions discard obsolete queued work.
  Native map transfers, death and resurrection remain server-owned.
- Warrior Protection/Arms/Fury and Mage Frost/Fire/Arcane have starter combat
  routes. Priest support covers healing, buffs, disease cures, resurrection and
  conservative damage. Shared roles feed target selection, threat, positioning
  and support. Class/spec rotations and high-level spell coverage remain incomplete.
- Follow, hold, attack, stop, range controls, strategy queries and manual rebuff
  use ordinary whispers and party/raid chat. Party chat respects raid subgroups.
  Optional slices include saved-position stay, learned ground mounts, ready
  checks, carried recovery items and native corpse opening.
- The MultiBot bridge supports handshake/ping, authorized rosters and managed
  lifecycle requests. Strategy STATE framing and single-bot/group mutation
  acknowledgements use the same strategy-control layer as ordinary chat.
- Inventory support includes bounded `gear?` inspection, explicit one-slot
  `gear apply`, starter scoring at levels 10–39 and guarded native loot votes.
  Qualified native affixes can be compared; unknown inputs remain unresolved.
- Human-led quest controls cover native incoming shares, nearby-giver acceptance,
  active-log inspection, explicit rewards and separately enabled abandonment.
  Native admission and inventory rules remain authoritative. Per-player quest
  drops are exempt from the optional human-first loot policy.
- Optional passive action history records bounded engine decisions and action
  returns for the separate context observer. Collection defaults off; a return
  value does not prove a spell landed or an asynchronous operation completed.

Implemented optional behavior requires its configuration gates and native
eligibility checks. A registered strategy or accepted cast does not establish
every spell effect, path or encounter outcome. Autonomous populations, autonomous questing,
travel, group formation and full dungeon/class parity remain future work.

## Validation

The October 2026 milestone combines party coordination, native gear/loot and
human-led quest work. Release qualification is:

| Source scope | Build result |
| --- | --- |
| Windows, all installed modules enabled | worldserver/tests-common built; 431/431 tests passed |
| Linux, complete current source and observer integration | worldserver/tests-common built; 431/431 tests passed |
| Windows, all three installed optional modules disabled | worldserver/tests-common built; 19/19 core tests passed |

Linux builds use Ubuntu 22.04/GCC 11.4 with normal PCH; Linux server runtime remains untested.
[PORTING.md](PORTING.md) records intermediate builds, donor revisions and adaptations.

Outdoor and Ragefire checks observed four-bot engagement, Warrior role actions,
Mage damage, Priest healing, tank aggro recovery, native corpse opening and
eating/drinking. All four completed dedicated entry into the party's Ragefire
instance. A later session observed DPS target reassessment and tank rescue.
These checks establish basic party operation; full clears, detailed positioning,
multi-tank behavior and quantitative healer mana savings remain open.
MultiBot group ACK/STATE/restore timing also needs a bundled runtime observation.

With the starter-score gate enabled, authorized `gear?` reports a bounded recent
survey. Testone's report was observed. Explicit `gear apply` additionally requires
the default-off StarterEquip gate; one empty-waist move passed native completion
and saved inventory preservation. Relogin and occupied-slot displacement remain
untested. The level-10–39 scoring model is a limited donor-derived heuristic.

Shared item-usage and non-affixed template comparisons retain explicit source
scope and unknown inputs. An optional native need/greed/pass adapter now connects
their roll-choice rules locally, with its
gate off. Windows validation and one controlled native decision/award check passed:
Testone needed, Testtwo greeded, casters passed, and the saved award was verified.
Broader runtime coverage remains pending. Automatic equipping, purchases and
disenchant classification remain ahead.
Template comparison does not establish the identity or eligibility of an actual
loot roll. The voting adapter additionally binds and rechecks native roll facts.

A real level-20 quest replay observed all four bots accept a native shared quest,
fight and loot its objectives, and reach native complete status. One explicit bot
turn-in was confirmed; its rewarded history and chosen item were saved after clean
shutdown. The other three bots remained complete but unrewarded. This is not
automatic quest travel/turn-in, a full quest-chain test or a relogin check.
The per-player loot exemption, reward batches, typed links and party-pushed
confirmation have source/regression coverage but no dedicated live qualification.

## Build and configuration

Place the module at `modules/mod-playerbots` in the matching core checkout.
The core discovers it as an optional static module. With the core's platform
dependencies installed, build from the core root:

```text
cmake -S . -B build-portable -DMODULE_MOD_PLAYERBOTS=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-portable --config RelWithDebInfo --target worldserver tests-common
ctest --test-dir build-portable -C RelWithDebInfo --output-on-failure
```

These commands build source and run tests. Database setup and extracted client
data are separate prerequisites for running a realm. Optional PowerShell helpers
are development tools; the C++ module and CMake build do not require Windows.
AHBot is a separately selectable module in the same fork.

Copy [conf/playerbots.conf.dist](conf/playerbots.conf.dist) to the realm's module
config directory and configure it deliberately. The default layout places
`modules/playerbots.conf` beside `worldserver.conf`; `Modules.ConfigDirectory`
can select another location. Bot admission and optional features default off.
Building the module does not create accounts or characters.

The optional `Playerbots.Healing.SaveMana.Enabled` policy applies donor mana
conservation to the ported Priest heals. It defaults off. Its percentage estimates
guide spell selection; native Cata spell costs and healing remain authoritative.
It still needs sustained-party qualification before broader use.

## Chat and addon controls

### Maintenance

With `Playerbots.Maintenance.Repair.Enabled = 1`, select a nearby repair NPC and
whisper a controlled bot `repair`, or use party/raid chat with the usual subgroup
scope. Both human and bot must be eligible to interact with the captured NPC.
The request expires after five seconds and rechecks control, location and idle
state on the world owner. Native repair-all uses bot funds and reputation discount;
insufficient money can leave partial repairs. Replies report observed equipped
durability and copper spent. Client qualification is pending. Selling, purchases
and automatic NPC travel remain ahead.
The October 11 follow-up built Windows worldserver/tests-common and passed
433/433 tests. Its Linux validation remains pending; the release table above
describes the published October 9 source.

### Human-led quests

Quest settings under `Playerbots.Quest` default off. The current controller can
use these commands by whisper, or authorized party chat where noted:

| Command | Gate | Behavior |
| --- | --- | --- |
| Native client quest share | AcceptShared.Enabled | Accept eligible incoming human shares through native handlers |
| `accept <quest>` / `accept *` | AcceptNpc.Enabled | Selected nearby giver; batch copies at most 25 offers and rechecks each |
| `quests [all\|completed\|incompleted\|summary]` | Inspection.Enabled | Read native active-log facts; `co`/`in` are filter aliases |
| `reward <quest> <item>` / `reward *` | Reward.Enabled | Explicit native reward; batch skips quests with several choice items |
| `share <quest>` | Share.Enabled | Whisper one bot to submit a native party offer; not proof of recipient acceptance |
| `drop <quest>` | Abandon.Enabled | Separate destructive opt-in; whisper one active quest only |

Quest/item operands accept numeric IDs or native links. Reward uses an item
entry, not a UI slot; item `0` is valid only for no-choice rewards. `reward *`
attempts at most 25 active IDs at the selected giver, with zero or one choice.
Mutation requests require fresh control, state and location checks. Partial
batches are not rolled back or automatically retried. No forced completion,
automatic travel, reward-choice guessing or rewarded-history reset is included.

`Playerbots.Quest.SyncLootWithPlayer.Enabled` optionally defers competitive
quest-class corpse items the human still needs. The native item's per-player
flag exempts shared drops; the group's loot method is not that flag. Deferral
does not reserve an item or change native recipient eligibility.

### Party and addon controls

Whisper an admitted bot, for example `/w Botmage list`, to list controllable
online bots. `follow`, `hold`, `stay`, `attack`, `stop` and `buff` use the same
authorization layer. Select a hostile target before requesting `attack`.
`stop` ceases combat; it leaves the bot logged in. A "requested" reply means
queued, and later responses report execution. Enabled `stay` holds a captured
position; its disabled fallback and `hold` use the existing plain stop behavior.

With `Playerbots.StrategyControl.Enabled`, `co ?`, `nc ?` and `de ?` query
registrations. Combat `focus/threat/potions` and noncombat `food/loot` support
`+`, `-` and `~`; role/spec strategies are protected, and dead state is query-only.
Changes require idle conditions and last for the current bot session. Global
feature settings still govern execution. A query can report "unchanged" because
it made no mutation; read the following strategy list for the actual state.

For the [Cata MultiBot addon candidate](addons/MultiBot/README.md), enable the
server bridge with `Playerbots.MultiBot.Enabled`. Structured strategy mutations
also require `Playerbots.StrategyControl.AddonMutations`; group requests require
the additional `Playerbots.StrategyControl.GroupMutations` gate. ALL selects
authorized controlled bots, GROUP/PARTY the current group including raids, and
RAID requires a raid. Execution rechecks control, login, group, phase and idle
conditions. Unresolved timeouts are unknown outcomes; refresh STATE before
retrying a toggle. Completed changes are not rolled back.

The full donor addon contains features the server has not ported. Its presence
does not imply class/talent data parity or support for every button.

## Contributing and provenance

Cata API comparisons, donor feature ports and repeatable bug reports are useful
contributions. Include the core/module revisions, bot class and level, and the
behavior observed. Keep credentials, client files and database dumps out of
reports. Follow [PORTING.md](PORTING.md) when documenting donor source and
adaptations so future upstream changes remain traceable.

Retained source headers specify GPL version 2 or later. This port preserves the
license and author credits; see [LICENSE](LICENSE) and [AUTHORS.md](AUTHORS.md).
