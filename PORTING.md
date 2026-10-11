# Port provenance and remaining work

## Selected-NPC maintenance follow-up — 2026-10-11

Fetched donor remote master at `79bd428115c8f33b74b13de23b133d49c3f75c60`.
Audited `src/Ai/Base/Actions/RepairAllAction.cpp`, `SellAction.cpp` and
`BuyAction.cpp`. Repair uses native repair NPC eligibility, reputation discount
and paid durability repair. Selling depends on donor item visitors and vendor/AH/
profession classifications; current unknown usage cannot authorize a sale.
Buying depends on budget, vendor stock, usage and consumable selection. Those
dependencies remain ahead; NPC discovery/travel is not required for explicit repair.
The three audited action files are unchanged between the previous `037c0141`
reference and this new master. The new pin applies to this audit; older feature
provenance remains pinned to its original donor revisions.

Adaptation: a separate PlayerbotRepair adapter and bounded copied-intent mailbox
submit native CMSG_REPAIR_ITEM on the world owner, with empty item GUID (all
carried items) and guild-bank byte zero. Human-selected NPC replaces nearest-NPC
discovery. Native Cata inventory order replaces donor weapon-first order. No
gold cheat, guild funding, discovery/travel or donor death-count reset is ported.
The default-off gate is `Playerbots.Maintenance.Repair.Enabled`; ordinary whisper
and party/raid `repair` share authorization and raid subgroup routing.

Execution rechecks gate, five-second expiry, attachment, human control, captured
map/instance and native NPC eligibility for both players. Bot combat/flight/cast/
teleport/logout/loading blocks execution. Only copied identifiers cross the
mailbox; native pointers remain scoped to the synchronous world update. The
result reports equipped missing durability and copper spent, including partial
repairs. Handler submission is not a claim of complete carried-item restoration.
Mailbox tests cover in-flight fencing, stale completion and timer wrap/expiry.
Windows full-script worldserver/tests-common built and passed 433/433, including
both new mailbox tests. Evidence: build-both/Testing/Temporary/LastTest.log.
Linux and client validation of this follow-up remain pending; the published
October 9 Linux 431/431 result below applies to its earlier source snapshot.

## Human-led party/quest release qualification — 2026-10-09

This checkpoint packages the preceding gear/loot, native quest and passive
diagnostic work. Windows full-script worldserver/tests-common passed 431/431.
The complete Linux snapshot was refreshed with current core and Playerbots
sources plus the canonical optional context module, explicitly reconfigured
to discover new tests, and built with Ubuntu 22.04/GCC 11.4 normal PCH. It passed
431/431; evidence is build/linux-release-20261009/LastTest.log and the compiler
container stopped. Fresh Windows with all three installed optional modules off
also built worldserver/tests-common and passed 19/19. The companion reader suite
passed 43/43. These replace older partial snapshot/build qualifications below.

The October 8 real-quest replay establishes four native ordinary shares and
objective completion, with one explicit bot turn-in and saved reward presence.
It does not establish automatic travel/turn-in, relogin, batch/link/party-pushed
confirmation gameplay, new per-player exemption behavior or Linux realm runtime.
Public README and core milestone record retain those limits. The exporter/query
project is separate and is not bundled by the core/Playerbots publication.

Outgoing source, docs and new history were reviewed for portable paths, runtime
assets, private identifiers/credentials and donor attribution. No captured
client data, copied databases, logs or binaries are release assets. Publication
uses the verified iotadev author and authenticated account, with ordinary
fast-forward updates. AHBot source is unchanged and matches its published pin;
only its local metadata/docs differ, so no AHBot update is included.

## Per-player quest-loot exemption and closed quest replay — 2026-10-08

The human correctly noted that shared quest drops need no master-first delay.
Native LootItem::freeforall, derived from ITEM_FLAG_MULTI_DROP, makes pickup
per-player: Player::StoreLootItem marks that player's quest/FFA record and does
not consume the global item for other eligible players. It is not the group's
FREE_FOR_ALL distribution setting and does not imply a transfer/reservation.

The existing world-owned corpse path now resolves each allowed/owner native
LootListID through LootItemInSlot for the bot, including native quest-slot mapping.
Only a matching item entry with actual freeforall=true exempts the slot from
master quest priority. Unknown/missing slot facts retain conservative deferral;
competitive quest items still defer while human need remains. Native loot
visibility, admission, inventory and grant rules are unchanged. Only the copied
boolean survives the local lookup; no live loot pointer is retained across a
handler call or thread boundary. The regression covers per-player exemption
versus competitive drops, disabled gate and satisfied human need.

The completed copied replay build/playerbot-smoke-20261008-215311 recorded all
four native accepts of quest 9156, combat/corpse looting and completed objectives.
Testone's explicit reward 22979 was confirmed; after clean world/database shutdown,
native rewarded history includes Testone and the human, and Testone's mapped
owned inventory has one 22979 (none in the before report). The other three bots
remain complete and unrewarded. This confirms one native explicit turn-in and
saved reward presence, not all four turn-ins, exact item properties or relogin.
Old priority deferred Testtwo's head 22893 while the human needed it; the new
per-player exemption was not deployed during that replay.

Linux prior reward-batch snapshot plus the exemption passed 425/425; compiler
stopped, evidence in build/linux-per-player-quest-loot-20261008/LastTest.log.
Concurrent observer history is outside that snapshot. Windows full-script
worldserver/tests-common passed 431/431. No additional realm was started;
fresh exemption gameplay is deferred
to a normal future replay, not a separate mandatory test.

## Native quest-share availability correction — 2026-10-08

The copied Tranquillien replay used native quest 9156, Wanted: Knucklerot and
Luzran, from actual Wanted Poster 181147. The human acquired it and all four
bots processed `quests`; the five identities shared a group/map/instance and
the optional phase diagnostics reported mutual peer visibility. No bot share
acceptance was logged; saved state after clean shutdown retained 9156 only for
the human. This is a failed sharing checkpoint, not a completed quest replay.

Native Player::CanShareQuest rejected IsQuestActive=true, while the pool manager
returns true both for active pooled quests and ordinary non-pooled quests.
Corrected only the rejection polarity to !IsQuestActive. Existing sharable-flag,
active-log lookup, native recipient admission and unavailable-pool response remain.
This affects human sharing and bot sharing even with modules disabled; it is not
a module bypass or donor forced-completion feature. The service-free regression
checks the rejection and actual pool-manager contract alongside retained guards.

Linux full-script worldserver/tests-common passed 424/424; compiler stopped and
evidence saved in build/linux-native-quest-sharing-20261008/LastTest.log. Windows
validation initially hit C1041 after overlapping builds wrote the same game.pdb.
The synchronized serial retry then passed full-script worldserver/tests-common
with 427/427 tests, including concurrent observer additions in the active tree.
The preceding module-off 19-test build predates this core fix.
The build helper now owns an exclusive per-output-directory file handle through
configure/build/tests. Service-free checks cover refusal of a second owner,
independent directories and reacquisition after release. Older running helpers
and direct CMake are not covered. The quest-gate test now selects its actual AST
assignments, not their old first-two position, preserving new observer resets.
Windows retries serialize writers and use process-local /FS synchronization;
no generated PDB was deleted and no global toolchain environment was changed.
The Linux check applied this Player.cpp correction to the previously qualified
reward-batch snapshot. Concurrent observer action-history changes in the active
checkout are not qualified by that older snapshot's 424 tests. The synchronized
Windows retry passed. Corrected realm attempt build/playerbot-smoke-20261008-214726
aborted when another database claimed port 13306 during the copy/start interval;
the attempt's owned processes stopped. No corrected realm is ready and no bot
sharing or subsequent quest progression is observed yet. Do not repeat against
shared ports until test ownership is coordinated.
The user authorized coordination with the observer task, which released shared
build output and selected separate runtime ports. Corrected quest fixture
build/playerbot-smoke-20261008-215311 is ready on the ordinary Playerbots ports,
with the human's saved quest retained. Client sharing remains pending.
Corrected sharing still requires the resumed native client check; no acceptance
or quest-item/reward outcome is inferred from the source repair.

## Selected-giver unambiguous reward batch — 2026-10-08

Adapted from TalkToQuestGiverAction::TurnInQuest/RewardNoItem/RewardSingleItem at
remotely rechecked master `037c01418b5d01506917a3db9b44fd56ac5f965c`.
Explicit `reward *` reuses the default-off Reward gate and selected-giver lease.
It snapshots at most 25 native active quest IDs, then re-resolves bot, human and
giver and rechecks control, location, state, gate and five-second age per quest.
Only ordinary completed, unrewarded, non-repeatable/non-turn-in quests involving
that giver qualify. Zero choice uses native slot zero; a single well-formed
choice retains its actual Cata sparse slot. Multiple choices, malformed count/
entry pairs and missing selected-item metadata are skipped, not ranked.

Both native CanRewardQuest checks and the typed Cata reward handler remain the
grant path. The donor's direct RewardQuest and forced CompleteQuest/master
progress synchronization are not copied. Callback-driven cast/transfer/death or
authority changes stop further attempts; partial results have no rollback,
automatic retry or chain rescan. Native rewarded-status transitions are counted,
not proof of exact reward contents, persistence or relogin. A batch intentionally
changes quest/reward/inventory state and is not an inspection command.

Three regressions cover exact wildcard scope, giver/zero-operand mailbox fencing
and zero/single/multiple/malformed choice slots. Linux full-script worldserver/
tests-common passed 424/424; compiler stopped, with evidence saved in
build/linux-quest-reward-batch-20261008/LastTest.log. Windows full-script
worldserver/tests-common also passed 424/424; no compiler remains running.
Environment and seven wait-policy checks passed without services. No realm started.
Use the next normal quest replay for client
coverage, with manual `reward <quest> <item>` retained for multiple-choice quests.

## Read-only donor quest-list filters — 2026-10-08

Adapted from ListQuestsAction::Execute/ListQuests at remotely rechecked master
`037c01418b5d01506917a3db9b44fd56ac5f965c`. `quests completed`/`co`,
`quests incompleted`/`in`, `quests all` and `quests summary` use the existing
default-off Inspection gate, world-owner native active log and bounded mailbox.
Completed means native QUEST_STATUS_COMPLETE; donor incompleted means not
complete, so failed entries remain included and visibly labelled. Summary counts
the whole active log regardless of the selected detail filter.

Bare `quests` retains this port's detailed output for compatibility; the donor
defaults to summary. Travel/detail and travel-manager dependencies are not ported.
Appended internal inspection operation IDs preserve existing operation values;
all inspection variants require zero quest/item operands and the same fresh
control/location checks. No core hook signature or quest mutation changes.
Two regressions cover exact command aliases, detail selection, zero operands and
mailbox fencing. Linux full-script worldserver/tests-common passed 421/421;
compiler stopped and evidence saved in build/linux-quest-list-filters-20261008/LastTest.log.
Windows full-script worldserver/tests-common also passed 421/421.
Environment and seven wait checks passed without services.
No realm started;
filtered client output remains unobserved and can join the normal quest replay.

## Narrow human quest-loot priority — 2026-10-08

Adapted from ItemUsageValue::Calculate at upstream master
`037c01418b5d01506917a3db9b44fd56ac5f965c`, rechecked for this batch. The donor
defers master-needed quest items after its earlier skill/key/consumable/equipment
categories. This slice covers only native quest-class corpse items, not the
complete item-usage ordering, all item classes or bag/container loot.

The independent default-off Playerbots.Quest.SyncLootWithPlayer.Enabled gate
checks the current human controller's native HasQuestForItem on the existing
world-owned corpse handler path. It skips only that bot's autostore attempt from
the native allowed/owner view. Money, slot permissions, blocked/master loot,
inventory admission and release retain native handling. There is no reservation,
transfer, guaranteed human eligibility, roll override or copied quest progress.
Once native human need is satisfied, the next eligible pickup is not deferred.
The map-owned own-bot Useful reader is not called on this world-handler path.

Two policy regressions cover opt-in/class/need and satisfied-need transition;
they do not prove live quest-item pickup. QuestFixture opts into this gate in the
copy; ordinary replays reset inherited values off, and abandonment remains separate.
Linux full-script worldserver/tests-common passed 419/419; its compiler stopped,
with evidence in build/linux-quest-loot-priority-20261008/LastTest.log. Windows
full-script worldserver/tests-common also passed 419/419. Fixture environment,
seven wait-policy and four controlled-loot branch checks passed without services.
Core/API source is unchanged; the preceding module-off
19-test result remains applicable. No realm started or live deferral observed.

## Selected-giver bounded accept-all — 2026-10-08

Adapted from donor AcceptAllQuestsAction and QuestAction::ProcessQuests at current
master `037c01418b5d01506917a3db9b44fd56ac5f965c`. Explicit `accept *` uses the
existing default-off AcceptNpc gate and captured selected giver/map/instance.
It prepares the bot's native quest menu and copies at most 25 available-icon
quest IDs before any accept callback. Existing/involved quests and turn-in
dialogs are not auto-rewarded or treated as offers. No nearby-NPC discovery,
quest progress copy, forced completion or direct AddQuest fallback is included.

Each candidate resolves fresh player/requester/giver objects and rechecks control,
attachment, native interaction, map/instance, request age and player state. Native
typed acceptance retains eligibility/capacity/source-item/spell/script ownership.
Casts/transfers/death/control changes or five-second expiry stop the batch; partial
results are counted, not retried or rolled back. Menus are not iterated after they
may have changed. Newly eligible chain quests are not rescanned automatically.
No live game objects are retained across iterations without resolution/recheck.

Two regressions cover the exact wildcard scope/trailing text and zero-single-ID,
required-giver, one-mailbox intent. This is explicit human direction, not automatic
quest acquisition or a native atomic transaction. Linux full-script
worldserver/tests-common passed 417/417; compiler stopped, with evidence in
build/linux-quest-accept-all-20261008/LastTest.log. Windows full-script
worldserver/tests-common also passed 417/417; no compiler remains running.
No core/API change; preceding module-off 19 tests remain applicable. No realm started.

## Native quest/item hyperlink command compatibility — 2026-10-08

Current donor quest actions extract quest/item links. This transport slice reuses
TrinityCore ChatCommands::Hyperlink framing with strict module-side first-ID
storage instead of a new link grammar or legacy truncating stoull conversion.
Accept/share/drop support a numeric quest or typed quest link; reward supports
numeric/typed quest and item operands, with numeric zero retained for no-choice
intent. Share/drop remain whisper-only. Authority, giver, admission, state,
native choice resolution and separate destructive opt-in remain unchanged.

The parser preserves original markup (rather than lowercasing |H along with the
verb), bounds the full command to 1024 bytes, gives native framing a complete
NUL-terminated operand, checks color bytes and token boundaries, rejects wrong
types/overflow/embedded NUL/trailing text, and accepts only exact commands. Link
names/levels and item affix/enchantment data supply no authority or native-instance
proof; only the entry ID is intent. Native handlers still resolve actual metadata.
Numeric parsing helpers remain for their regression coverage and compatibility.

Two regressions cover mixed numeric/link operands, title spaces, case and no-choice
zero, plus malformed/wrong-type/overflow/trailing/oversized/NUL input. This is normal
whisper/party transport compatibility, not a new LANG_ADDON protocol or MultiBot
quest UI feature. Linux full-script worldserver/tests-common passed 415/415;
compiler stopped, with evidence in build/linux-quest-links-20261008/LastTest.log.
Windows full-script worldserver/tests-common also passed 415/415; no compiler
remains running. Core code/API are unchanged;
the previous module-off 19-test pass remains applicable. No client replay started.

## Explicit active quest abandonment — 2026-10-08

Adapted from DropQuestAction at verified upstream master
`037c01418b5d01506917a3db9b44fd56ac5f965c`, using native Cata abandonment instead
of its direct SetQuestSlot/TakeQuestSourceItem/RemoveActiveQuest operations. The
default-off independent Playerbots.Quest.Abandon.Enabled gate admits only
`drop <numeric active quest ID>` by whisper to one controlled bot. No party
fan-out, wildcard/title matching, obsolete-quest cleanup or rewarded-history
reset is ported. Completed history without an active slot cannot be deleted.

The existing copied-intent/expiry/authority/state/map/instance checks apply.
Execution resolves and rechecks the current native log slot, then calls typed
QuestLogRemoveQuest. Native equipment/source-item eligibility, timed/PvP state,
quest-granted item removal, scripts, achievement and persistence behavior remain
owned by Cata. Abandonment may remove quest-provided items; ordinary requirement
drops retain native rules. Confirmation means active-slot removal, not proven
exact item preservation, saved state or relogin. No abandonment was executed.

Two regressions cover single numeric intent/no reward data and operation/serial
fencing. QuestFixture explicitly keeps abandonment off even while other quest
controls are enabled; a separate -QuestAbandon opt-in requires QuestFixture.
The environment tests verify inherited enabled abandonment is removed and that
the separate opt-in is honored. Linux full-script worldserver/tests-common passed
413/413; its compiler is stopped, with evidence in build/linux-quest-abandon-20261008/.
Windows full-script worldserver/tests-common also passed 413/413; no compiler
remains running. Core files
are unchanged and the previous module-off 19-test pass remains applicable.

## Explicit outgoing party quest share — 2026-10-08

Adapted from ShareQuestAction at verified upstream master
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Default-off
Playerbots.Quest.Share.Enabled admits `share <numeric quest ID>` by whisper to
one controlled bot. No party-chat fan-out or automatic share loop was introduced.
The existing mailbox rechecks current full control, attachment, map/instance,
alive/out-of-combat state, a shared non-battleground group, active-log ownership
and CanShareQuest. It expires after five seconds and needs no giver/item intent.

Execution uses Cata's native HandlePushQuestToParty uint32 input, not the donor's
typed WotLK packet. Recipient eligibility, capacity, busy and auto-accept quest
flags stay native. Submission is not acceptance/persistence. Incoming bot-origin
offers are not auto-consented by the human-only AcceptShared adapter; no new
bot-to-bot delegation, quest add, progress copy or core hook was introduced.

Two regressions cover parsing/signed bounds, giver-free intent and operation
fencing. QuestFixture resets all five gates. Linux full-script worldserver/tests-common
passed 411/411; its compiler stopped, with ignored evidence in
build/linux-quest-outgoing-share-20261008/LastTest.log. Windows full-script
worldserver/tests-common also passed 411/411; no compiler remains running.
the preceding module-off 19-test check applies to unchanged core files. No realm
was started. Outgoing offer/client acceptance/persistence remain unobserved.

## Party-pushed quest confirmation and native identity repair — 2026-10-08

Donor QuestConfirmAcceptAction at verified master
`037c01418b5d01506917a3db9b44fd56ac5f965c` responds through the native confirmation
handler. The existing AcceptShared gate now routes actual pending party-pushed
acceptance to Cata's typed QuestConfirmAccept; ordinary shares retain the typed
QuestGiverAcceptQuest route. Party pushes require the native flag and active quest
on the controlled human sharer, not a manual-shareable flag. Turn-in dialogs and
signed confirmation-ID overflow remain unsupported. No synthetic invitation,
WotLK wire layout, direct AddQuest or duplicate source-spell cast was introduced.

Source review found HandleQuestConfirmAccept clearing both sharing fields before
its sharer lookup. Local blame attributes that ordering to a3dd0d75cfe (2023-07-11),
not recent module work; this says nothing about the latest remote core. The narrow
repair copies the GUID before clearing and uses the copy for lookup. Native
pending-ID, same-raid, original-active-quest, CanTakeQuest and CanAddQuest checks
are unchanged, as are native source-spell/script semantics. The repair affects
human confirmations too, even with modules disabled.

Two routing regressions cover manual/party eligibility, active sharer, turn-in
and signed bounds. The environment suite separately checks capture/clear/lookup
source order and native admission-guard presence; this is not a player/session
gameplay test. Linux full-script worldserver/tests-common passed 409/409 tests;
compiler stopped, with ignored evidence in build/linux-quest-confirmation-20261008/.
Windows full-script worldserver/tests-common also passed 409/409; fresh Windows
all-modules-off passed 19/19. No compiler remains running. Human/bot confirmation
and saved/relogin outcomes remain unobserved. A real party-pushed quest may join
the later bundled replay when available; do not force one to close coverage.

## Native active quest inspection — 2026-10-08

Adapted from current donor ListQuestsAction at verified upstream master
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Default-off independent
Playerbots.Quest.Inspection.Enabled admits the exact `quests` whisper/party
command through the existing bounded quest mailbox. It lists at most 25 active
native slots, quest IDs/titles/status, six carried-item requirement counts, four
native kill/GO counters and up to six reward item IDs/counts, followed by a summary.
Titles use the native UTF-8 truncation helper. Carried counts exclude bank and are
not an invented quest-objective tally; status comes from native GetQuestStatus.
Currency, exploration, player-kill/timer and other conditions are not detailed.
Donor filters/travel descriptions, localization and quest links are not ported.

Read inspection requires current full control, attachment, same captured map/
instance and non-transfer/loading/logout state. It may read during combat/death
or a pending share dialog; it never clears that dialog or invokes accept/reward
handlers. The mutation-only alive/out-of-combat/cast guards remain unchanged.
The existing core forwarding function now permits the module-validated zero
quest/giver read envelope; no public header or new session hook was added. A
domain/mailbox regression prevents inspection from carrying quest/reward intent.
This is world-owner native data, not a map-engine read or external API expansion.
QuestFixture explicitly resets all four quest gates including Inspection, and
the existing environment tests check inherited enabled settings for all four.
Windows/Linux full-script worldserver/tests-common built and 407/407 tests passed.
Linux compiler is stopped; ignored evidence is
build/linux-quest-inspection-20261008/LastTest.log. Fresh Windows all-modules-off
worldserver/tests-common passed 19/19 for the forwarding implementation change.
No compiler remains running. No realm was
started or live read qualified.

## Explicit native reward execution (uncommitted)

Source basis remains RewardAction::Reward / TalkToQuestGiverAction at verified
master `037c01418b5d01506917a3db9b44fd56ac5f965c`. The default-off
Playerbots.Quest.Reward.Enabled gate admits whisper/party
`reward <quest ID> <item ID>` from the controlling human at a selected nearby
quest giver. Item zero means no choice item only for an actual zero-choice quest.
Expected item intent is resolved against the current six native slots; missing,
ambiguous duplicate entries and malformed item/count pairs reject. A cached
recommendation is not consulted or treated as reward authorization.

The unpublished acceptance-only bridge is generalized to a copied quest-command
operation/item envelope, with module-side validation. Acceptance still requires
item zero; reward quest IDs must fit the signed Cata packet field. Acceptance
and reward share one mailbox/serial, five-second expiry and giver/map/instance
binding. Existing full-control, attachment, alive/out-of-combat, transfer/loading/
logout and native interaction checks are retained. Reward requires an involved
giver, ordinary non-repeatable/non-turn-in quest, native completed state and both
CanRewardQuest overloads. The typed QuestGiverChooseReward handler owns item/money/
XP grants, item consumption, quest scripts and native persistence. The module
does not call RewardQuest directly, force completion or invent inventory space.
Already-rewarded quests do not invoke a second grant. Confirmation means the
native rewarded predicate changed, not proven item contents or saved/relogin state.

Four regressions cover strict item-intent parsing/signed quest range, shared
mailbox operation fencing, sparse native slot resolution and ambiguous/malformed
choices. Auto-selection, links, automatic completion, repeatable/daily/turn-in
flows and progress/master synchronization remain separate. QuestFixture now
explicitly resets all three quest gates, including Reward, on every copied
configuration. Windows/Linux full-script worldserver/tests-common built and
406/406 tests passed. Linux compiler is stopped; ignored evidence is
build/linux-quest-reward-execution-20261008/LastTest.log. Fresh Windows all-three-
modules-disabled worldserver/tests-common passed 19/19 after the bridge change.
No compiler or realm remains running.

The existing QuestFixture now also captures bounded rewarded history and native
inventory-mapped item totals (including bank), level/XP/money before admission
and after clean shutdown in quest-reward-state-result.json. Orphan item rows do
not count. Mocked reader tests cover empty/valid/malformed/over-limit rows,
ownership joins, numeric range and invalid GUID scope. This is comparison evidence,
not an automatic reward assertion: natural gameplay can alter counts/resources,
and item properties/relogin remain separate. No reward has been live-qualified.

## Read-only quest reward-choice dependency — 2026-10-07

Verified upstream master is still `037c01418b5d01506917a3db9b44fd56ac5f965c`.
This batch adapts TalkToQuestGiverAction::BestRewards/RewardMultipleItem: useful
equipment/replacements outrank BadEquip, then the donor's first non-None category
in native choice order supplies the candidate set. Existing template equipment
comparisons and shared item-usage readers provide the Cata facts; no new weight
table, vendor classification or pretend native reward instance was introduced.

The map-owned calculated `quest reward choices` value takes a strict numeric quest
qualifier and publishes copied owner/quest/choice facts plus ranking. It reads all
six Cata slots, retains sparse native slot indices, and refuses missing item/count
rows rather than silently compacting them. Survey availability means readable
choice metadata, not reward eligibility or completed quest state. Selection Ready
means the policy has enough facts; a single mandatory choice needs no usage ranking.
For multiple choices, Unknown is not donor None. Missing/non-finite scores leave
multiple qualified candidates unresolved; ties use the lowest native candidate
slot. The donor's zero-score fallback to slot zero is not copied when that slot
is absent from the qualified set.

Existing starter template admission may create/delete a transient native Item and
consume an item GUID; it never stores/saves a reward item. Cached facts remain
map-owned advice, not freshness/authorization for a later reward mutation. Native
giver, quest completion, choice index/count, inventory capacity/uniqueness and
quest scripts must be revalidated by the eventual execution path. This batch has
no chat/addon reward command, auto-turn-in, source-item completion, RewardQuest,
database write or core bridge change. Progress/master synchronization remains
separate. Four tests cover zero/single/sparse slots, donor category precedence,
ties/zero-score indices, malformed/unknown and incomplete score inputs.
Linux full-script worldserver/tests-common built and 402/402 tests passed;
ignored evidence is build/linux-quest-reward-choices-20261007/LastTest.log.
Its compiler container is stopped. Windows full-script worldserver/tests-common
also passed 402/402 tests. No compiler or realm remains running. The preceding
module-off 19-test build is unchanged: this
dependency adds no core header/API or runtime reward action.

## Explicit selected-giver NPC acceptance — 2026-10-07

Upstream master remains `037c01418b5d01506917a3db9b44fd56ac5f965c` (remote
rechecked). This batch adapts AcceptQuestAction and QuestAction::AcceptQuest.
Default-off Playerbots.Quest.AcceptNpc.Enabled admits `accept <numeric quest ID>`
through ordinary bot whisper or existing party/raid command scope. The human's
selected creature/gameobject supplies the giver identity; there is no NPC scan,
accept-all, item/self giver, quest-link parser or autonomous travel in this slice.

One copied request per bot carries requester, quest, full giver GUID, map/instance,
creation time and serial. The thin optional session bridge only posts intent;
the existing world-owner update consumes it. It expires after five seconds and
rechecks human identity, full control, attachment, map/instance, alive/idle/transfer
state, an empty native share dialog and both players' native interaction with the
actual quest giver. The giver must offer that quest. Cata's typed accept handler
retains CanTakeQuest/CanAddQuest, source-item/spell and quest-script ownership.
No WotLK wire layout is copied. The donor's syncQuestWithPlayer direct AddQuest
fallback after native failure is deliberately not ported: rejected native
acceptance remains rejected in this fork.

Queued receipt is not completion. Native quest-log slots distinguish newly
accepted, already present and rejected outcomes. Requests hold no Player/Quest/
giver pointers and are not retried after denial. A pending native shared dialog
is left untouched, not replaced by the explicit NPC command. Three regressions
cover strict numeric parsing, copied identity/capacity/completion fencing and
expiry across timer wrap. Authority and native acceptance are source-reviewed,
not live-qualified. Windows full-script worldserver/tests-common built and
398/398 tests passed. Linux full-script worldserver/tests-common also passed
398/398; its compiler container is stopped, with ignored evidence in
build/linux-quest-npc-20261007/LastTest.log. Windows all-three-modules-disabled
worldserver/tests-common passed 19/19. This session bridge changes a common header and therefore
triggers a wider rebuild. The
preceding ordinary-share slice passed 395 tests on both platforms.

The existing copied-realm harness now supports opt-in -QuestFixture for the
reused interactive recovery/mixed-party recipe. Both acceptance gates are
explicitly reset on every generated configuration, disabled unless that option
is selected. The actual reset prefix is exercised in memory by the existing
environment test, including duplicate/indented enabled seed entries; syntax,
environment and the existing seven wait/four controlled-loot checks passed.
No native realm was started. Fresh module-disabled validation of the new core
session bridge passed; no compiler remains running.

Opt-in QuestFixture now records saved quest presence/status before admission and
after clean native world shutdown, with a 125-row bound for the five-character
roster. It captures complete/incomplete/failed states without editing them. The
existing environment tests exercise empty/valid/malformed/over-limit rows and
invalid GUID scope using mocked read-only SQL. Native quest persistence/relogin
remains unobserved; no forced acceptance assertion or item/quest grant was added.

## Ordinary human-controller quest shares — 2026-10-07

Adapted from upstream AcceptQuestShareAction at the verified master revision
`037c01418b5d01506917a3db9b44fd56ac5f965c`. The new default-off
Playerbots.Quest.AcceptShared.Enabled setting permits an admitted bot to respond
to an existing ordinary native pending quest share. The share itself remains
owned by Cata's player/session state; there is no synthetic invitation, outgoing
packet interception, new core hook or direct quest/database edit.

The existing world-owner session update checks the current human sharer, native
group/map, attachment and full control authority; the bot must be alive, idle,
not loading/logging out, flying or transferring. It resolves the quest and
requires native sharer eligibility. Cata's typed QuestGiverAcceptQuest handler
then retains interaction, CanTakeQuest/CanAddQuest, sharing response, source-item/
spell and script ownership. Unlike the donor, this adapter does not call AddQuest
directly or copy WotLK packet layouts. The bounded receipt identity permits one
attempt per observed pending share and resets when native sharing state clears.

Outcome logging/replies distinguish a newly observed active quest from already
present and not accepted. Receipt or a handler call is not reported as success.
Ordinary shares only: pushed-to-party acceptance confirmations and turn-in dialogs
are explicitly excluded. NPC discovery, quest rewards, master progress syncing,
automatic loot quest policy and autonomous questing remain separate ports.
Unsupported/unauthorized native pending dialogs are left untouched rather than
silently accepted or deleted; handling their decline/timeout UX is later work.

Two receipt regressions cover duplicates, changed share identities, clearing and
later identical offers. Native authority/admission and persistence are source-
reviewed, not live-qualified. Linux full-script worldserver/tests-common built and
395/395 tests passed; ignored evidence is build/linux-quest-share-20261007/LastTest.log.
The compiler container is stopped. The first compile exposed a WotLK HasQuest
assumption; completion now uses Cata FindQuestSlot against MAX_QUEST_LOG_SIZE.
The corrected Windows worldserver/tests-common build also passed 395/395 tests.
No compiler remains running. The preceding own-quest/
consumable dependency passed 393 tests on each platform.

## Own-quest item usefulness dependency — 2026-10-07

Upstream master was rechecked and remains
`037c01418b5d01506917a3db9b44fd56ac5f965c`. This batch adapts
`ItemUsageValue::IsItemUsefulForQuest`: native HasQuestForItem first (including
quest source items), then the donor carried-count fallback for active-log direct
objectives and item spells with SPELL_EFFECT_CREATE_ITEM. Cata uses six item
objectives rather than the donor's four, and ItemTemplate::Effects rather than
the WotLK fixed spell array. Only matching objective IDs trigger inventory counts.
These are map-owner reads, with no cache mutation, database write or spell cast.

The existing `item usage` value can now return Quest with a distinct QuestLog scope
for non-affixed items reaching its previously unsupported fallback. Useful
consumable stock (Use/Keep) and equipment decisions retain precedence; exhausted
or unsupported consumable stock can now reach quest usefulness, covering quest
consumables. If the quest predicate is false, the existing stock result is retained;
other unsupported item classes remain Unknown, not None. Master quest
synchronization, container-loot exceptions,
quest acceptance/sharing/reward and profession/economic precedence are not ported.
This is not a complete copy of donor ItemUsageValue::Calculate.

The native roll adapter is unchanged: QuestLog is not native-loot eligibility or
equipment proof. No new need vote, autonomous loot policy or permission bypass is
introduced. Three tests cover outstanding objective/count rules, the existing
pass policy for quest facts and consumable fallback precedence. Native source/
create-item integration is source-checked, not live quest-qualified. The initial
Windows dependency build passed 392 tests. With the consumable fallback included,
Linux worldserver/tests-common built and 393/393 tests passed; evidence is ignored
build/linux-quest-items-20261007/LastTest.log. Its compiler container is stopped.
The corresponding Windows incremental build/test run also passed 393/393.

The connected slices recorded above now use donor AcceptQuestAction/
AcceptQuestShareAction semantics and native Cata admission, not direct database
quest edits or copied WotLK wire layouts. Source review identified Cata's typed
QuestGiverAcceptQuest and QuestConfirmAccept handlers and native sharing state;
receiving a share or command alone must not be treated as completed acceptance.
Quest rewards, NPC discovery/travel and autonomous questing remain separate work.

## Read-only context companion integration — 2026-10-07

The existing companion core/Playerbots patches were reconciled into the current
gameplay source, preserving the affix extension. A generic map-session player
update hook dispatches after the bot update; optional diagnostics copy engine
state/strategies, active-engine last-action value, staying and queue count. The
getter is map-owner-only and independent of strategy mutation gates. The
canonical exporter remains in cata-context-api/server-module, linked into this
checkout as optional mod-context-api. It copies scoped native state to a cache
and publishes JSON from a background writer; external readers retain no game
pointers. Runtime capture defaults off.

The current full-party harness supports explicit -ContextCapture, writes a
disabled config otherwise, and captures the controller plus four bot identities.
The companion's 17 reader/interface tests passed again. Windows full-script
worldserver/authserver/tests-common and Linux full-script worldserver/tests-common
built successfully, with 390/390 native tests on each. Windows with all three
optional modules disabled passed 19/19. Linux compiler was stopped. The approved
five-member copied replay completed in build/playerbot-smoke-20261007-164901/.
All five shared Ragefire instance 1; native logs recorded Mage/Warrior actions,
Priest healing, target-death transitions, corpse opening and a drink start.
The harness exited zero and stopped its owned services; all five had combat and
offline observations and the capture published stopped status. The monitor also
counted 434 stale online entity polling observations, with maximum sample skew
4034 ms. This is not full freshness qualification, and native sampling cost is
unmeasured. The totals cannot establish a cause. A bounded external anomaly
reporter now retains timing/age/status evidence for a later ordinary session;
23 Python tests passed, including six reporter regressions. No gameplay policy
or native hook was changed for that diagnostic addition.
Existing API field meanings and limits remain in the canonical
snapshot contract; selected target/last action do not prove a current attack or
landed spell, and per-entity sampling is not an atomic party snapshot.

## Native random-affix roll comparison — 2026-10-07

Donor ItemUsageValue::QueryItemUsageForEquip and StatsWeightCalculator affix
scoring remain pinned at current master `037c01418b5d01506917a3db9b44fd56ac5f965c`
(remote verified for this batch). Cata's ItemEnchantmentMgr provides native
property/suffix type semantics and GenerateEnchSuffixFactor. The roll adapter
passes verified copied metadata to the shared unowned comparison reader.
The ordinary template query remains non-affixed; no string qualifier can grant
the new native-loot proof.

Properties normalize to positive signed IDs, suffixes to negative IDs. Invalid
types, incompatible/both template modes, missing generated affixes, signed-range
overflow and mismatched/zero suffix factors are rejected. Existing collector
lookups must resolve the enchantment and all required effects; unknown/proc/use/
conditional/socket/set inputs still cannot score. AffixLootVerified is applied
only to a local copied stat fact, independently of owned-instance proof, and
never written into the cached hypothetical value. The native group fence still
binds the actual blocked loot entry/count/affix and revalidates before voting.

Comparison rows copy the normalized property and factor without implying an
owned item GUID. Native slot admission and current owned scoring remain in use.
Same-entry copies count as the same variant only when property/factor also match,
allowing a better affix on the same base item to qualify as an upgrade. This
correction applies to both carried and unowned comparisons. No weighting table,
default gate, core API or native inventory/loot persistence was replaced.

Four regressions cover positive property mapping, negative suffix/scaling,
malformed/absent metadata and same-entry variant upgrades. The existing score
test verifies local loot proof cannot alter cached facts or admit proc effects.
Windows/Linux worldserver/tests-common built and 388/388 tests passed on each;
Linux evidence is ignored `build/linux-loot-affix-20261007/`. Compiler container
is stopped. Natural-affix decisions/awards remain unobserved and can join the
next human-led dungeon; no isolated playtest was run. This batch is uncommitted
after the closed 384-test milestone.

## Gear/loot milestone qualification and review — 2026-10-07

The complete local source passed Windows and Linux worldserver/tests-common
builds with all 384 checks. Linux used the existing native Ubuntu 22.04/GCC 11.4
Release snapshot, normal PCH and both optional modules enabled; its compiler
container was stopped after success. The refreshed snapshot builds without Git
metadata, so validation identifies the copied source rather than a binary Git
revision. Full test evidence is retained in ignored
`build/linux-native-loot-20261007/LastTest.log`.

The current Windows core-only worldserver/tests-common build also passed all
19 checks with both optional modules disabled. No Linux server runtime was run.
The controlled supported need/greed/pass and saved-award replay below completes
the runtime check for this milestone; it does not establish full item/class or
dungeon parity. Test realm, native database and compiler container are stopped.

Review covered the core hooks, native group fence, map/world mailboxes, instance
and authority checks, score/unknown-input boundaries, new files, fixture-only
mutations and outgoing documentation. No new blocker was identified. The targeted
credential/host scan found only the existing synthetic localhost fixture
credentials, with no personal host identity or API credential in the scanned
outgoing implementation files. This is a bounded milestone review, not a complete
security audit. Existing gameplay limits remain recorded below.

## Controlled native need/greed fixture and temporary eligibility — 2026-10-06

The next copied harness replay supports explicit `-ControlledLootRoll` alongside
the existing reused mixed-party recovery/dungeon/roll recipe. It validates both
offline Warriors' nonbroken chest 2866, one native Oggleflint spawn in Ragefire,
and an unused fixture loot ID before any mutation. In the copy only, a transaction
adds a 100% single chest drop and changes that creature template's loot source.
Original loot rows remain intact and the prior source is recorded in ignored JSON.
Four mocked checks execute the actual preparation branch and verify rejection
of invalid gear, ambiguous spawns and occupied loot IDs before SQL mutation.

Default-off Dev.LootRollFixture.Enabled requires the existing Dev/Fixture20 gates.
Native map-owned role preparation preserves Testone's original chest in carried
inventory and leaves the slot empty; Testtwo retains the known equipped chest.
The normal native item/template reader must admit the non-affixed uncommon item.
One normal boss kill should therefore exercise Testone need, Testtwo greed and
caster pass. The human passes to make the expected native award deterministic.
This intentionally tests an empty-slot decision while preserving existing gear;
duplicate-stock avoidance and automatic equipment management are separate work.
After logout, the harness records both vote submissions and the persisted stock
increase by one. Runtime acceptance requires all three, not just submission logs.

Native CanEquipNewItem rejects armor during combat and native cast locks.
The adapter now leaves evaluation queued during those temporary states; mailbox
expiry/world retry can refresh the request while the native roll remains alive.
It does not bypass equip admission or interrupt combat, casting or recovery.
The existing expiry regression now covers successful fresh evaluation after an
expired request. Windows worldserver/tests-common built and 384/384 tests passed;
four mocked SQL-branch checks and seven harness wait-policy checks also passed.
Linux validation remains pending. The October 6 controlled replay completed,
with the player confirming expected behavior on October 7. All four submissions
refer to the same native roll: Testone need, Testtwo greed, both casters pass.
`controlled-roll-result.json` records Need=true, Greed=true and SavedAward=true;
Testone's saved count of chest 2866 increased from one to two. This qualifies the
controlled need/greed/pass decision and native award for this supported item,
including saved persistence. It does not establish broader item/class coverage
or relogin. World database pools closed and MySQL completed normal shutdown;
no native Cata service process remained at verification. Evidence is in ignored
`build/playerbot-smoke-20261006-233354/`. No assertion/fatal match was found.

## Connected optional native party rolls — 2026-10-06

Current donor master was verified as `037c01418b5d01506917a3db9b44fd56ac5f965c`.
LootRollAction::Execute provides the pending-vote iteration and item-usage policy.
The Cata adapter uses a single bounded request/reply mailbox: native world/group
code copies a pending roll, the map evaluates current item facts, and world
execution validates the same live roll before invoking HandleLootRoll.

Core Group helpers expose copied identities and guarded admission, not iterators
or pointers to the module. Facts bind group/roll, entry/count/slot, native random
enchantment type/ID, suffix factor and map/instance. Execution repeats membership,
controller attachment, identity, the current blocked loot entry/count/affix,
NOT_EMITED_YET and current
vote mask checks. Need Before Greed also repeats native CanRollNeedForItem.
The original client/CountRollVote behavior is unchanged; bots use its native
packet format (GUID, uint32 slot, uint8 choice) only after the additional guard.

Mailbox expiry is five seconds with wrap-safe arithmetic. It stays occupied
through map evaluation, accepts one completion, and drops late serials/replies.
World polling is limited to once per second and one outstanding roll per session.
Map reads require current controller/security, map/instance and native actor
eligibility; calculations reset the relevant cached survey/stock values.
Only non-affixed starter equipment and supported consumables are classified.
Unknown/unsupported facts, random-affix loot and disallowed choices fall back to
pass. No recipes/tokens/profession/disenchant or vendor/AH classification is added.
The initial policy uses donor need level 2, greed enabled and disenchant disabled.
Native admission and native winning-item storage remain authoritative.

Gate Playerbots.Loot.Rolls.Enabled defaults off and requires StarterScore.
PassOnGroupLoot takes precedence and disables the roll adapter. The optional
copied harness -LootRolls recipe enables the two necessary gates and disables
pass-on-group-loot in that copy, alongside normal recovery/dungeon play.
PB-ROLL reports native submission; a visible vote/award is still runtime evidence.
Five regressions cover bounded work, expiry/wrap, changed identities, duplicate/
late replies, refreshed native vote masks and cancellation. The final reviewed
Windows worldserver/tests-common build passed 384/384 tests. Linux, current
module-off validation and native party/loot acceptance remain pending.

The subsequent copied Ragefire replay (`build/playerbot-smoke-20261006-224959/`)
completed four-bot entry and normal party combat. Logs contain 12 PB-ROLL pass
submissions, one per bot on each of three roll identities (items 9749, 10401, 774).
No duplicate bot/roll submission appears. No need/greed or winning-item award was
observed. Harness exit zero, clean world/MySQL shutdown and no remaining native
Cata processes were confirmed. This establishes pass submission through the
adapter; full voting/award acceptance remains deferred. The next fixture should
use a controlled known supported drop in the disposable copy, preserving native
loot generation and rolling rather than granting an item directly to a bot.

## Unowned non-affixed template comparison — 2026-10-06

The donor QueryItemUsageForEquip creates a temporary Item for native slot
admission. Cata's existing Player::CanEquipNewItem performs that same dry run,
creating/deleting a transient Item without storing it in inventory or saving it.
The new `template equipment comparisons::<item ID>` reader uses FindEquipSlot
to select native candidate slots, then CanEquipNewItem for admission and the
existing native/stat-weight/broken-current comparison reader for decisions.
It does not bypass native uniqueness, skill or class checks. Transient allocation
can consume a native item GUID; no persistent item is granted or created.

Random-property/suffix templates and supplied nonzero affixes remain unavailable.
Coupled hand layouts and incomplete existing-instance inputs remain Unknown.
`template item usage::<item ID>` exposes the resulting typed TemplateEquipment
fact separately from carried usage, with no owned candidate GUID. The existing
StarterScore gate still defaults off. This is a hypothetical read-only comparison,
not proof of actual roll identity, permission to vote, or a native equip request.
No Group/Roll access, roll packet, loot action or new core hook is added.
Two regressions cover affix rejection and distinct template/owned identity.
Windows worldserver/tests-common built and all 379 checks passed for this
extension. Linux is queued; the last Linux pass remains the 372-test equip batch.
No client/runtime template-comparison check was run.

## Shared item-usage and read-only roll policy — 2026-10-06

Donor `ItemUsageValue.h`, `ItemUsageValue.cpp: Calculate/QueryItemUsageForEquip`
and `LootRollAction.cpp: Execute/CalculateRollVote` at
`037c01418b5d01506917a3db9b44fd56ac5f965c` provide the category identities and
roll-choice branches. The new `item usage::<item ID>[,<signed property ID>]`
value composes the existing consumable-stock and carried-equipment readers.
It returns a typed fact with explicit source scope; unavailable, unported and
incomplete inputs remain Unknown rather than donor None. An owned safe upgrade/
empty slot maps to Equip, broken-current replacement to Replace, repair-needed
candidate to BrokenEquip, and retained stock to Keep. Entry/property matching
does not imply proof about an unowned loot instance. Equipment still requires
the existing default-off StarterScore gate and qualified native survey.

The pure roll-choice helper ports need/greed/disenchant, recipe binding, unique
blocking and configured need downgrade. Its default policy passes; unknown facts
or invalid need level produce no decision. Unlike donor equipment branches,
the local helper applies supplied loot permission to all kinds, conservatively.
No configuration, strategy action, packet handler or Group/Roll mutation is
enabled. Quest/master synchronization, professions, token eligibility, bags,
vendor/AH and disenchant classification remain unported; category declarations
do not claim those readers exist. Master/free-for-all handling, native allowed
vote masks and membership/vote revalidation belong to the future native adapter.

Cata `CMSG_LOOT_ROLL` is PROCESS_THREADUNSAFE, unlike the PROCESS_INPLACE equip
route. A later roll adapter must execute through the native world/group owner,
not inspect or mutate live Group/Roll from the map update. It must resolve the
actual loot identity/affix and obtain qualified unowned-item usage first; the
carried-equipment fact is not sufficient. Native GroupHandler::HandleLootRoll
passes identity/type to Group::CountRollVote; the latter increments totals for
accepted choices without a NOT_EMITED_YET check or vote-mask check at this call
boundary. The bot adapter must recheck both on the owner thread and submit at
most once; session packet dispatch alone does not establish that safety.
No upstream group behavior was changed. Five regressions cover usage mapping,
signed-affix matching, incomplete inputs, uniqueness/downgrade and recipe/
disenchant rules. Windows worldserver/tests-common built and passed 377/377
checks before the template extension above. Linux qualification remains queued.

## Explicit single-step native starter equip — 2026-10-06

The copied outdoor check subsequently passed: Testone reported native completion
(`PB-EQUIP` result 3), and the saved inventory after bot logout contained the same
owned item identities, counts and captured properties. Only candidate item 9758
moved from its carried position into the previously empty waist slot. The player
reported success; the harness exited zero and all test-owned services stopped.
Evidence is retained locally in ignored `build/playerbot-smoke-20261006-113536/`
(`gear-before.json`, `gear-after.json` and console logs). This qualifies one
empty-slot move and saved persistence, not relogin, occupied-slot displacement,
automatic equipment selection or broader class/score coverage.

Donor `EquipAction.cpp: EquipItem` and `ItemUsageValue.cpp: QueryItemUsageForEquip`
at `037c01418b5d01506917a3db9b44fd56ac5f965c` use native inventory admission and
swap/equip execution rather than database rewrites. Cata's exact-slot handler is
PROCESS_INPLACE, accepted by MapSessionFilter, and delegates to Player::SwapItem.
A narrow session hook posts copied requester intent; world-thread chat never
mutates inventory. `gear apply` requires default-off StarterEquip and StarterScore
gates, full control and attachment. Map execution repeats current authority,
attachment, expiry, alive/same-map, transfer/combat/cast and rest/loot checks.

One mailbox request remains busy through completion. Calculated candidate/stat
dependencies are refreshed without resetting manual strategy/range values. Only
Upgrade/FillSlot/ReplaceBroken are actionable. The first safe carried candidate
in existing survey order is re-resolved against owner, destination identity,
item/affix/factor, carried location and native CanEquipItem before the native
exact-slot handler runs. At most one change is submitted per explicit request;
success requires that item GUID in the destination slot. Relevant snapshots and
obsolete queued actions are invalidated afterward. Queued is not completed.
No autonomous loop, bags, repairs, purchases, loot votes or custom persistence
are added; starter-model limits remain.

Three new tests cover mailbox completion ownership, five-second expiry/wrap and
safe actionable decisions; parser checks reject extra apply arguments. Whitespace
checks pass. Windows worldserver/tests-common built and passed 372/372 tests.
Linux worldserver/tests-common also built and passed 372/372 tests; the compiler
container was stopped afterward. Native slot changes,
displaced-item preservation, relogin and repeat-request behavior were not covered
by that build qualification; the runtime result above records the later saved
empty-slot check. Both shipped gates stay off. The copied outdoor harness supports
explicit `-GearApply`, with before/after saved inventory evidence for the observed
empty-waist candidate. Its preparation does not constitute runtime acceptance.

## Gear inspection operational check — 2026-10-06

The copied outdoor fixture completed the authorized `gear?` check. The player's
screenshot shows Testone's four slot alternatives: one empty-slot candidate,
one unknown/incomplete comparison and two Keep results, with bounded score text.
The player reported normal behavior and logged out. This accepts native delivery
and basic diagnostic interpretation for the shown Warrior report; it does not
establish every class response, every authority/gate transition, optimal rankings
or a measured before/after inventory fingerprint.

The harness exited zero, all four bots logged out, world/auth/test database stopped
and MySQL recorded clean shutdown. No assertion/fatal match appeared; no combat or
healing casts were required or observed by the harness's class counters. Evidence
stays local in ignored `build/playerbot-smoke-20261006-092902/` in the core.
The route remains read-only/default-off. Native equipment execution is not enabled.

## Authorized read-only gear inspection — 2026-10-06

Ordinary whispers and party/raid chat recognize only `gear`, `gear?` and `gear ?`.
The existing server-origin lookup, full-control security and raid-subgroup routing
remain authoritative. The world-thread chat handler never reads the inventory or
decision engine; it reports copied map-owned data through the existing immutable
session read model. No new session command hook or duplicate lifecycle registry
was introduced. The legacy strategy-snapshot name/getter remains compatible.

Gear publication uses the default-off starter-score gate, refreshes at most once
per two seconds except identity/gate changes, and exposes no more than six rows.
Replies require matching bot identity and a gear snapshot younger than five
seconds; dead/transferring bots are unavailable. Disabled, not-ready and unsupported
level/spec/state outcomes are explicit. Strategy STATE has a separate ready flag,
so gear-only publication cannot masquerade as a valid empty strategy snapshot.
The original strategy gate, one-second publication cadence and addon framing remain.
Gear data cannot authorize equipment changes; inspection sends no equip request.

Five new regressions cover fixed read-command boundaries, gated/rate-limited
publication, expiry/timer wrap, bounded formatting/unknown scores and gear-only
versus strategy readiness. Linux worldserver/tests-common passed 369/369; logs
are in ignored `build/linux-gear-inspection-20261006/` in the core and the compiler
is stopped. Windows worldserver/tests-common also built and passed 369/369. Source security/group routing is
reviewed; Testone's native query delivery passed the subsequent check above.
Full gate/authority transitions and measured inventory invariance remain unqualified.

The local harness adds explicit `-GearInspection`, enabling only the copied
configuration and skipping dungeon entry unless separately requested. Parser,
existing wait-policy and environment-isolation checks passed without services.
No combat, item grant or equipment mutation is required for this inspection.
Startup retries caught missing retired-party normalization and premature safe
placement in the new mode; both were corrected for copied offline fixture data.
The placement ordering check passed. Failed setup copies shut down their test
database cleanly; those failed setup attempts add no native query acceptance.
The later successful replay is summarized above.

## Read-only carried-gear comparison survey — 2026-10-05

Donor `StatsWeightCalculator.cpp: CalculateItemTypePenalty`,
`ItemUsageValue.cpp: QueryItemUsageForEquip` and the 1.1 default upgrade threshold
from `PlayerbotAIConfig.cpp` at `037c01418b5d01506917a3db9b44fd56ac5f965c`
supply this bounded survey. `starter equipment comparisons` uses the existing
default-off starter-score gate and the same eight-spec/level-10–39 scope.

Native carried candidates are re-resolved by identity and checked again for
current ownership, carried location, class/spec/level snapshot and exact native
CanEquipItem destination. Current occupied-slot metadata replaces cached data.
Donor two-hand, Arms/Fury/Protection and caster weapon multipliers now feed the
shared score reader, using native inventory type, dual-wield and Titan's Grip
capability. Obsolete talent-aura bonuses and unsupported classes are not copied.
No armor-type penalty is invented: the donor's general armor penalty is disabled,
native usability still applies, and level-40 armor specialization remains outside
the starter model. These are heuristics, not full slot-combination optimization.

Owned affixes require matching signed identity, all five actual property-enchant
slots and the native suffix factor before a local fact copy gains instance proof.
The cached hypothetical facts remain unverified. Non-property instance enchants
are still unsupported; sockets, sets, procs and incomplete scores stay Unknown.
Handedness changes and coupled offhand displacement also stay Unknown, rather
than treat a two-slot change as a single-slot upgrade.

The read-only result distinguishes Keep, Upgrade, FillSlot, ReplaceBroken,
NeedsRepair and Unknown. Healthy duplicate-entry replacement can repair a broken
slot; normal same-entry swaps retain donor conservative behavior. Broken candidates
are deferred for repair. The fixed donor 1.1 comparison default is used; no item
is equipped, repaired, purchased or rolled for, and no recommendation is sent to chat.

Five new regressions cover weapon preferences, caster hand penalties, coupled
layouts, positive/empty/broken/same-entry decisions and incomplete comparisons.
The existing affix test also checks that local instance proof cannot leak into
cached hypothetical facts. Linux worldserver/tests-common passed 364/364; logs
are in ignored `build/linux-equipment-survey-20261005/` in the core, and its
compiler is stopped. Windows evidence subsequently confirmed worldserver linkage
and all 364 tests passed. Native survey queries
have not yet been exercised with a real inventory fixture.

## Default-off starter gear score models — 2026-10-05

Donor `StatsWeightCalculator.cpp: GenerateBasicWeights/GenerateAdditionalWeights`
at `037c01418b5d01506917a3db9b44fd56ac5f965c` supplies the heuristic rows.
Optional `Playerbots.Equipment.StarterScore.Enabled` defaults off; the read-only
`starter item score::<item ID>[,<property ID>]` value covers level 10–39 Warrior
Arms/Fury/Protection, Mage Arcane/Fire/Frost and Priest Holy/Discipline. Unknown
trees/classes, Shadow Priest and levels outside the window remain unavailable.
These are source-adapted starter heuristics, not optimal Cata or endgame weights.

Donor base additions and the supported spec-specific rows are retained, except
mastery, defense, armor penetration, block value/rating remain unmapped rather
than inherit inappropriate Wrath coefficients. Native Cata
`Unit::SpellBaseDamageBonusDone/SpellBaseHealingBonusDone` adds intellect above ten
to spell power: caster/healer intellect weights include that extra power term
(Mage 0.3+1, Priest 0.8+1). The reader rejects the non-linear low-intellect case.
Mage spirit has explicit zero combat weight; no Wrath Molten Armor spirit bonus
or old spell-rank adjustment is copied. Other coefficients remain donor heuristics,
not newly measured marginal stat values; passives, caps and higher-level tuning
are not claimed. The range ends before level-40 armor-specialization assumptions.

The reader requires native usable weapon/armor templates, live same-level facts
and the existing completeness/class/spec/profile checks. It cannot score mastery,
unsupported legacy stats, unresolved affixes, procs/conditions or instance-level
socket/set inputs. Wands and thrown weapons now use the native ranged inventory
channel rather than the melee channel. No equip action, recommendation chat,
purchase or loot vote consumes these scores; the gate is not enabled in a test realm.

Six regressions cover all eight supported spec mappings, level bounds, donor base
additions/spec differences, Cata intellect and omitted Mage spirit, unmapped stats,
plain caster comparisons and ranged weapon channels. Linux worldserver/tests-common
passed 359/359; logs are in ignored `build/linux-starter-gear-score-20261005/` in
the core, and the compiler is stopped. The corresponding Windows build also
passed 359/359. Native
runtime score queries and gameplay usefulness remain unqualified.

## Native equipment admission and guarded score/comparison mechanics — 2026-10-05

Donor `ItemUsageValue.cpp: QueryItemUsageForEquip` calls native CanEquipItem
before considering replacement; `StatsWeightCalculator.cpp: CalculateItem`
accumulates stat weights, and the upgrade branch requires both a better score
and the relative-improvement threshold. Pin:
`037c01418b5d01506917a3db9b44fd56ac5f965c`.

Read-only `equipment candidates` examines carried owned weapon/armor instances
against every native equipment slot with CanEquipItem(swap=true,not_loading=true).
Only successful exact-slot destinations enter a copied identity/slot snapshot.
Native skill/class/level/unique/combat/cast/ownership checks remain authoritative;
no temporary Item or equipment mutation is created. Bank/container upgrades and
hypothetical unowned drops are outside this value. Existing/candidate durability
and native affix identity/factor are copied, not Item pointers. Future execution
must re-resolve/recheck; a dry run is not lasting authorization.

The donor weighted sum and strict comparison are available as guarded mechanics.
Models default unqualified and must match native class, primary tree, collector
profile and level range. Unmapped nonzero stats are unknown, not zero-weight;
intentional zero weights remain distinct. Nonfinite inputs and narrowing overflow
reject. Partial effects/procs/use/conditions, unverified affixes and unresolved
socket/set instance inputs cannot produce a score. Missing scores produce Unknown,
not an upgrade/no-upgrade guess.

No Wrath table is qualified as Cata. Tests use synthetic weights, not balancing
recommendations. Cata coefficient mapping, item-type/weapon/armor preferences,
proc valuation, actual gems/enchants, broken-item replacement and native execution
remain separate work. No automatic score advice, equip action, purchase or vote
is enabled. Five regressions cover model identity/level, unmapped versus explicit
zero, incomplete evidence, numeric rejection and strict thresholds. Linux
worldserver/tests-common passed 353/353 after the test included native talent-tree
declarations. Logs: ignored `build/linux-equipment-boundary-20261005/` in the core;
its compiler is stopped. The corresponding Windows build also passed 353/353. Native carried-item
admission has compiled but has not been exercised in a client replay.

## Random affix collection and socket/set score context — 2026-10-05

Donor `StatsWeightCalculator.cpp: CalculateRandomProperty/CalculateSocketBonus/
CalculateItemSetMod` at `037c01418b5d01506917a3db9b44fd56ac5f965c` supplies this
bounded collection/context batch. Item-base queries accept `item ID,property ID`;
positive IDs select property rows, negative IDs suffix rows, and omitted/zero IDs
leave a random template unresolved. Bounded signed parsing and wide absolute-value
conversion avoid overflow at the minimum signed integer.

All five native affix slots are read. Suffix allocations use each slot's own
percentage and deterministic native GenerateEnchSuffixFactor, with wide product
arithmetic, native truncation and overflow rejection. This avoids the donor's
inner search limited to the three enchant-effect slots, which can miss the fourth
and fifth suffix allocations. Missing rows/enchants/factors remain explicit;
unknown or conditional enchant effects retain their existing markers. A supplied
affix remains pool/instance-unverified: this is read-only estimation, not proof
that an item instance or drop can legally have that affix. No random affix is generated.

Socket count/bonus identity and existing native equipped-set counts/maximum
thresholds are copied into the snapshot. Donor heuristics are exposed separately:
three percent per socket, five percent for a first set piece, ten percent per
existing piece below the final threshold, otherwise no set multiplier. They do
not activate gems, socket bonuses or set spells, nor claim a complete score.
Missing set metadata and skill conditions remain explicit. Native pointers are
borrowed only on the map update. No item, group, database or loot vote is changed.

Five regressions cover query boundaries, signed identity safety, wide suffix
allocation, bounded socket multipliers and set heuristic thresholds. Linux
worldserver/tests-common passed 348/348; logs are in ignored
`build/linux-item-affix-context-20261005/` in the core and the compiler is stopped.
The corresponding Windows build also passed 348/348. Cata spec weights/native equip eligibility,
proc/on-use valuation and instance-level socket/enchant context remain ahead.

## Flat equipment effects and enchantment stats — 2026-10-05

Donor `StatsCollector.cpp: HandleApplyAura/CollectEnchantStats/AverageValue` at
`037c01418b5d01506917a3db9b44fd56ac5f965c` supplies the next collection layer.
The existing item-base snapshot now includes supported generic flat on-equip
auras; separate `enchant base stats::<enchant ID>` snapshots collect native
enchantment stat entries and flat equip spells. These values do not apply effects.

Primary/all-primary stats, role-filtered combat rating masks, mastery, attack power,
healing power, generic spell damage, physical armor, block value and mana regen
use the donor collection channels. Cata's scoped StatType and both native all-stat
selectors replace the donor STAT constants. Native enchant Effect/EffectArg/
EffectPointsMin replace Wrath type/spellid/amount fields. Zero suffix-allocation
amounts remain unresolved rather than fabricate an affix bonus.

Generic flat spell amounts use deterministic native CalcBaseValue plus the donor
die-range mean and native per-level adjustment. Cata scaling variance has zero
mean and takes precedence over dice, matching native CalcValue's branch order;
no random roll or live effect modifier is invoked for this read-only snapshot.
Procs, triggered spells, on-use uptime, family/form/aura/stack conditions and
resource/mastery/target-level-sensitive formulas are explicitly deferred.
Skill/level/condition-restricted enchants remain conditional, not equip permission.
Unsupported auras/rating bits stay flagged rather than silently become zero.
Random affixes, socket activation, item sets, spec weights and native equipment
eligibility remain separate dependencies; no vote/purchase/equipment change occurs.

Five regressions cover all-stat selectors, typed rating masks/mastery, donor
role/school semantics, deterministic averages and unsupported effects. Linux
worldserver/tests-common built and passed 343/343 after the native StatType
adaptation. Logs are in ignored `build/linux-item-flat-effects-20261005/` in the
core and the compiler container is stopped. The corrected Windows build also
passed worldserver/tests-common and all 343 tests.

## Item base-stat collection foundation — 2026-10-05

Donor `src/Mgr/Item/StatsCollector.cpp: CollectItemStats/CollectByItemStatType`
and `StatsCollector.h` at `037c01418b5d01506917a3db9b44fd56ac5f965c` provide
the collection structure for equipment scoring. The read-only
`item base stats::<item ID>` value preserves donor profile precedence, typed
hit/crit/haste filtering, additive stat collection, health/mana normalization and
spell-power/heal-power channels. Cata mastery is appended as its own channel;
extra armor joins armor rather than a removed Wrath stat.

Native Cata ItemTemplate stat values, scaling-distribution stat IDs, effective
armor and weapon damage tables replace Wrath raw ItemStat/Armor/Damage fields.
Signed stat conversion follows native Player::_ApplyItemBonuses. Lookups are
bounded and nullable, and snapshots retain no Item/Player pointers. Unsupported
stats and the presence of item effects, random properties, sockets and item sets
are explicit. This collects base facts, not a complete score: enchant/proc effects,
socket evaluation, random affixes, feral conversions and set bonuses still need
their owning donor layers. Available does not grant native equip permission.

The scoring audit found donor Wrath armor-penetration/defense weights, old talent
spell IDs, role-specific overflow rules and item-set/socket/random-property
dependencies. Do not copy those weights blindly to Cata or substitute item level
alone for a spec-aware upgrade decision. No equipment mutation, inventory/database
write, loot vote or class-admission change is enabled by this collector.

Four regressions cover profile precedence, resource/stat accumulation, typed
ratings/ranged attack power and distinct mastery/unsupported input. Linux
worldserver/tests-common built and passed 338/338 tests after correcting the
native empty-socket sentinel; logs are local in ignored
`build/linux-item-base-stats-20261005/`. The corrected Windows build subsequently
passed worldserver/tests-common and all 338 tests as well.

## Consumable item-usage stock foundation — 2026-10-05

Donor `src/Ai/Base/Value/ItemUsageValue.cpp: Calculate/GetConsumableType/
CurrentStacks/BetterStacks` at `037c01418b5d01506917a3db9b44fd56ac5f965c`
provides the supply policy: fewer than two current-plus-better stacks means USE,
two to fewer than three means KEEP, and two better stacks suppress further stock
requests. The new read-only `consumable usage::<item ID>` value ports that branch.
It does not claim full donor `item usage`: unsupported equipment, quests, skills,
trade and disenchant decisions remain explicitly Unsupported, not NONE.

Cata metadata uses ItemEffect/SpellInfo rather than Wrath's fixed item spells.
Food/drink reuse the existing item-category fallback; potion/flask recovery effects
preserve donor effect order, restricted to actual mana energize for mana supplies.
Mana eligibility uses maximum capacity, so temporarily empty mana does not change
the item's use category. Only usable carried stock of matching class/subclass/type
and equal-or-higher item level contributes as better stock. Item identities are
aggregated once: unlike the donor per-stack traversal, split carried stacks cannot
multiply the same item's total. Bank/equipment stock is excluded. Native usability,
unique maximum counts and stack sizes remain authoritative; no Item pointer is
cached and no inventory, database, purchase, loot vote or item-use action changes.

Four regressions cover bounded item qualifiers, category/mana eligibility, exact
two/three-stack thresholds and unsupported/unusable/max-stock states. Windows and
Linux worldserver/tests-common built and passed 334/334 tests. Linux logs are in ignored `build/linux-consumable-usage-20261005/` in
the core; its compiler container is stopped. New source files required explicit
CMake regeneration before linking on both platforms. The donor pin still matches
GitHub master when checked during this slice. This is a prerequisite for future loot/item decisions,
not automatic need/greed or an equipment-upgrade implementation.

## Coordination replay — 2026-10-05

The copied mixed-party Ragefire replay completed all four bot arrivals in instance
1. Logs record 10 Testtwo and 6 Botmage active DPS reassessments, 13 Testone party
aggro recoveries, 25 Mage accepted damage casts and 19 Priest accepted healing
casts. The player reported normal behavior. No assertion/fatal match appeared;
the harness exited zero, world/auth/test database stopped, and MySQL recorded
clean shutdown. Evidence stays local in the core's ignored
`build/playerbot-smoke-20261005-163042/`.

This qualifies ordinary party operation and active DPS reassessment, not explicit
attack preservation, cast-boundary timing, detailed facing or multi-tank retention.
The fixture has one Protection tank and an Arms damage Warrior. No resurrection,
rest cycle, addon aggregate ACK/restore or full-clear acceptance is added by this
session. Those observations remain deferred, not prerequisites to each new slice.

## Active DPS target reassessment — 2026-10-05

Donor `src/Ai/Base/Strategy/DpsAssistStrategy.cpp: InitTriggers` and
`src/Ai/Base/Trigger/GenericTriggers.cpp: NotDpsTargetActiveTrigger::IsActive`
at `037c01418b5d01506917a3db9b44fd56ac5f965c` adopt a non-null DPS target when
it differs from the current target, rather than waiting for the current enemy
to die. The existing Cata donor-ranked DPS selector was previously used only
for initial fallback acquisition; active damage bots did not reassess it.

The map-thread companion adapter now reassesses at its existing one-second
action cadence for engine-enabled Mage and damage Warrior auto-assists. Explicit
attack commands, tanks/main-tank assignees and native non-melee casts are left
alone. Priest support remains on its healing cadence. Fresh GUID resolution,
party engagement, crowd-control exclusions, range/LOS and native attack checks
remain mandatory; this does not admit autonomous pulls. A successful switch
cancels obsolete queued actions through the existing attack transition. Missing
or unchanged candidates leave the active attack untouched. The donor enemy-player
exception is outside this creature-only companion scope; no PvP admission changes.

Two regressions cover reassessment authority/role/cast gates and missing/unchanged
candidate behavior. Windows and Linux worldserver/tests-common builds passed all
330 registered tests. Linux used the existing Ubuntu 22.04/GCC 11.4 Release
normal-PCH source snapshot; logs are in the core's ignored
`build/linux-dps-reassessment-20261005/`. The compiler container is stopped.
Native multi-enemy reassessment remains pending and can join the next dungeon
replay. Local/uncommitted.

## Main-tank resolution and coordinated aggro guards — 2026-10-05

Donor `src/Bot/PlayerbotAI.cpp: GetMainTankGuid/IsMainTank` at
`037c01418b5d01506917a3db9b44fd56ac5f965c` first honors explicit native group
assignment, then selects the first living role-recognized tank. The Cata resolver
preserves first-assignment and first-fallback order, including an unavailable
explicit assignee: death/disconnect does not silently reassign the group role.
Unassigned fallback uses only living, in-world, non-transferring same-map members,
matching this companion adapter's local party scope. Selection carries GUIDs,
not retained group/player pointers, and is rebuilt on native map updates.

Existing tank-target icon/aggro classification and auto-assisted rescue now honor
recognized tank roles or the main-tank assignment. Automatic Warrior taunts use
the same guard, closing their earlier mismatch with rescue switching. A human
main tank whose current spec is not recognized therefore remains protected from
automatic aggro theft. Class/spec masks, threat percentage denominator and class
admission are unchanged: assignment is not a new rotation or tank-spec override.

This retains the companion adapter's conservative no-steal rule for every tank.
It does not copy donor `HasAggro`'s permission for an explicit main tank to reclaim
another tank's aggro; encounter-specific handoffs and explicit taunt commands are
separate work. Target admission, crowd-control exclusions and native cast checks
remain authoritative. No group flags, database rows or thread ownership change.

Two additional selection regressions cover explicit/unavailable precedence,
empty identities, fallback ordering and fresh snapshots. Final Windows and Linux
worldserver/tests-common builds passed all 328 registered tests. Linux reused
Ubuntu 22.04/GCC 11.4 Release with normal PCH; logs are in the core's ignored
`build/linux-main-tank-resolution-20261005/`. An initial isolated-test link failure
was corrected by testing the identity-templated selection policy with plain
identities while the server retains native GUIDs, without adding game-library
dependencies to policy tests. The compiler container is stopped. Native multi-tank
play remains pending; this batch is local/uncommitted.

## Explicit main-tank target retention — 2026-10-05

Donor `src/Bot/PlayerbotAI.cpp: IsExplicitMainTank/GetGroupTankNum` and
`src/Ai/Base/Value/TankTargetValue.cpp: FindTankTargetSmartStrategy::IsBetter`
at `037c01418b5d01506917a3db9b44fd56ac5f965c` distinguish explicit main-tank
assignment from role inference. In a multi-tank group, smart ranking retains
the explicit main tank's current target before applying aggro/range/threat bands.

The Cata port reads native group member flags on the map update and preserves
the donor first-assigned-slot rule. Living tank count uses the existing strategy/
spec role classifier, restricted to in-world, non-transferring members on the
bot's map. Remote/offline tanks therefore cannot activate local target retention.
Assignment alone does not fabricate a tank spec, new bot class admission or
spell capability. No group, Player or Creature pointer is retained.

Only candidates admitted by the existing engaged-party, range/LOS, crowd-control
and native attack checks can be retained. Existing raid-icon fast-path precedence
remains ahead of smart ranking, as in the donor. With no eligible current target,
one tank, or no explicit assignment, original ranking remains unchanged. The
session's auto-assisted rescue uses this value without changing its no-pull,
explicit-command or other-tank protections.

Two regressions cover assignment/count boundaries, current-target precedence,
stable ties and unchanged fallback ranking. Windows and Linux worldserver/
tests-common built and passed all 326 registered tests. Linux reused Ubuntu
22.04/GCC 11.4 Release with normal PCH and a refreshed native source snapshot;
logs are local in ignored `build/linux-main-tank-20261005/` in the core tree.
The compiler container stopped after validation. Native multi-tank behavior
remains pending. This is a new local slice,
not part of the published 324-test coordination/recovery milestone.

## Coordination/recovery operational check — 2026-10-05

The copied level-20 Ragefire party session completed with player-confirmed
eventual eating/drinking. Console capture records Mage/Priest native drink starts,
48 accepted Priest heal casts, 86 accepted Mage damage casts, Warrior role actions
and party-aggro recovery. All four bots held on owner death and resumed following
after the owner was alive nearby. Priest resurrection was not observed.
Conservation was enabled only in copied module configuration through the harness's
optional HealerSaveMana switch; shipped defaults are unchanged. Quantitative mana
savings, individual suppression decisions and detailed tank orientation were not
measured. The harness exited zero and all test-owned services shut down cleanly;
no assertion/fatal match appeared in the console capture. Local runtime evidence
is retained in ignored `build/playerbot-smoke-20261005-113123/` in the core tree.
This accepts basic coordination/recovery operation, not full dungeon or class parity.

## Coordination/recovery batch Linux validation — 2026-10-05

The current local healer-conservation, melee-positioning and recovery-metadata
batch built Release worldserver/tests-common on Ubuntu 22.04/GCC 11.4 with both
modules enabled and normal core/script PCH. All 324 CTest checks passed, matching
Windows's 324-check result. The saved native-filesystem source copy was refreshed
with every changed/new module file; this validates an uncommitted snapshot, not
an exact published revision. Logs remain local under the ignored core directory
`build/linux-coordination-20261005/`. The compiler container was stopped afterward;
game realms were not started. Linux runtime and sustained native behavior of
these new slices remain untested. Earlier slice counts below describe their
initial Windows checkpoints.

## Recovery item metadata and completion mode — 2026-10-05

Donor `src/Mgr/Item/ItemVisitors.h: FindFoodVisitor::Accept` and
`src/Ai/Base/Actions/UseItemAction.cpp` at
`037c01418b5d01506917a3db9b44fd56ac5f965c` identify food/drink through
consumable or food subclasses and item on-use categories. Cata stores these
categories on `ItemEffect`; its native Player buying path also consults those
categories. Recovery execution previously required only the food subclass and
consulted SpellInfo's category, while ready-check counting already accepted
both subclasses and preferred the item category.

Both paths now share subclass/category translation. Recovery retains its stricter
regen-aura requirement and first valid on-use effect selection; readiness still
reports stock rather than promising cast eligibility. Native usability, cooldown,
spell request, consumption and aura checks remain authoritative. Selected eating
versus drinking mode is recorded alongside the spell ID in map-owned value state,
so item/spell category differences cannot change the resource used for the 95%
completion threshold. Cancellation clears both values and retains existing
combat/transfer interruption and recorded-aura cleanup.

Two regressions cover metadata precedence/subclasses and resource completion/reset.
Windows worldserver/tests-common built and all 324 registered tests passed.
The complete batch also passed Linux's 324-check suite. Native recovery
observations remain pending for a sustained-party session. No item data,
free supplies, regeneration amounts or readiness thresholds are changed.

## Consistent melee role positioning — 2026-10-05

Source comparison against donor master
`037c01418b5d01506917a3db9b44fd56ac5f965c`,
`src/Ai/Base/Actions/MovementActions.cpp: SetBehindTargetAction::Execute`,
confirmed its current-victim exclusion. The existing Cata movement eligibility
also excludes designated tanks, an adapter role policy rather than an exact
copy of that donor action. Initial attack, ongoing stance reconciliation and
the reach action previously used only the victim check when submitting chase.
They now share the eligibility policy: current victims and designated tanks
request front positioning; non-tank attackers without aggro retain rear chase.
This avoids competing role decisions during tank aggro recovery and also keeps
the legacy combat fallback consistent. Native chase/path handling, movement
permission, target authority and behind-action eligibility remain unchanged.

One policy regression covers all role/victim combinations and behind eligibility
after aggro loss. Windows worldserver/tests-common built; all 322 tests passed.
The complete batch subsequently passed 324 tests on Windows and Linux. Native
positioning remains pending; this is not confirmation
that the earlier Warrior facing/idle observation has been resolved. Include it
in the next useful party session rather than requiring an isolated client test.

## Optional healer mana conservation — 2026-10-05

Refreshed upstream master in an isolated reference checkout; it remains pinned
at `037c01418b5d01506917a3db9b44fd56ac5f965c`. Donor sources are
`src/Ai/Base/Strategy/ConserveManaStrategy.cpp`,
`HealerAutoSaveManaMultiplier::GetValue`, `src/Ai/Class/Priest/PriestActions.h`
and `src/PlayerbotAIConfig.cpp`.

The port retains the donor 60% mana threshold, integer percentage snapshots,
65%/45% health guards, tank estimate scaling and efficiency comparisons.
Party Heal estimates 50% at medium efficiency; Flash Heal estimates 15% at low
efficiency; Renew and Shield estimate 15% at very high efficiency. These are
donor scheduling heuristics, not Cata healing formulas. In particular, donor low
mana suppresses Flash Heal on non-tanks even at low health; tank health below
45% bypasses that efficiency restriction. Slower/efficient healing alternatives
remain available through existing action fallback.

`Playerbots.Healing.SaveMana.Enabled = 0` is an optional module-local gate.
The current Cata healing actions can try several native candidates; conservation
therefore filters each candidate at action eligibility/execution rather than
porting the donor multiplier's single-target RTTI hierarchy prematurely. Both
the named engine actions and legacy HealParty fallback use the same policy.
Target health, mana and role are resolved on the current map update, with native
cast eligibility still authoritative. The existing party-action model includes
self among candidates, so it applies these party estimates to self as well.
No new spells, mana restoration, character data or thread ownership is introduced.

Three policy tests cover efficiency/overheal boundaries, tank emergencies,
donor percentage truncation and disabled/unported paths. Windows worldserver
and tests-common built successfully; all 321 registered checks passed. The
prior 318-check Linux validation belongs to the committed operational milestone;
the complete new batch subsequently passed Linux worldserver/tests-common and 324 tests.
Native sustained-party behavior is pending and can join later dungeon play.

## Shared-state operational party milestone — 2026-10-05

The corrected dedicated dungeon-entry fixture completed all four bot arrivals
in one Ragefire instance. Player feedback reported working behavior; several
trash pulls exercised Warrior role actions, Mage damage, Priest Renew,
combat/noncombat transitions and native corpse opening. The realm stopped
cleanly; no assertion appeared. Full clears, measured support/resource recovery,
death/resurrection and addon aggregate ACK/STATE/restore timing remain deferred.
This records bounded operational acceptance, without extending class or autonomy
claims. The core harness now enables dungeon entry independently of recovery
settings; its optional DungeonFixture uses existing native entry hooks.

## Full Linux candidate validation — 2026-10-04

The current uncommitted source built Release worldserver and tests-common on
Ubuntu 22.04/GCC 11.4 with both optional modules enabled. CTest passed 318/318
checks. Normal core/script PCH was enabled with two compiler workers. A source
copy on the container's native filesystem excluded generated builds, Git metadata
and active `.conf` files; no exact committed revision is claimed for this build.
Configure/build/test logs are retained locally in the ignored core directory
`build/linux-milestone-20261004/`. The redundant slow Windows-mounted build was
interrupted after success. Unrelated realm containers remained stopped.
Linux realm runtime and native aggregate ACK/STATE/restore timing remain untested.
Earlier pending-Linux statements below describe their original batch checkpoints.

## Linux protocol/group-policy check and current documentation — 2026-10-04

The current protocol, group batch, pending ownership and completion inbox tests
compiled directly with Ubuntu 24.04/GCC 13.3 and passed all 30 cases with 4,380
assertions. This checks portable policy and concurrent completion transport;
it does not accept the native worldserver integration or the complete Linux
suite. The available environment lacks the full server toolchain and Docker's
Linux engine was stopped at that initial check. Full Linux source validation
subsequently passed as recorded above; realm runtime remains untested.

README.md now summarizes current capabilities, configuration and observed client
results. Its older batch-by-batch text is preserved in
docs/README_HISTORY_2026-10-04.md. Donor pins and adaptations remain recorded here.
The 2026-10-04 outdoor replay observed four-bot combat, return to noncombat,
Priest Renew casts and native corpse opening, with a clean shutdown. A client
chat screenshot confirms group loot removal in all four strategy lists. It does
not confirm aggregate ACK timing, framed STATE refresh or restoration.

## Group strategy client/server timeout contract — 2026-10-04

The pinned MultiBot `80148dff` reader and installed Cata compatibility candidate
expire a pending strategy token at five seconds. The prior six-second server batch
deadline would produce a late ACK that the reader discards. Cata group aggregation
now closes unresolved work at four seconds. Removing the pending entry revokes
its weak map-execution lease, so still-queued toggles cannot run after the reply;
already executed changes remain counted and are not rolled back. The native
strategy mailbox still expires at five seconds, and ordinary/BOT requests keep
their existing transport. Pure tests cover the new deadline and timer wrap.
The checked-in Lua 5.1 mock now drives the actual patched Cata addon response
reader and five-second callback: an ACK while pending is accepted, and a late
ACK is rejected without a second callback. It does not simulate server latency
or replace the bundled client replay. The Windows modules-enabled worldserver
build and all 318 tests pass; Linux and in-game validation remain open.

## Opt-in group dispatch and world completion delivery — 2026-10-04

Donor basis remains bridge `1da05982` RunStrategyMutationCommand,
BotMatchesCombatScope/SendStrategyMutationAck and addon `80148dff` STRATEGY_ACK.
The donor synchronous loop is adapted to Cata map ownership. ALL intersects the
native authorized roster with controlled bots; GROUP/PARTY are current-group
aliases including raids, and RAID additionally requires a raid. At most 128
copied members are frozen before posts; oversized batches do not partly execute.
Existing replay/rate, security, phase and safe-idle checks remain authoritative.

The world-owned table caps 32 batches and one per account, with non-repeating
in-process generations. Copied binding holds account/group/scope and weak opaque
login/cancellation markers, not native pointers. Logout atomically rotates the
login marker. Map execution rejects replacement logins, revoked leases and changed
native membership. Admission rejection is failure; queued work remains unknown.
The world pump drains copied results and resolves current account/character/login
before replying once. Lost logins abandon replies. Transfer/group/gate cancellation
revokes unexecuted work without rollback; missing results remain unknown at the
four-second deadline. Refresh STATE before retrying a toggle.

`Playerbots.StrategyControl.GroupMutations = 0` is a separate opt-in requiring
the base and AddonMutations gates. No runtime config changed. Ordinary/BOT requests
retain generation zero. Six additional pure tests cover scope/login binding,
atomic limits, out-of-order/admission results, abandonment/generation protection,
cancellation and timer wrap. Final reviewed Windows worldserver built and all 318
checks passed; both-modules-disabled worldserver built with 19/19 checks passing.
Review preserved the BOT rejection path and tightened native IsMember checks at
admission/execution. Linux and bundled replay remain pending. Pure tests/compilation
are not native group-command timing or gameplay acceptance, nor full strategy parity.

## Native group correlation and completion inbox — 2026-10-04

The existing strategy request hook now carries an optional copied batch generation.
Ordinary and BOT requests keep generation zero and their existing reply path.
A group request requires a valid token, an empty BOT target and a supported
mutation; query-only or mixed group/BOT metadata rejects before admission.
Mailbox capacity, cancellation and five-second expiry are unchanged.

Map execution retains the existing controller/security/phase/idle checks and
publishes the strategy snapshot before submitting a copied terminal result.
`StrategyCompletionInbox` synchronizes map producers and one future world
consumer, caps storage at 4096 results and rejects invalid or nonterminal records.
It contains no native pointers, session references or packet delivery. Overflow,
cancellation and expiry do not synthesize success/failure; missing results must
remain unknown at the batch deadline. Duplicate/stale filtering belongs to the
world-owned batch policy, not the transport.

This is the Cata asynchronous adaptation of the same bridge/addon pins documented
below, not a new donor feature. Five additional tests cover native mailbox
correlation/regression, cancellation/expiry, bounded copied transport, batch
correlation and concurrent producers/drain. Windows modules-enabled worldserver
built and all 312 checks passed. With both optional modules disabled, worldserver
also built and all 19 core-only checks passed. Linux validation remains pending:
the Docker daemon endpoint was rechecked and is absent; no host settings changed.
These are copied-policy/transport checks, not native group-command gameplay proof.

No addon group dispatch is enabled and no producer can currently be reached by
client group commands. Authorized scope selection, bounded pending batches,
requester-session binding, execution-time membership checks and the world-thread
delivery pump remain required together. The future pending table must admit at
most 32 batches of 128 members; inbox overflow still needs honest timeout handling.
Group controls continue to return UNSUPPORTED_SCOPE.

## Group strategy completion policy, not runtime fanout — 2026-10-04

Bridge and addon main were refreshed, unchanged at `1da05982` and `80148dff`.
Source basis: bridge RunStrategyMutationCommand/SendStrategyMutationAck aggregate
counts/reasons and addon Core/MultiBotComm.lua STRATEGY_ACK parsing. The donor
applies changes synchronously; Cata requires completion across owned map updates.

`PlayerbotStrategyBatch` is a pure aggregation prerequisite, not an enabled
feature. It freezes at most 128 unique copied bot IDs, validates the same bounded
state/strategy grammar, binds requester/token/state and a nonzero generation,
rejects queued-as-success/stale/foreign/duplicate results and emits one ACK only
when all results settle or its four-second deadline expires. Known admission
rejection counts as failure; unresolved results remain unknown on TIMEOUT.
The reader permits succeeded+failed below matched and exposes reason, although
its partial/failed UI label does not itself communicate rollback semantics.

Six new policy cases cover copied/limited identities, no match, correlation,
admission versus completion, complete/mixed outcomes, timeout and timer wrap.
Final reviewed Windows tests-common validation passed all 307 checks.
Runtime source was unchanged; the preceding
301-case worldserver build remains the current runtime build. Linux Docker was
rechecked and the daemon endpoint remains absent; no host settings were changed.
No native hook, group dispatch, inbox, roster permission or packet pump was added.
Group controls still return UNSUPPORTED_SCOPE. See the matching core's
PLAYERBOTS_GROUP_MUTATION_PACKET.md before connecting this policy to runtime.

## Optional donor focus strategy and hostile self-area metadata — 2026-10-04

Refreshed upstream master, unchanged at
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Source basis:
`src/Ai/Base/Strategy/ThreatStrategy.cpp` (FocusMultiplier::GetValue and
FocusStrategy::InitMultipliers). The donor suppresses non-healing AoE and
CastDebuffSpellOnAttackerAction, but keeps other actions. The Cata adapter uses
explicit action metadata instead of RTTI against donor spell-action classes:
`isHealingAction` and `isDebuffOnAttacker`, both false by default. Future area
healing/attacker-debuff actions must declare their category. Current support
actions remain non-area and are not blocked; this adds no attacker-debuff action.

The pure multiplier/strategy is registered alongside threat in supported contexts.
It is optional, never a class default, and accepts safe-idle `co` add/remove/toggle
or single-bot C-state MultiBot changes under the existing strategy-control gates.
Base-gate disable removes focus while restoring class utility defaults. Role/spec
metadata, group routing and native cast ownership remain unchanged.

The existing MageSpellAction adapter previously marked every self-targeted action
as threat-free. Its hostile self-centered action (Frost Nova) now has AoE metadata,
while positive self buffs remain None and current enemy-targeted actions remain
Single. Thus existing Frost Nova participates in both optional focus suppression
and the shared donor area-threat guard. No new area spell, density selection,
ground targeting or pull/crowd-control safety system is enabled. This metadata
mapping is scoped to the existing Mage adapter, not a general spell classifier.

Windows worldserver built and all 301 checks passed. Three new cases cover
focus categories/null action, the Mage target/positivity mapping and a production
focus-multiplier engine replay (area/attacker-debuff blocked, healing/single-target
allowed, removal restores area actions). Control/default-restoration checks now
cover optional focus. These do not qualify native spell data or gameplay; observe
registration/timing and existing Frost Nova behavior in the same deferred replay
when the encounter permits it. Newer Linux validation remains pending. No core,
runtime, database, installed-addon or publication changes were made.

## Combat utility strategy controls — 2026-10-04

Refreshed donor master, unchanged at
`037c01418b5d01506917a3db9b44fd56ac5f965c`. This extends the existing donor
Engine::ChangeStrategy / ChangeStrategyAction vocabulary and MultiBot bridge
RunStrategyMutationCommand transport pinned at `1da05982e478cb00e0b6c87314afe7e0e9653ffb`;
no replacement engine or role/spec command system is introduced.

The state-specific mutation allowlist now permits `co` threat/potions as well as
`nc` food/loot, with add/remove/toggle and optional query. Dead state is query-only.
One shared allowlist drives admission and map-thread engine execution; mixed or
cross-state batches reject atomically. Native authorization/controller/phase,
safe-idle and expiry checks are unchanged. Roles/specs/cure/stay remain protected.
Global recovery/loot/class gates still apply: registering a strategy cannot enable
disabled item use or create spells/items. The threat strategy does not govern
Warrior native auto-attacks or direct/manual execution.

Combat utility defaults are installed once while controls are enabled, so a
removed strategy is not immediately re-added by the next map update. Disabling
the base control gate restores potions for supported contexts and threat for
Mage/Priest; Warrior's default remains no threat multiplier. Class/spec/cure/healer
defaults continue independently and cannot be removed through these controls.
Overrides are session-local; logout restores defaults through a new behavior.

Single-bot structured mutations now accept C threat/potions and N food/loot.
Copied request state determines the completion ACK's C/N field; queue admission
is not success. Snapshot publication still precedes delivery. Group/fanout,
self-bot, role/spec overrides, persistence and reset remain unsupported. No new
capability, core hook or installed-addon change is required; the existing
opt-in STRATEGY_MUTATION_V1 remains a bounded subset. Timeout remains an unknown
outcome: refresh before retrying a toggle.

Windows worldserver built and all 298 checks passed. Two new cases cover combat
state/allowlist/copied correlation and default restoration preserving unrelated
strategies; existing ACK coverage now checks combat success/failure state fields.
These do not prove native chat/addon timing. Include one idle combat-utility
toggle/restore in the existing deferred replay. Newer Linux validation remains
pending; no runtime/config/database/publication changes were made.

## Shared secondary-caster interrupt targeting — 2026-10-04

Refreshed donor master, unchanged at
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Source basis:
`src/Ai/Base/Value/EnemyHealerTargetValue.cpp`,
`src/Ai/Class/Mage/MageActions.h` and `Strategy/GenericMageStrategy.cpp`,
`src/Ai/Class/Warrior/WarriorActions.h` and `Strategy/FuryWarriorStrategy.cpp`.
The existing shared interrupt adapter now registers the qualified GUID value
and donor-named `counterspell on enemy healer` / `pummel on enemy healer`
trigger/action routes at the existing interrupt priority (40).

The donor's "enemy healer" means an interruptible positive cast, including
buffs, not a creature-role classification or only a direct HEAL effect. Native
Cata generic/channel cast state and CanBeInterrupted remain authoritative.
The existing engaged-PvE attacker GUID value provides deterministic candidates;
each is re-resolved on the map, excludes the current target and must still be
fighting the controller/nearby attached party. Control-protected, invisible,
unreachable, evading and player-controlled targets reject. Native spell
preflight filters range/cost/immunity before selecting a candidate, allowing a
later usable attacker when an earlier caster is out of range.

Execution resolves and checks the GUID/cast again, then uses the existing learned
spell/native cast helper. Cooldown/global-cooldown, transfer, controller distance
and own-cast guards remain in force. No Attack call, selection change, movement
or secondary-target chase was added; no native Creature/Spell pointer survives
the map update. Current-target interrupts may still serve an ungrouped bot's
existing combat; only secondary targeting requires attached-party engagement.
These routes reuse the existing default-off Warrior/Mage combat gates. Cata
Pummel is used rather than importing WotLK Shield Bash/stance prerequisites.
No own-cast cancellation, Priest interrupt, NPC role inference, cross-bot
reservation or full interrupt coordination parity is claimed.

The final reviewed Windows worldserver/tests-common build passed all 296 checks.
Existing trigger coverage now checks both primary
and secondary names/priorities; one new pure policy case covers admission and
exclusions. It does not prove native range/cost/cast races or landed interrupts.
Observe this in the same deferred replay only when a durable secondary enemy
casts a positive interruptible spell; otherwise leave runtime coverage deferred.
Newer Linux validation remains pending. No core hooks/runtime/addon changes.

## Donor healthstone fallback and shared recovery-stock classification — 2026-10-04

Refreshed donor master, unchanged at
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Source basis:
`src/Ai/Base/Strategy/UsePotionsStrategy.cpp`, its healthstone action-node factory
and critical-health trigger, plus `src/Mgr/Item/ItemVisitors.cpp`
(FindPotionVisitor) and the preceding native item-use adaptation.
Native Cata `src/server/scripts/Spells/spell_warlock.cpp` owns the 6262
Healthstone heal script; no WotLK rank/item-name list or healing formula is copied.

The shared critical-health route now prefers `healthstone`, with the donor
`healing potion` alternative. The carried item must be a non-potion consumable
whose first valid on-use spell is native 6262 and passes the shared instant,
positive, combat-usable direct-heal classifier. Native inventory usability,
item/category/global cooldowns and typed item requests remain authoritative.
The module's last-potion filter applies to potions, not healthstones; native spell
checks still make the final decision. No stone creation, distribution, Warlock
class support, script changes or cooldown bypass is added. Existing execution
guards and the default-off `Playerbots.Potions.Enabled` option apply to both.

Readiness stock and potion execution now share `RecoverySpell`. Previously the
stock scan could count flasks or restorative effects after the first usable
effect, even though execution would reject them. Counting now matches the
bounded instant potion types actually supported. It intentionally ignores
current health/mana need and cooldowns: carried stock is not cast readiness.
Healthstones do not substitute for the explicit healing-potion stock requirement.
Food/drink stock classification is unchanged.

Windows worldserver built and all 295 checks passed. Two new cases cover
healthstone classification/lockout/action-node wiring and an engine-level
preference/fallback replay with available, unavailable and rejected actions.
Native item effects, potion/stone cooldown timing and newer Linux validation
remain pending; the pure/stubbed tests are not in-game acceptance. The stopped
fixture, core hooks, databases and installed addon were not changed.

## Shared carried combat potions — 2026-10-04

Refreshed donor master, unchanged at
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Source basis:
`src/Ai/Base/Strategy/UsePotionsStrategy.{h,cpp}`,
`src/Ai/Base/Actions/UseItemAction.{h,cpp}` (UseHealingPotion/UseManaPotion),
and `src/PlayerbotAIConfig.cpp` (critical health 25%, medium mana 40%).
The shared `potions` combat strategy is registered in Warrior/Mage/Priest
contexts. Trigger/action priorities retain the donor medium-heal-plus-one and
emergency ordering. Healthstones and their donor fallback are not yet ported;
healing potion is the direct health route. Distinct internal trigger names avoid
colliding with class defensive/healing triggers. Roles/spec metadata is unchanged.

Default-off `Playerbots.Potions.Enabled` gates execution independently of rest.
Only alive, controlled companions already in combat attempt carried, usable
instant healing or mana potions. Casting, transfer, controller loss/distance,
rest/loot, mounting/flight/vehicle, charm/control effects and arenas prevent use.
Native last-potion state and item/category/global cooldowns remain authoritative.
Inventory scans return copied GUIDs; execution resolves the current item again
and checks its inventory position, ownership/usability and first valid on-use
spell. Only positive, instant, combat-usable direct heal/mana-energize spells
match. Bank stock, flasks, healthstones and channeled recovery are not used.

The existing typed native item request owns spell checks, item consumption,
cooldowns and restoration. No spell grant, synthetic item, direct health/power
write, movement cancellation or database operation was added. Item pointers
are not retained across updates or dereferenced after submission (native use
may delete them). `PB-POTION` logs submission only, not success/effects.
Strategy control still cannot mutate this combat strategy; no extra MultiBot
capability is advertised. Missing stock allows ordinary engine fallbacks.

The final reviewed Windows worldserver/tests-common build passed all 293 checks.
Three new pure tests cover threshold/invalid-value
boundaries, busy/disabled/last-potion policy and trigger/priority registration.
They do not prove native item effects or cooldown timing. Include carried-potion
use in the existing deferred party replay; Linux validation of newer work remains
open. Core hooks and the runtime fixture were not changed.

## Shared area-threat prerequisite — 2026-10-04

Refreshed upstream master, unchanged at
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Source basis:
`src/Ai/Base/Strategy/ThreatStrategy.cpp` (ThreatMultiplier::GetValue) and
`src/Ai/Base/Value/ThreatValues.cpp` (ThreatValue::Calculate overloads).
This extends the earlier `7bae1b5c` single-target adaptation, not the donor's
entire attacker scan or AoE spell system.

The `threat::aoe` qualifier now takes the maximum tank-relative threat ratio
over the existing native engaged-PvE attacker GUID value. Each GUID resolves
again on the current map and must still pass the existing alive/visibility,
range, engagement, crowd-control and ownership filters. The common per-target
calculation retains recognized human/bot tanks, fleeing behavior and safe
zero/nonfinite/saturating arithmetic. No native target pointer is cached.

Scheduled AoE damage now passes both donor guards: attacker maximum below 50%
and current-target threat below 80%. Single-target damage retains its 80% guard;
non-damage support, ungrouped bots and the one-shot neglect flag remain exempt.
No new offensive AoE action is enabled. This guard does not establish splash
geometry, crowd-control avoidance or pull safety for future area spells, nor
does it govern Warrior native auto-attacks or direct/manual execution.

Windows worldserver/tests-common built and all 290 checks passed. Two additional
pure-policy cases cover maximum/empty/discarded inputs and both cutoffs/exemptions.
They do not execute native threat references or establish in-game AoE behavior.
Linux validation of newer work and the deferred bundled party replay remain open.
Core hooks were unchanged; the preceding core-only 19-check result still applies.

## Bounded MultiBot strategy mutation acknowledgements — 2026-10-04

Resumed the interrupted implementation and rechecked bridge upstream HEAD,
unchanged at `1da05982e478cb00e0b6c87314afe7e0e9653ffb`. Source basis:
`src/MultiBotBridge.cpp`, RunStrategyMutationCommand and SendStrategyMutationAck.
The upstream reader contract checked previously at `80148dff` requires
scope, encoded target, token, C/N state, matched/succeeded/failed counts and reason.

`RUN~STRATEGY~scope~target~token~state~encoded-changes` now has strict bounded
field decoding and donor-shaped STRATEGY_ACK replies. A separate default-off
`Playerbots.StrategyControl.AddonMutations` requires the base strategy-control
gate and advertises `STRATEGY_MUTATION_V1` only when both are enabled. This is a
bounded implementation of the capability, not full strategy parity: only BOT
scope, N state and food/loot operators execute. Valid group scopes, combat roles
and unsupported strategies receive explicit failure/no-match reasons. Self-bot,
group fanout, persistence and role/spec overrides remain unimplemented.

The world handler uses native roster/control authorization and the existing
per-account rate/replay guard before queueing. Correlation token and target are
copied through the same session mailbox; ordinary chat remains unchanged.
Map execution rechecks controller, phase, native security, feature gates and
idle conditions. Changed or unchanged accepted registrations count as success;
globally disabled rest/loot remains disabled. No acknowledgement claims item use,
loot awards or combat behavior. Rejections count as failures, not queued success.

Completion ACK is emitted only after map execution and publication of the updated
registration snapshot. The requester is freshly resolved for delivery. Expiry,
transfer/control cancellation or an unavailable recipient may instead reach the
client's timeout: that is an unknown outcome, not proof of no mutation. Refresh
STATE before retrying a toggle; explicit add/remove are preferable for retries.
Malformed/oversized envelopes reject before mutation; target identity is never
replaced to squeeze a success acknowledgement into the 250-byte budget.

Windows worldserver/tests-common built and all 288 checks passed, including six
new cases for field decoding, result counts/budget/overflow, replay protection,
copied correlation and invalid mailbox metadata. Fixed the omitted test-only
lifecycle guard include during resume. The current modules-disabled worldserver
and tests-common also built and passed all 19 core-only checks with Playerbots and
AHBot disabled. Linux/native client execution remain pending. No installed addon,
runtime service, database, commit or remote publication was changed.

## Read-only MultiBot strategy framing — 2026-10-03

Refreshed bridge upstream HEAD, unchanged at
`1da05982e478cb00e0b6c87314afe7e0e9653ffb`. Source basis:
`src/MultiBotBridge.cpp`, AppendStateFramesForBot, AppendStateFramePacket and
SendStatesFrames. Checked the current MultiBot-Chatless reader at
`80148dff3f3a25a56d38dba0ecbd4f165b8c1d3f`, fetched without changing the installed
addon. This adapts the existing protocol rather than inventing a Cata UI format.

`GET~STATE~encoded-name~token` and `GET~STATES~token` return donor-compatible
STATE_BEGIN/ITEM/END and global STATES_BEGIN/END frames. Item scopes are C/N,
indices are one-based, names are percent-encoded and end counts match begins.
Only `STATE_FRAMING_V1` is added when the existing default-off strategy-control
gate is enabled; structured strategy mutation/self-bot capabilities remain absent.
Legacy unframed requests are not implemented or silently treated as framed.

Cata uses an immutable map-published session snapshot rather than donor direct
cross-thread engine reads. It contains bot/controller identities, capture time
and copied combat/noncombat names, never native objects. Changes publish at the
end of the map update; unchanged data is refreshed at most once a second. Native
world-thread roster security is reapplied per query. Ordinary callers must match
the captured controller; native GM read authority also covers unattached bots,
without broadening mutation permissions. Identity/freshness checks reject old
sessions or data five seconds old; transfer/missing snapshots abort
the whole request. These are registration snapshots, not current effects,
eligibility, role overrides, dead-engine export or dungeon readiness.

Requests use a separate per-account four-per-two-second state-query guard within
the existing bounded requester cache. Complete responses are preflighted before
any BEGIN: at most 128 bots, 256 strategies/scope, 192 bytes/name and 256 packets
per transaction, each at most 250 bytes. Overflow, duplicate names or malformed
requests do not yield truncated success. No runtime services were started.
Windows worldserver/tests-common built and all 282 checks passed after final
GM-read review, including five new framing/request/freshness cases. The current
modules-disabled worldserver/tests-common also built and passed all 19 core-only
checks with Playerbots and AHBot disabled. Linux/client consumption remain unverified.

## Default-off bounded strategy chat transport — 2026-10-03

Uses the same donor `ChangeStrategyAction` pin below; upstream master was
rechecked unchanged. `Playerbots.StrategyControl.Enabled` defaults off.
Ordinary whisper and subgroup-aware party/raid routing now recognize exact
`co`, `nc`, `de` prefixes. Bare prefixes or `?` query registrations in that
engine. Only noncombat `food` and `loot` accept add/remove/toggle; roles/specs,
threat/support, stay, reset, qualifiers and unported strategies are rejected.
Addon-language chat and structured MultiBot mutation capabilities are unchanged.

A narrow core hook carries a single copied requester/state/operator request,
limited to 253 transport bytes, 16 operators and five seconds. World admission
checks native full-control authority and attachment; map execution resolves
identities afresh and rechecks controller, phase, authority and transfer state.
Acceptance means queued, not changed. Mutations require safe idle conditions;
queries read registrations, not enabled config/cast eligibility or dungeon readiness.
Successful map execution reports changed/unchanged and, when requested, the final
sorted strategy list. No engine/context pointer crosses sessions or threads.

Noncombat defaults are initialized once while the feature is enabled, so valid
removals are not immediately undone. Disabling the feature restores defaults;
logout loses overrides. Global rest/corpse-loot feature gates remain authoritative.
Already active rest/loot work must finish before a mutation; this transport does
not cancel native casts, loot or movement. Spec refresh and lifecycle stay retain
ownership because they are not in the mutable allowlist. Later native control
requests and transfer cancel pending requests, including before near-teleport ACK.
Windows worldserver/tests-common built and all 277 checks passed after correcting
the phase guard to Cata's native `IsInPhase`. Four new cases cover transport
boundaries, protected strategies, copied single-use/expiry/cancellation and timer
wrap. Native authority/behavior and Linux validation remain pending. Windows
module-disabled worldserver/tests-common also built and all 19 core-only checks
passed for the new strategy-hook snapshot, with Playerbots and AHBot disabled.

## Bounded strategy-operator engine layer — 2026-10-03

Rechecked upstream master at `037c01418b5d01506917a3db9b44fd56ac5f965c`.
Source basis: `src/Bot/Engine/Engine.cpp`, `Engine::ChangeStrategy`, and
`src/Ai/Base/Actions/ChangeStrategyAction.{h,cpp}`. MultiBot already uses the
donor's comma-separated add/remove/toggle/query vocabulary. Local
`Engine::ChangeStrategies` implements those operators for one selected engine.

Cata adaptations: requests are limited to 250 bytes and 16 tokens; malformed,
unsupported, qualified/aliased or unauthorized names reject the whole batch.
The caller must supply an explicit state-specific mutable allowlist. Adding a
strategy cannot implicitly replace a protected sibling. Changes are staged
before modifying registrations/queues; unchanged and query-only batches retain
queued work. A changed batch clears stale continuers/last-action state and
rebuilds strategy metadata. Query is a result flag, not a chat response or an
intermediate snapshot. Context factory caching may occur during validation;
rejection leaves engine registrations and queues unchanged.

This layer is internal only. No `co`/`nc`/`de` chat or addon mutation capability
is advertised. Reset (`!`), repository persistence and donor random-account
permissions are not ported. Next transport work must carry copied identities,
revalidate native authority on the map thread, choose per-state allowlists and
define interactions with spec/role refresh and lifecycle-owned stay. A context
registration by itself is not permission to enable it in every state.

Windows worldserver/tests-common built and all 273 checks passed, including six
new engine cases for operators, atomic rejection, queue preservation/cleanup,
sibling protection, state isolation and bounds. Linux validation and the
accumulated gameplay replay remain pending; no new client check is needed solely
for this unexposed engine layer.

## Default-off stay control/lifecycle wiring — 2026-10-03

This completes the owning control layer for the saved-position foundation below,
using the same donor revision. `Playerbots.Movement.Stay.Enabled` defaults off.
When disabled, ordinary `stay` remains the old hold alias. When enabled, `stay`
posts a five-second, single-use copied requester identity through a narrow session
hook. Map execution rechecks the current controller, native full-control security
and safe idle ground conditions. Busy requests are rejected, not falsely reported
as a completed mode change. A map confirmation reports the captured position.
`hold` remains the plain hold; no arbitrary coordinate/persistence transport exists.

Activation preserves the controller, clears native follow and automatic assist,
cancels queued decisions/competing movement and enables stay only in the noncombat
engine. Loot acquisition and combat reach cannot start while staying; support reach
still requires ordinary follow/assist. In-range support/rest may run, and returns
yield to casting/recovery or nearby party combat. This is not donor combat-stay:
the current explicit attack route still requires following. Follow/hold/stop and
other movement/control requests release the anchor. Queued stay requests are
canceled by later control requests. Native death, controller loss, map/instance,
transfer and either participant's copied phase/terrain identity changes invalidate
stay. Near-teleport invalidation occurs before native acknowledgement clears the
semaphore. No ghost return or implicit follow after invalidation was introduced.

Native returns use a distinct PBST point-movement ID. A narrow read-only native
getter permits exact point-generator cleanup; no generator/player pointer is
retained across updates. Only matching Player point movement is canceled. Native
path rejection retries no faster than five seconds, without arrival claims.
Read-only phase flag/personal-GUID getters support snapshots containing only
numeric IDs/flags, not Condition/Terrain/native pointers. Phase comparison is
exact for the exposed active phase/terrain/UI-map sets.

Five additional pure cases cover mailbox one-use/cancellation/expiry/wrap, phase
identity changes, lifecycle/owned-ID admission and retry timing. Chat tests now
distinguish Stay from Hold; disabled compatibility remains an adapter branch.
Windows worldserver/tests-common built and all 267 checks passed after final
cancellation cleanup. The regenerated current core with Playerbots and AHBot
disabled also built worldserver/tests-common and passed all 19 core-only checks.
Linux and bundled control/path/lifecycle behavior remain pending. No realm,
commit or push started. Older module-off pending notes below are superseded for
this Windows snapshot, not for Linux/runtime evidence.

## Saved-position/stay foundation — 2026-10-03

Upstream master was rechecked at `037c01418b5d01506917a3db9b44fd56ac5f965c`.
Donor references: `src/Ai/Base/Value/PositionValue.{h,cpp}`,
`Actions/PositionAction.{h,cpp}`, `Actions/StayActions.cpp`,
`Strategy/StayStrategy.cpp` and `ReturnToStayPositionTrigger` in GenericTriggers.
The supported Warrior/Mage/Priest contexts register the `position` value,
`set stay position`, `return to stay position`, `stay` actions and donor-named
stay strategy/return trigger. None is automatically activated. The ordinary
`stay`/`hold` command is still the existing hold alias, not a saved-position mode.

Cata adaptations: the value owns its map rather than referencing an unconstructed
member; capture accepts only finite coordinates and a nonzero controller identity.
Snapshots also bind map and instance, including valid map/coordinate zero.
Return uses the current companion 3-yard tolerance and 35-yard reaction envelope:
idle bots displaced within it request native path-generated MovePoint, while a
safe farther displacement recaptures the current position instead of teleporting.
Native moving bots are not repeatedly redirected. Busy/combat/nearby-party combat,
casting, rest, loot, rebuff, transfer, transport, flight/fall and native movement
restrictions yield. Queries do not reanchor; only an executed action can do so.
Move submission is not confirmed path success or arrival.

Not ported: permissive donor atof/atoi persistence, arbitrary coordinate commands,
qualified single-position/current-position history, random return/guard travel,
sit timers or active combat stay. Before activation, the owning control batch
must cancel competing follow/support/loot requests, explicitly clear anchors on
controller/follow/death/transfer/phase changes and own return-movement cleanup.
Registration alone does not establish these lifecycle guarantees. No new chat
transport, core hook, runtime flag or active strategy was added in this slice.
Five pure/value/strategy cases cover identity, finite capture, return boundaries,
per-value isolation/reset and donor priority/default metadata. Native actions
and control behavior still require acceptance after control wiring.
Windows worldserver/tests-common built and all 262 checks passed. Linux remains
pending. Core source did not change in this slice, so its preceding module-off
Windows build and 19 core-only checks remain applicable. No realm was started.

## Current platform boundary review — 2026-10-03

The current Windows modules-enabled worldserver/tests-common build passed all
262 checks including the subsequently added saved-position foundation. A regenerated matching core build with Playerbots and AHBot disabled
passed worldserver linking and all 19 core-only checks. This supersedes earlier
Windows module-off pending notes below, not their outstanding Linux/client
acceptance. Docker's Linux engine is unavailable; an existing Ubuntu installation
lacks the alternative build toolchain/dependency setup. No replacement packages
were installed. The stopped party fixture remains prepared while the user is
remote. No runtime, commit or push was performed for this validation pass.

## Learned ground-mount following — 2026-10-03

Donor: `src/Ai/Base/Actions/CheckMountStateAction.{h,cpp}` at upstream master
`037c01418b5d01506917a3db9b44fd56ac5f965c`, rechecked before this port.
The bounded adaptation follows a mounted controller using already learned,
active ground-mount spells. `Playerbots.Mount.Ground.Enabled` defaults off.
It uses native Cata riding and mount-capability records, spell overrides, cast
validation and movement. Speed preference comes from native spell/capability
effects, with a stable learned-spell ID tie break; no training or spell grant.

The existing map-thread follow adapter owns coordination. Accepted casts pause
follow refresh; rejection/interruption resets the formation signature so native
follow can resume. Retry is bounded to five seconds and an owned cast to ten.
Commands, controller changes/dismount, combat (including nearby party combat),
rest, loot and pending rebuff release only this adapter's recorded cast/aura.
External mount auras are not broadly removed. Death/transfer/flight/falling/
vehicle states suspend coordination; ownership is reconsidered on a safe update.
No native Player/Spell pointer is retained between updates.

This is not the complete donor mount/travel strategy. Flight-capable and
underwater capabilities, travel forms, preferred/item mounts, battlegrounds,
autonomous travel, fall/flight-flag cleanup and mount-speed synchronization are
excluded. Native outdoor/level/riding/form/water/control/range gates remain;
the donor's Wrath thresholds and special flight spell IDs are not copied.
Five pure tests cover admission, owned cleanup, ground-only capability policy,
retry clock wrap/cleanup and candidate ordering. Native data/casting and bundled
client behavior require separate runtime acceptance. Windows worldserver and
tests-common built; all 257 checks passed. Linux/module-disabled validation and
the bundled client replay remain pending. No test realm was started for this port.

## Shared strategy-aware roles — 2026-10-03

Upstream master was rechecked at `037c01418b5d01506917a3db9b44fd56ac5f965c`.
References: PlayerbotAI::IsTank/IsHeal/IsDps/IsRanged/ContainsStrategy,
Engine's cached type mask and HealPriestStrategy's HEAL|RANGED metadata.
The shared helper prefers configured bot strategy roles, with native Cata
primary-tree/form fallback for ordinary players or uninitialized bots. Wrath
talent-point/presence/aura heuristics are not copied. Unknown hybrid specs do
not fabricate a tank/healer role.

A narrow read-only session hook publishes a copied atomic combat role mask;
core treats the bits as opaque. No peer accesses another bot's live engine or
AI/context/player pointer. Ordinary sessions/module-off return zero. Unlike
the donor's all-engine union, combat metadata supplies roles so utility/idle/
dead strategies do not inadvertently assign or erase a combat role. Peer reads
can see the preceding map refresh; existing value cache intervals remain.
Future strategy controls must refresh the snapshot before role consumers run.

Party cure/buff/resurrection ordering, tank threat/rescue/target ranking, DPS
weighting and melee positioning share the helper instead of duplicated Warrior/
Priest class shortcuts. Controller-first ordering, native eligibility, flags,
cast guards and authorized target scope remain. Other human tank/healer specs
are recognized, not admitted as bot classes; DPS estimation keeps its supported-
class guard. Priest healing gains donor RANGED metadata; cure is utility rather
than a healer role by itself. Shadow-spec bots retain their actual implemented
healer fallback, not a fabricated Shadow rotation.

Five new pure/engine cases cover native roles/forms, healer/damage distinctions,
unknown specs, strategy precedence, and combat metadata retained through idle/
dead states and strategy replacement. Windows worldserver/tests-common built;
all 252 checks passed. Linux/module-off and bundled client acceptance remain
pending. No new spells or movement geometry; Warrior idle is not proven fixed.

## Ready-check supplies and optional deferred rebuff — 2026-10-03

Upstream master was rechecked and remains
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Audited ReadyCheckAction,
ForceRebuff, ItemCountValue, InventoryAction and Mgr/Item/ItemVisitors.
Native ready replies now include the donor's carried usable food/healing-potion
requirements, plus drink/mana-potion requirements for mana users. Quantities are
read fresh from backpack/bags, using native item eligibility and Cata ItemEffect/
SpellInfo records; equipment/bank contents are not counted. Each stack counts
once per matching category, with saturating sums. Counts describe possession,
not demonstrated potion-use functionality or encounter readiness.

Adaptations: food/drink use the first valid native on-use category; recovery
potions/flasks use native HEAL and mana ENERGIZE effects. Unknown spell records
are skipped rather than terminating discovery. The donor's arbitrary-food
fallback for missing drink is deliberately not copied: food alone must not
satisfy a mana user's water requirement. Wrath ammo/happiness checks are omitted
for Cata and the supported Warrior/Mage/Priest scope. Unlike the donor's current
unconditional affirmative confirmation, missing supplies produce not-ready.
The map-thread supply report is labeled a snapshot, not native reply acceptance.

`Playerbots.ReadyCheck.ForceRebuff` defaults off, like the donor option. With
native ready handling enabled and eligible learned Mage/Priest party buffing,
the existing pass is started or an existing manual pass is preserved. A copied
map-owned deferred identity waits for an explicitly finished pass, then checks
HP/MP/proximity/state and supplies afresh. Native casts and buff GCD still gate
pass completion. Death/transfer/combat/controller changes, pass replacement,
disabled flags and timeout never masquerade as completion. Pass serial/ownership
prevents stale checks from canceling a newer manual pass. The world update also
invalidates the mailbox when native group/check/initiator authority no longer
matches; no Group state is mutated from map work.

The 30-second native check lifetime remains authoritative even though a manual
rebuff has a two-minute window. Expired checks receive no late confirmation;
owned ready-check rebuff work is canceled at the next map update. This is still
only the two supported party-buff routes, not all buff/role/encounter readiness.
Five new pure cases cover usable stack counts/saturation, required categories,
deferred wait/finish/reject/cancel, replacement-safe mailbox cancellation and
explicit pass completion/serial identity. Windows worldserver/tests-common built
and all 247 registered checks passed after final nearby-party-combat and config
guard review. Linux/module-off and bundled client acceptance remain pending.

## Native basic ready-check bridge — 2026-10-03

Donor reference remains `037c01418b5d01506917a3db9b44fd56ac5f965c`,
`src/Ai/Base/Actions/ReadyCheckAction.cpp` and `src/PlayerbotAIConfig.cpp`.
The donor reports HP/MP, distance and inventory but then sends affirmative
confirmation regardless of the aggregate checker result. This port does not
copy that unconditional confirmation, Wrath ammo/happiness checks, or the
GUID-prefixed request body into Cata.

`Playerbots.ReadyCheck.Enabled` defaults off. An authorized native leader or
assistant initiation assigns a process-unique world-thread check identity to
the Group, then posts copied group/check/initiator/timestamp identities to
server-origin sessions. The map update evaluates basic operational readiness:
alive/in-world, no combat/attack/transfer/cast/manual rebuff, health >85%, mana
>65% for mana users, and the initiator on the same map within 100 yards. These
thresholds follow donor defaults; requiring the actual initiator nearby instead
of an optional master is a conservative Cata adaptation. This is not the full
donor supply checklist, encounter readiness, or a buff-coverage guarantee.

A mutex-protected one-slot request/reply bridge supersedes old work when a new
check arrives and rejects older map completions. Both directions are once-only
and expire at 30 seconds with unsigned clock-wrap handling. The world update
re-resolves the current group and initiator, checks actual membership, authority,
check generation and expiry, and calls the native handler with only one state
byte. Group mutation/native confirmation never runs in the map update. An
authorized native finish invalidates queued bot replies; ordinary client answer
processing and packet broadcast behavior remain unchanged. No Player/Group
pointer is retained by the bridge.

Automatic rebuff/deferred confirmation and the donor inventory checklist remain
follow-ons. A manual rebuff currently answers not-ready, rather than reporting
ready from its eventual completion. Disabled/replaced/expired/invalidated checks
produce no bot confirmation. Pure tests cover once-only delivery, replacement,
identity mismatch, expiry/wrap and basic readiness boundaries; native group and
packet timing still require bundled client acceptance. Windows worldserver and
tests-common compiled; all 242 registered checks passed, including four new
ready-check cases. Initial Windows name/API compatibility errors were corrected
before the successful build. Linux and module-off validation remain pending.

## Manual force-rebuff state and readiness boundary — 2026-10-03

References at donor `037c01418b5d01506917a3db9b44fd56ac5f965c`:
`src/Bot/ForceRebuff.{h,cpp}`, `src/PlayerbotAIConfig.cpp`,
`src/Ai/Base/Actions/ReadyCheckAction.cpp` and `GenericBuffUtils.cpp`.
This slice ports the manual rebuff operation, not automatic native ready-check
initiation/replies or the donor's full readiness checklist.

Exact ordinary chat `buff` uses existing full-control and party/raid-subgroup
routing. A narrow native RequestPlayerbotRebuff hook hands one copied requester
GUID/timestamp to a mutex-protected session mailbox. Busy requests are rejected;
consumption is once-only with a five-second wrap-safe expiry. The map update
re-resolves the requester and rechecks authority, transfer/life, existing
controller, supported Mage/Priest class and party-buff feature flag before Begin.
Initial transport replies mean requested; the map reply says pass started.
No generic engine-command interface, Player pointer or Group mutation crosses
this boundary. Unsupported/unavailable/expired requests do not start the pass.

Map-owned state uses the donor two-minute window and default 60-second refresh
margin, growing to elapsed+5 seconds. Existing finite-duration aura coverage is
refreshed only if remaining+margin < maximum; permanent/nonpositive duration
is not forcibly replaced. Freshly renewed auras therefore do not create a
perpetual refresh loop. Single/party variants still jointly determine coverage.
Normal buffing remains absent-only. The margin is currently fixed at the donor
default; the donor configurable margin and ready-check option are not exposed.

Native review found Spell::CanAutoCast rejects identical existing auras before
its power/range/cast checks. For aged existing supported buffs on self or actual
group members only, the refresh path uses native CheckPetCast/CheckCast followed
by prepare instead of that duplicate-aura autocast shortcut. Learned-spell,
override, power, GCD/cooldown and native target/cast validation remain. Ordinary
casts and absent buffs retain CanAutoCast, including its target-selection check.
An ungrouped attached controller does not gain this special existing-aura refresh
path. Rejected casts can keep eligible work pending until the bounded timeout;
native effect application and stronger-aura behavior are not assumed proven.

Engine ticks now pass the pending flag to the existing force-check scheduling.
Per-cycle eligible/proposed work and the last accepted buff spell ID are tracked.
Native SpellHistory queries re-resolve the current cast override rather than
retaining SpellInfo. Noncombat Priest engine heals yield while buff work or its
GCD is active, a Cata eligibility adaptation of the donor buff-first multiplier;
combat healing does not yield. Present buff values are re-evaluated during the
pass instead of relying on the ordinary two-second needed cache.
The pass ends when no current eligible work, cast or buff GCD remains, or expires.
The log explicitly means no eligible work, not all-party coverage/readiness.
Combat pauses rebuff work within the window; death, transfer and controller
change clear it. No launched spell is forcibly canceled.

Readiness audit: native Cata `WorldSession::HandleRaidReadyCheckOpcode` is
PROCESS_THREADUNSAFE and its answer branch consumes a single state byte.
Donor SendReadyConfirm writes a GUID before the state and calls the handler
directly. That packet/call cannot be transplanted into the map update. The next
bridge must observe authorized native initiation on the world thread, retain
only request identity, and validate group/initiator/generation/expiry before a
world-thread native reply. Do not infer ready from completion of two buff routes.
No native ready-check response is sent by this slice.

Windows worldserver compiled and all 238 registered checks passed. Four new pure cases cover window/wrap,
refresh boundaries/permanent auras/combat, cycle identity/cancellation and
mailbox busy/once-only/TTL behavior; chat grammar also checks exact buff.
Native GCD/aura effect timing and authorization races remain client acceptance.
Linux and module-off validation of the new hook remain pending. No realm startup,
client session, commit or push.

## Named healing and missing-aura target values — 2026-10-03

Donor master was rechecked at `037c01418b5d01506917a3db9b44fd56ac5f965c`.
Audited `src/Ai/Base/Value/PartyMemberToHeal.cpp`,
`PartyMemberWithoutAuraValue.cpp`, `src/Ai/Base/Util/GenericBuffUtils.cpp`
(`BuffBelowRefreshTarget`, `MakeAuraQualifierForBuff`) and
`src/Bot/ForceRebuff.cpp::BuffBelowRefreshTarget`.

Priest context now registers `party member to heal` as an ObjectGuid value.
It uses the existing in-range health + distance/10 probe and incoming-heal
deferral, with immediate map re-resolution. Health triggers test the selected
patient rather than any member below their threshold; spell-specific actions
still check all ranked candidates for aura/spell eligibility and native cast
fallback. This is the donor selection/trigger relationship, not a new promise
that health always outranks distance. Healing candidate eligibility now also
rejects unavailable/transferring, GM, charmed and unfriendly members.
The existing 90% healing cutoff, ungrouped controller support, 30-yard cast
envelope and separate bounded approach remain Cata adaptations. Donor focus-heal
targets, pets/charms and the full far-range value are not included.

Shared contexts now register qualified `party member without aura` GUID values.
Supported donor qualifiers are `arcane intellect,arcane brilliance` and
`power word: fortitude,prayer of fortitude`, with their single base-name aliases.
They map to the existing native Cata buff spell and single/party aura IDs, rather
than Wrath spell records. Other names/lists/classes return no target. This is a
bounded qualifier adapter, not a general spell-name or comma-list parser.
Buff checks query the value; execution retains the ordered native cast-attempt
loop and current idle/learned-spell/controller/map gates. Either native aura
variant satisfies normal buff coverage.

Donor missing-aura checks pass baseBeforeDuration=0. Outside force-rebuff, an
existing aura is therefore not proactively refreshed. The Cata value preserves
that normal absent-only behavior. Duration-based force-rebuff depends on the
donor pending window, margin, GCD/work tracking and readiness response; those
must be ported as an owning batch, not approximated by unconditional refresh or
invented duration thresholds. Existing trigger scheduling support alone is not
full ForceRebuffState acceptance.

Windows worldserver compiled and all 234 registered checks passed. Three new pure cases cover supported/rejected
aura qualifiers and healing's distance-probe selection. Real native aura lookup,
trigger timing and patient selection remain bundled client acceptance. Linux
is pending while Docker is unavailable. No new core hook, movement owner, DB write,
realm startup, client session, commit or push.

## Named dispel and resurrection target values — 2026-10-03

Donor master remained `037c01418b5d01506917a3db9b44fd56ac5f965c` when checked.
References: `src/Ai/Base/Value/PartyMemberToDispel.{h,cpp}` and
`PartyMemberToResurrect.{h,cpp}`, using the generic party search audited below.
The shared Cata context now registers `party member to dispel`, qualified by
numeric native dispel type. Priest context also registers
`party member to resurrect`. Both expose ObjectGuid instead of donor Unit pointers.
Consumers use fresh Get lookups; no LazyGet target snapshot or native pointer
crosses threads. Resurrection re-resolves the GUID through the bot's current map.

Dispel values reuse living role/subgroup candidates and native aura lists. Strict
full numeric parsing replaces donor atoi; malformed/overflow qualifiers and
unimplemented class/type combinations return no target. Current routes are Mage
curse removal (475) and Priest disease removal (528), requiring the native learned
spell. Party values exclude self to preserve existing separate higher-priority
self-cure actions, an explicit Cata adaptation. Trigger/usefulness checks use the
value; execution retains the complete ordered attempt loop so a native rejection
on the first eligible member cannot starve later targets. Strategy flags and
command/cast/rest/loot permission remain action responsibilities, not new values.

The resurrection value reuses the existing native 30-yard cast candidate helper,
including role priority, CORPSE, pending/incoming resurrection and LOS guards.
It is an in-range cast value, not full donor sight-distance discovery. The separate
bounded 40-yard approach helper and typed session movement intent are unchanged;
values do not submit movement or enable autonomous dead-state travel.

Windows worldserver compiled and all 231 registered checks passed. Two new pure
tests cover supported cure routes and rejected qualifiers; factory wiring and
real native targets are source-reviewed, not client-qualified. Linux acceptance
remains at the 219-case subset while Docker is unavailable. Named healing and
missing-aura values, pets, aura refresh-duration rules and additional dispel/talent
routes remain separate ports. No realm startup, client test, commit or push.

## Protected combat targets and movement ownership audit — 2026-10-03

Donor master was rechecked at `037c01418b5d01506917a3db9b44fd56ac5f965c`.
`src/Ai/Base/Value/InvalidTargetValue.cpp::Calculate` rejects polymorphed,
charmed, feared and isolated current targets, independently of target ranking.
The Cata fallback ranking previously rejected polymorph, but explicit admission
and retained combat did not share these donor control exclusions.

One native predicate now applies those exclusions to DPS/tank fallback ranking,
explicit/automatic admission, retained combat and shared positioning/attack-facing.
Cata `Unit::isFeared()` supplies the donor fear-aura check. Roots and stuns are
not blanket exclusions; this is the donor target policy, not a general test for
every damage-breakable aura. Existing native attackability, PvE, visibility,
party scope, range/LOS/leash and marker rules remain unchanged. Explicit attack
does not bypass actual control protection. Marker-only exclusions still belong
to fallback ranking and are not newly imposed on explicit commands.

Retained protected targets use existing cease cleanup: cancel decision queues,
clear the target, stop native autoattack and active motion. No newly selected
victim, global CC registry, aura mutation or forced teleport is introduced.
Already launched spells/projectiles and existing periodic damage are not canceled;
this does not promise that ongoing effects cannot break CC. Native runtime timing
and aura mappings still need integrated qualification.

The same audit covered donor `MovementActions.cpp::ReachCombatTo`,
`SetBehindTargetAction::Execute` and `MoveOutOfEnemyContactAction::Execute`, plus
`CombatStrategy.cpp::InitTriggers`. Native Cata
`ChaseMovementGenerator::Update` already checks angle/distance/LOS, predicts moving
destinations, uses collision-aware positioning and launches native paths.
Donor geometry is not a standalone replacement: behind positioning additionally
uses collision validation and recent-flee history; MoveTo uses duplicate-move,
wait/priority state. Those dependencies must be ported together if needed.
The current chase-angle adaptation is not full donor geometry or stuck recovery.
Mount-state/travel, full stay/return, pets and autonomous ghost movement remain
separate owning features, not prerequisites for the current level-20 party batch.

Windows worldserver compiled and all 229 registered checks passed. A pure policy case covers each control
exclusion and clearing; it does not exercise real native auras. Linux is pending
while Docker is unavailable. No realm startup, client session, commit or push.

## Shared cure and buff candidate search — 2026-10-03

Upstream master remains `037c01418b5d01506917a3db9b44fd56ac5f965c`.
Audited `src/Ai/Base/Value/PartyMemberToDispel.cpp`,
`PartyMemberWithoutAuraValue.cpp` and `PartyMemberValue.cpp`. Both dispel and
missing-aura values use the generic role/subgroup party search, not healing's
health ordering. The earlier Cata cure health sort and party-buff self-first
group traversal are superseded by this shared search.

Mage party curse removal, Priest party disease removal and Mage/Priest party
buff checks/casts now consume one living-support candidate helper. Grouped
candidates reuse controller/healer/tank/other priority with local-subgroup
preference and stable ties. Native life, in-world, transfer, same-map, friendly,
GM/charm, 30-yard and LOS checks filter targets. The attempt helper preserves
that order, skips ineligible candidates and continues after native cast rejection.
Healthy members are not removed by a healing cutoff. Self-cure remains a
separate donor action at its existing higher priority; this is not an emergency
dispel redesign. Healing keeps its separate health/distance selection.

Ungrouped bots retain the existing self/attached-controller support adaptation;
the donor generic search is self-only without a group. Grouped support does not
add an out-of-group controller to the roster. Cata native dispellable-aura lists,
spell learning, buff aura checks, idle/combat gates and native casts remain
authoritative. Donor pets, aura refresh-duration utilities, arbitrary aura-name
lists, magic-dispel talent policy and generic support approaches are not added.
There is no lifecycle/core hook, DB write or additional movement owner.

Tests now exercise the production candidate-attempt helper without health sorting,
including full-health members, preserved order, rejection fallback and empty input.
A combined role-order/eligibility/fallback case covers the shared policy path.
Windows worldserver compiled and all 228 registered checks passed. Linux remains pending while its
Docker engine is unavailable; native aura/effect and role behavior still need the
bundled client replay. No realm startup, client session, commit or push.

## Party support ordering and resurrection eligibility — 2026-10-03

Upstream master was checked at `037c01418b5d01506917a3db9b44fd56ac5f965c`.
References: `src/Ai/Base/Value/PartyMemberValue.cpp`,
`src/Ai/Base/Value/PartyMemberToHeal.cpp` and `src/Bot/PlayerbotAI.cpp`
(`IsHeal` / `IsTank`). Generic donor party selection prioritizes the controller,
healers, tanks, then others, with local-subgroup preference within each bucket.
Healing has a separate health/distance policy; this port does not replace it
with role ordering.

The shared map-thread candidate helper now supplies that ordering to Priest
resurrection. Native group identity, map, transfer, friendliness, GM and charm
guards precede ordering; corpse, pending/incoming resurrection, native range and
LOS checks remain per-target eligibility. Approach discovery checks the
controller's 20-yard envelope before choosing a corpse, so an ineligible earlier
corpse cannot hide a later eligible one. Native casting and the existing single
session movement owner remain authoritative. In-range resurrection can prioritize
a dead controller; approach still requires a living controller and yields to
eligible injured living members or nearby party combat.

Cata roles use the active primary talent tree: healing specs, Warrior/Paladin
Protection, Death Knight Blood, and Feral in native Bear form. Unknown specs
remain neutral. This classifies existing party members; it does not implement
additional bot classes or donor strategy-based role overrides. Dead Feral members
without Bear form remain neutral rather than guessing tank identity. Wrath Frost
Presence / Dire Bear assumptions were not copied. Equal-role/subgroup ties keep
native roster order instead of the donor's push-front reversal.

Windows worldserver compiled and all 227 registered checks passed. The two new
pure tests cover role/subgroup ordering, stable ties and empty candidates; native
role classification and multi-corpse fallback remain source-reviewed, not runtime
qualified. Linux validation is pending because Docker's Linux engine is unavailable;
the older 219-case Linux result does not accept this slice. Pets, released ghosts,
autonomous recovery and full donor party-value coverage remain unported. No realm,
client session, commit or push occurred.

## Queued range chat and active-chase refresh — 2026-10-03

The previously audited donor RangeAction vocabulary now connects to normal
whisper and party/raid chat using the existing per-bot control/security policy
and raid-subgroup routing. Native core adds only a specific range-request hook;
it does not expose AiObjectContext or generic arbitrary engine commands.
The module session owns one mutex-protected copied request (requester GUID low,
bounded parameter and timestamp). Busy mailboxes reject new requests; consumption
is once-only and expires requests after five seconds using wrap-safe elapsed time.
No player/context pointer or global registry crosses the world/map boundary.

Dispatch checks native bot identity and full control before posting. After party,
movement and instance processing, the map update invokes the named range action
with a GUID-owned Event. That action independently re-resolves the same-map
requester and checks full control and transfer state again. Requests discarded
for expiry/unavailable/transfer/changed authority do not claim success. Initial
transport replies say requested; only the action's effective-range response
confirms application. Ordinary group chat delivery is preserved. LANG_ADDON and
new addon widgets/protocol extensions are not introduced by this slice.

Changing effective spell range marks map-owned refresh intent. On a validated
Mage combat tick, refresh reuses the current native victim and existing CHASE
motion only, with the shared engine/owner/PvE/map/LOS/leash/cast/control/rest/loot
guards. It submits native MoveChase with the new range; casting/control blocks
defer it rather than being interrupted. It acquires no new target and creates
no second movement owner. New attack chases already read the context value.
Friendly healing/resurrection snapshots continue their existing re-evaluation
cadence. Native spell range, permissions and spell selection remain authoritative.

Examples: `range ?`, `range spell ?`, `range heal 28`, `range spell 0`.
This queue, TTL, strict bounds and deferred native refresh are Cata adaptations;
the donor RangeAction itself has no Cata session mailbox. Persistence, shoot/flee
range support and full donor movement priority/history remain separate.

Windows worldserver compiled and all 225 registered checks passed. New tests cover copied/one-slot/once-only
mailbox behavior, invalid requests, expiry/clock wrap and exact chat prefix
extraction. Linux validation could not connect to Docker's Linux engine and is
pending; no older Linux result accepts these changes. No client session, realm
startup, commit or push occurred. Runtime authorization races, responses and
active-chase refresh remain bundled client acceptance items.

## Donor range action and bounded parsing — 2026-10-03

Upstream master remained `037c01418b5d01506917a3db9b44fd56ac5f965c` when
checked. Audited `RangeAction.{h,cpp}`: the donor named action supports `?`,
`<qualifier> ?` and `<qualifier> <number>`, setting the qualified manual range
and reporting override/default values. The donor uses atof and arbitrary
qualifiers; its all-range query also falls through to a false return after
printing. Those permissive/error behaviors are not copied.

Warrior, Mage and Priest now register the named `range` engine action together
with their shared range values. The parser accepts query-all, spell/heal query,
finite bounded numeric setting and zero reset. Spell overrides must be 2–25 yards,
heal overrides 2–30. Unsupported qualifiers, malformed/partial numbers,
nonfinite/overflow values and input over 64 bytes are rejected before context
lookup. Parsing is locale-independent; responses use the classic locale and
report effective/default distances. A valid all-range query returns success.

Execution remains map-thread-only. The action resolves the event's requester
GUID on the bot's current map, checks the existing full-control security policy
again and rejects transfer/unavailable state before inspecting or changing the
context. Responses use the current system-message convention instead of donor
TellMaster. No player pointer, arbitrary qualifier, SQL write or persisted range
parser crosses a boundary.

This is the engine action layer, NOT a newly enabled whisper/party/addon command.
The current chat bridge only queues fixed follow/hold/attack/cease controls.
Next: add a bounded session-owned parameterized handoff, recheck authorization
when consumed and invoke this named action on the map thread. Do not write
AiObjectContext values from world-thread chat callbacks or invent a second global
command registry. Active native chases also need an explicit range-refresh policy;
changing the value alone does not establish that an existing chase was updated.
Shoot/flee range support, persistence and per-spell range selection remain separate.

Windows worldserver compiled and all 222 registered checks passed. New tests
cover query/set/reset grammar, whitespace, limits, malformed/partial/nonfinite
numbers, unsupported qualifiers and effective/default response formatting.
Linux validation could not start because Docker's Linux engine was unavailable;
the previous 219-case Linux result does not validate this slice. No client test,
realm startup, commit or push occurred.

## Shared donor movement permission — 2026-10-03

Audited upstream master `037c01418b5d01506917a3db9b44fd56ac5f965c`:
`PlayerbotAI::CanMove`, `MovementAction::IsMovingAllowed`,
`SetFacingTargetAction::isPossible`, `ReachTargetAction::isUseful` and
`StayStrategy`. The donor centralizes lost-control, root/charm, frozen/polymorph,
controlled-motion and travel restrictions. Reach also checks stay/channeling;
StayStrategy includes an actual return-to-position trigger and stay action.

The shared Cata movement layer now exposes one native CanMove gate, consumed by
engine facing/reach/behind, session follow/path catch-up, attack-entry chase,
Warrior stance chase, friendly support approaches and corpse-loot movement.
It rejects non-world/dead players, teleport/taxi/flight motion, restricted native
unit states, charm, frozen/polymorph, occupied controlled-motion slots and vehicles.
Existing spell-cast, mounted, LOS, owner and leash gates remain at their callers.
Initial follow adoption under a temporary control restriction records an invalid
formation signature so ordinary following can be submitted after control ends;
Priest post-combat follow restoration uses the same gate and retry mechanism.
This does not cancel the core's controlled-motion slot or grant movement authority.
Existing command teardown/cancellation still owns the active companion motion.

Cata has no donor NULL_MOTION_TYPE sentinel: its GetMotionSlotType returns
MAX_MOTION_TYPE when empty. The adapter instead checks GetMotionSlot for nullptr.
The initial build caught that enum difference; the corrected Windows build passed.
The Cata policy deliberately rejects all vehicle and ghost movement rather than
copying donor vehicle exceptions/ghost travel. Spirit of Redemption-specific
handling, swimming/flying flag updates and full movement priority/history remain
outside this slice. This is a movement-submission gate, not a blanket decision or
spell-casting suspension policy; legacy spell fallback paths are not newly ported.

Stay audit: current hold clears controller/follow/assist and pending actions,
so the new target-dependent actions cannot bypass it. Do not call that full
StayStrategy parity: stay position storage, return movement and combat-while-stay
semantics remain a later command/position port. No placeholder stay strategy was added.

Windows worldserver compiled and all 219 checks passed. Linux worldserver
compiled; 219 cases ran with 218 passing and one existing expected failure,
with no unexpected failures. Pure policy checks cover each restriction independently and recovery
when it clears; native crowd-control timing and follow/chase resumption remain
bundled client acceptance items, not proven by the policy tests.

## Specialized resurrection-reach prerequisite — 2026-10-03

Upstream master remained `037c01418b5d01506917a3db9b44fd56ac5f965c` when
checked. Audited `ResurrectPartyMemberAction::getPrerequisites` in
`GenericSpellActions.h`, `ReachPartyMemberToResurrectAction` in
`ReachTargetActions.cpp` and `PartyMemberToResurrect.cpp`. The donor registers
`reach party member to resurrect` as a specialized prerequisite, uses
GetRange("spell") for positioning and excludes non-corpse targets, existing
resurrection requests and incoming resurrection casts.

The Priest resurrection action now exposes that prerequisite. Its named reach
action submits a typed same-update support intent to the existing movement
bridge. Healing and resurrection share one owned GUID/destination, native
MovePoint submission and cancellation/yield path; no second movement owner or
retained Player pointer was added. Target selection is rechecked before request
submission, movement and casting. Native Resurrection cast range remains 30
yards; approach positioning uses the bounded spell range (20 by default).

Approaches require the enabled Priest healing engine, follow/auto-assist,
a living same-party controller, native map/LOS/friendly/corpse eligibility,
no nearby attached-party combat and no eligible injured living member within
healing range. Discovery is bounded to 40 yards from the bot and 20 from the
controller, with the existing 35-yard bot-controller leash. Stop/transfer,
casting, mounted/flight/control restrictions and rest/loot cancellation remain.
Incoming resurrection and new resurrection requests invalidate selection.

Those companion bounds, group iteration order, living-controller requirement,
live-patient preference and snapshot path ownership are Cata adaptations, not
full donor target ranking/travel. Released ghosts, a dead controller requiring
approach, autonomous recovery and full path/stall handling remain unported.
Existing in-range resurrection still uses native cast checks; this batch does
not change death state, create a corpse or grant resurrection directly.

Windows worldserver compiled and all 218 checks passed. Linux worldserver
compiled; 218 cases ran with 217 passing and one existing expected failure,
with no unexpected failures. New checks cover the specialized prerequisite, typed intent reset and
bounded resurrection approach policy. Native movement/casting remains a bundled
client acceptance item, not established by these policy tests.

## Engine-owned healing-reach intent — 2026-10-03

Rechecked upstream master at `037c01418b5d01506917a3db9b44fd56ac5f965c`.
`HealPriestStrategy::InitTriggers` registers `party member to heal out of spell
range` -> `reach party member to heal` at ACTION_CRITICAL_HEAL + 10.
`ReachPartyMemberToHealAction` consumes GetRange("heal"). This is a trigger,
not a blanket healing-spell prerequisite. `ResurrectPartyMemberAction` has a
separate reach prerequisite; resurrection approach remains unported here.

The Cata Priest context now registers those healing trigger/action names and
priority, plus qualified range values. A successful action submits same-update,
map-owned intent; the session re-resolves the friendly target and owns native
MovePoint submission/cancellation. No Player pointer is retained or passed to
another thread. New approaches no longer begin ahead of the decision engine.
Existing active approaches retain their bounded lifecycle until arrival,
cancellation or a three-second yield. The yield disables new reach intent for
that decision tick. Intent is disabled/reset at the start of each map update.

Existing living-party, controller/follow, engine flag, cast, transfer, rest,
loot, LOS and companion-leash checks remain. Mounted/flight/native movement
control states also prohibit approaches. In-range injured members retain
precedence. The shared heal range defaults to 30 yards; bounded context overrides
can reduce the positioning distance without overriding native cast eligibility.
The 40-yard discovery and 20-yard controller envelope remain Cata adaptations.
This is not full donor ReachCombatTo pathing, spell-range selection, ghost travel,
or generalized friendly movement ownership.

Windows worldserver compiled and all 215 registered checks passed. Linux
worldserver compiled; 215 cases ran with 214 passing and one existing expected
failure, with no unexpected failures. New checks cover donor names/priority, same-update intent
disable/consumption and qualified healing-reach bounds. Client acceptance remains
part of the next bundled party replay; no live fix of the Warrior episode is claimed.

## Qualified range-value seam and prerequisite audit — 2026-10-03

Audited upstream `RangeValues.{h,cpp}`, `PlayerbotAI::GetRange`,
`ReachSpellAction` and `GenericSpellActions.h` at
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Donor RangeValue is a qualified
manual float defaulting to zero; GetRange falls back to configured distances.
ReachSpellAction consumes GetRange("spell"). Generic CastSpellAction currently
returns an empty prerequisite list; specialized resurrection/healing paths
have their own reach responsibilities. A blanket spell-prerequisite port would
therefore misrepresent this source and was not introduced.

The shared Cata movement layer now registers donor-named qualified `range`
values. Native chase admission and engine range discovery/submission consume
the same context value through a bounded GetRange adapter. Spell default remains
20 yards; the future heal qualifier defaults to 30 but is not wired into Priest
healing movement in this slice. Zero means default. Cata consumption rejects
negative/nonfinite overrides and clamps positive spell/heal values to the current
companion envelope (2–25 and 2–30 yards respectively). Unknown qualifiers resolve
to zero rather than creating an unbounded context entry. These caps preserve
existing authority; they are Cata adaptations, not copied donor validation.

No player/addon range command or persisted Save/Load parser is exposed. Qualified
values are map-owned context state and reset to zero; no DB/config schema or
thread ownership changes. This is not automatic per-spell maximum-range selection
or full donor range/command/persistence parity. Learned spell and native cast
validation remain authoritative. Subsequent prerequisites should follow actual
specialized donor actions instead of adding generic work to every cast.

Windows worldserver compiled and all 212 registered checks passed. Linux
worldserver compiled; 212 cases ran with 211 passing and one existing expected
failure, with no unexpected failures. Tests cover independent qualified state/reset,
default fallback, supported qualifiers, finite bounds and negative/NaN/infinite
values. No separate client session is required merely to validate this seam.

## Caster movement and attack-entry facing — 2026-10-03

Rechecked donor `CombatStrategy::InitTriggers`, `ReachTargetAction::isUseful`,
`SetFacingTargetAction` and `AttackAction::Attack` at upstream master
`037c01418b5d01506917a3db9b44fd56ac5f965c`. The Mage combat context now consumes
the shared movement seam: donor `not facing target` / `set facing` and
`enemy out of spell` / `reach spell` trigger/action names and priorities.
Generic/Fire/Arcane and the independently registered Frost strategy all receive
the caster triggers. Warrior melee/behind behavior remains on its existing profile; caster strategies
do not receive behind-target triggers. Native chase is preserved when active.

Caster reach uses the existing Cata companion 20-yard chase envelope, now named
once in the shared movement layer. This is not donor-configured spell distance
or dynamically resolved learned-spell range. Native spell validation remains
authoritative, and admission/leash boundaries are unchanged. Priest support
movement is not redirected to hostile chase: it retains its separate healing
reach/follow ownership. The existing optional Mage/Warrior engine flags gate
their movement actions independently.

A shared non-forced facing operation also runs at the existing validated native
attack-entry seam, matching donor AttackAction's face-before-attack/chase
responsibility. It cannot bypass control, casting, transfer or native unfinished
spline restrictions; inability to turn does not fabricate attack/cast success.
Engine-enabled Mage periodic facing now belongs to the named engine action,
while its old direct fallback remains when that engine route is disabled.

Final Windows worldserver compiled and all 210 registered checks passed. Final
Linux worldserver compiled; 210 cases ran with 209 passing and one existing
expected failure, with no unexpected failures. New regressions cover caster registry/priority,
range/facing and active-chase gates. They do not prove native turning, path arrival
or resolution of the observed Warrior stall. Spell-action prerequisites,
dynamic spell reach, flee/movement priorities and active-chase failure recovery
remain bounded donor dependencies, not reasons for a separate test this slice.

## Engine combat movement, first donor batch — 2026-10-03

Refreshed upstream master to the unchanged
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Adapted registry names, trigger
priorities and responsibilities from `CombatStrategy::InitTriggers`,
`MeleeCombatStrategy::InitTriggers`, `SetBehindCombatStrategy::InitTriggers`,
`SetFacingTargetAction`, `SetBehindTargetAction` and `ReachTargetAction`.

`PlayerbotCombatMovement` registers reusable named `set facing`, `reach melee`
and `set behind` engine actions with their donor trigger names. The first
consumer is the existing optional Warrior combat context, shared by Arms,
Fury and Protection. There is no replacement class rotation or session-only
facing special case. Actions resolve the current Creature/controller afresh
and require the native attack victim to match; they never acquire or attack
a new enemy. Existing owner/target leash, map, living, PvE and optional-route
boundaries remain, alongside casting/control/transfer/rest/loot guards.

Facing applies only at stationary melee range and uses non-forced native
SetFacingToObject, which refuses an unfinished spline. Reach submits native
chase only when out of melee and no native chase is already active. Behind
positioning applies to stationary melee DPS with a stationary enemy, not a
Protection spec or a bot holding the enemy's aggro. It reuses Cata's angle-aware
native chase rather than transplanting WotLK collision/flee-history services.
Native pathing and cast checks remain authoritative; submission is not arrival.

This is an initial strategy/action seam, not full donor reach/positioning parity.
Reach does not recover a stalled active chase; behind positioning is a native
chase adaptation, not the donor's two-candidate collision/flee algorithm.
Caster range, generic spell prerequisites, attack-time facing and broader
movement priorities remain later donor work. Existing session chase setup and
role switching are preserved. The observed Testtwo stall remains unconfirmed
until a later integrated check; this port is not proof that it is fixed.

Windows worldserver compiled and all 208 registered checks passed. Linux
worldserver compiled; 208 cases ran with 207 passing and one existing expected
failure, with no unexpected failures. Three regression cases cover donor trigger
names/priorities and movement selection gates, not native pathing or command
cleanup. No new realm/client session is required solely for this source batch.

## Human-led Ragefire checkpoint and deferred movement gap — 2026-10-02

Windows replay `build/playerbot-smoke-20261002-232706` transferred all four
level-20 role bots into native Ragefire map 389, instance 1 after the human
established the party bind. Repeated Molten Elemental/Earthborer/Trogg/Shaman
pulls and Oggleflint engagement recorded 83 Mage offensive casts, 51 Priest
healing casts, Protection abilities and tank rescue switches. The human reported
generally functional behavior, then observed Warriors facing away and Testtwo
standing idle during the boss encounter. Testtwo had previously logged Mortal
Strike/Rend/Heroic Strike on Oggleflint; this was not simply missing acquisition.
No dungeon clear, boss kill, individual item awards or complete state/recovery
qualification is claimed. Native bot save/logout and all test-service shutdown
completed cleanly after the human logged out.

Source review found an omitted donor responsibility: `AttackAction::Attack`
and `SetFacingTargetAction` in `MovementActions.cpp` at upstream master
`037c01418b5d01506917a3db9b44fd56ac5f965c` explicitly face the target.
Our Warrior action eligibility requires a forward arc, but its adapter relies
on native ChaseMovementGenerator; that generator faces when launching a spline,
not explicitly in its already-positioned stop branch. This supports porting
donor facing/positioning as the next shared combat-movement batch. It does not
prove the screenshot's distance/path state or guarantee that the whole idle
episode will resolve from facing alone. A standalone stationary-facing patch
was drafted, then withdrawn in favor of that donor batch; no live fix is claimed.

Project issue policy: record symptoms, source evidence, intended donor owner
and remaining uncertainty. Defer nonblocking gameplay gaps to their owning
feature port rather than adding isolated adapter fixes. Address earlier only
when safety, data correctness, crashes or a genuine development/test blocker
requires it. A planned donor feature is not evidence that a defect is fixed.

## Bundled outdoor checkpoint — 2026-10-02

Disposable Windows replay `build/playerbot-smoke-20261002-231751` used the
current level-20 Protection/Arms/Frost/Holy party. All four joined Test and
completed native same-map summon acknowledgments. All four engaged Luzran,
then returned to noncombat after target death. Logs recorded Protection Shield
Slam, Arms damage, Mage Frostbolt/Fireball, and Priest Renew/Heal on Testone.
An earlier encounter logged Testone switching to rescue Botmage's aggro.

Testone opened Luzran's corpse through native loot permissions; the human
reported automatic looting. Individual item awards/persistence were not
independently verified. Initial explicit Luzran attacks were rejected outside
the 25-yard admission range; later closer engagement succeeded. No assertion or
crash appeared in the checked fight logs. All bots saved/logged out and the
world/auth/database processes stopped cleanly when the human logged out.

This confirms a useful integrated outdoor operation, not every policy added
since the shared-state candidate. Concurrent resurrection, rest interruption,
healing reach, stop/resume and defense without leader engagement were not
independently isolated. The follow-on dungeon run remains pending. The outdoor
harness mode cannot dispatch its console-only dungeon commands after launch;
replay without `-RecoveryLoot` uses the existing automatic native bind/transfer
sequence. In-game development commands remain console-only; no permission
loosening or live database repair was used to work around that restriction.

## Party-aware noncombat recovery guard — 2026-10-02

Compared donor `NonCombatActions.cpp`, `DrinkAction::isPossible`,
`LootNonCombatStrategy.cpp`, `InitTriggers`, and `PlayerbotAI.cpp`,
`DoNextAction`, at upstream master
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Donor recovery/loot use the
noncombat flow and native bot combat checks. A group-wide exclusion is not an
exact donor port: this slice is a conservative Cata adapter guard needed to
keep recovery consistent with the recently expanded attached-party defense.

Rest start/continuation and corpse-loot discovery, pursuit and final native
world-session processing now reject nearby attached-party combat. The shared
helper keeps bot/leader combat as immediate blockers, then checks living,
in-world, nontransferring members of the bot/leader's shared native group,
on the same map and within 35 yards of the leader. Unrelated groups and remote
members do not block recovery. Member eligibility is shared with the existing
party-engagement target gate; the recovery check itself does not select enemies,
enter combat state or authorize attacks. Any eligible member combat blocks
noncombat work, regardless of whether a target is eligible for our PvE attacks.

Rest also explicitly rejects owner transfer/out-of-world/different-map state.
Existing rest aura cancellation, owned loot-movement cleanup, mailbox completion
and native loot permission checks remain. No new cross-thread Player/Group cache
is introduced: the guard runs in the existing map update or native thread-unsafe
world-session loot context, not a new worker. Default flags and DB authority
are unchanged.

Windows worldserver compiled and all 205 registered checks passed. Linux
worldserver compiled; 205 cases ran with 204 passing and one existing expected
failure, with no unexpected failures. The regression composes the shared admission
policy with rest eligibility; it does not instantiate native groups or prove
live aura/path/mailbox cancellation. Qualify interruption and recovery after
party combat in the existing bundled fixture, not a separate playtest.

## Incoming resurrection coordination — 2026-10-02

Refreshed upstream master; it remains
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Adapted
`src/Ai/Base/Value/PartyMemberToResurrect.cpp`, `FindDeadPlayer` and
`IsTargetOfResurrectSpell`, and `PartyMemberValue.cpp`, `IsTargetOfSpellCast`.
The donor rejects a corpse already receiving a resurrection cast, in addition
to its existing completed-request check.

The Cata Priest now skips such a corpse and can select the next eligible group
member. A shared native cast-inspection helper serves direct healing and
resurrection, without caching Player/Corpse/Spell pointers or predicting cast
completion. Other living, in-world, same-map group members' unfinished native
casts qualify only when the explicit unit target or nonempty corpse target
matches and the spell has a donor-recognized resurrection effect. Direct heals
retain unit-target matching and their existing effect/health policy. Interrupted
or completed casts cease to reserve the corpse on the next inspection.

Resurrection discovery also rejects bot/member transfers, out-of-world members
and nonfriendly targets. Existing corpse-only, request, 30-yard, line-of-sight,
native cast, learned-spell and optional-engine gates remain. This is not ghost
travel, automatic resurrection acceptance, role-priority resurrection ordering,
or full donor dead-state parity. Recovery still uses native Cata authority.

Windows worldserver compiled and all 204 registered checks passed. Linux
worldserver compiled; 204 cases ran with 203 passing and one existing expected
failure, with no unexpected failures. The policy regression covers corpse eligibility,
pending requests and incoming casts; it does not simulate native spell/corpse
targets or prove live resurrection. Include concurrent resurrection, interrupted
cast release and fallback to another corpse in later bundled recovery testing
when those situations arise; no forced wipe is required for this source slice.

## Attached-party combat engagement — 2026-10-02

Audited upstream `src/Ai/Base/Value/AttackersValue.cpp`, `Calculate` and
`AddAttackersOf`, at master `037c01418b5d01506917a3db9b44fd56ac5f965c`.
The donor gathers attackers from the group, with native combat/threat and
anti-killsteal checks. The existing Cata attacker-value port gathered group
attackers, but downstream leader-only engagement gates prevented their use.

Target selection, session auto-assist admission/continuation, tank rescue and
Priest damage admission now share an attached-party engagement helper. It keeps
the living, nearby same-map leader requirement. A second member must be living,
in world, not transferring, on the same map, in the bot/leader's shared native
group, within 35 yards of the leader, and already in combat with the creature.
Leader engagement also remains valid for an ungrouped attached companion.
Attacker discovery explicitly adds the controller's native combat references;
existing native threat/victim discovery and candidate validation remain intact.

This is a bounded Cata adaptation, not a copy of the donor's full attacker
validation or an autonomous pull strategy. PvE, detection/LOS, target range,
crowd-control exclusions, explicit command precedence and native attack checks
remain. No retained Unit pointer, new session admission authority or database
operation is introduced. Historical owner-only descriptions below describe the
earlier slices and are superseded by this scope change.

Windows worldserver compiled and all 203 registered checks passed. Linux
worldserver compiled; 203 cases ran with 202 passing and one existing expected
failure, with no unexpected failures. Six policy assertions cover owner-only,
attached-party, unrelated-group, ineligible-member and unengaged-target cases;
they do not prove native combat relationships or integrated tank/healer effects.
Include party-member engagement without leader engagement in the deferred
bundled fixture check, rather than scheduling a separate playtest.

## Native near-teleport acknowledgment — 2026-10-02

Source audit of the failed placement followed native `.summon` through
`ChatHandler::extractPlayerTarget`, `ObjectAccessor::FindPlayerByName`,
`Player::TeleportTo` and `WorldSession::HandleMoveTeleportAck`. Logged summon
commands and continued old-position walking fit an uncompleted near teleport;
name lookup is not proven defective. Native same-map relocation waits for a
client acknowledgment, which a socketless bot cannot send. The existing module
bridge handled only the explicitly requested dungeon worldport path.

Adapted donor `src/Bot/PlayerbotAI.cpp`, `HandleTeleportAck`, at upstream master
`037c01418b5d01506917a3db9b44fd56ac5f965c`. Map updates now acknowledge an
in-world, fully prepared near teleport through typed Cata native handlers.
Native allowed-mover checks are retained; a missing active mover is established
through native SetActiveMover only when self is already an allowed mover, and
an existing non-self mover is never seized. The existing delayed-teleport getter
is now public read-only so the adapter cannot acknowledge before preparation.
No native permission check, world/map ownership or position mutation is bypassed.

After native completion the adapter clears stale queued/reach/path work and
refreshes formation; a compare-exchange cease request preserves any already
queued explicit command. Far/dungeon transfer remains on its existing world
thread path. Human session behavior is unchanged. Cleanup also waits if delayed
operations chain another transfer or remove the player from the world.
Final Windows worldserver and all 202 registered checks passed; final Linux
worldserver and 202 cases passed with one existing expected failure (201 passing,
no unexpected failures). Script syntax and diff checks passed. Native headless
near-teleport completion and later client summon placement are separate checks.

The existing disposable fixture runner has an opt-in `-CheckNearTeleport` mode,
restricted to `-CheckRosterOnly -RoleFixture`. It validates native same-map
destinations, discovers the unambiguous native Silvermoon-region teleport name,
requests online teleports there and back to Tranquillien,
requires fresh per-bot completion markers for both legs, then checks offline
saved landing coordinates through read-only queries after native logout. No
direct position edits or client commands are used to claim completion. It
retains normal role/equipment/consumable verification and automatic shutdown.

Headless replay `build/playerbot-smoke-20261002-192342` passed. The copied native
DB resolved `SilvermoonCity`; all four online bots logged a fresh acknowledgment
for that destination and for the return to Tranquillien (eight completions).
After native logout, read-only queries verified each saved return landing within
one unit of native coordinates, alongside roles/talents/equipment/consumables.
World/auth/database shut down cleanly and test listeners closed. The initial
`192109` run exited cleanly before teleport because the guessed `Silvermoon`
name was absent; destination discovery corrected the fixture, not server code.
This verifies native same-map completion and persistence on Windows, not a
client `.summon` session, automatic follow after summoning, Linux runtime or
integrated combat acceptance.

## Coordination review and dungeon handoff — 2026-10-02

Review found a stale reach-owner condition: a completed/stalled snapshot path
with an unchanged candidate GUID could keep returning before healing decisions
indefinitely. The adapter now yields when native point movement ends or after
three seconds, clears its owned movement, and allows one engine decision tick.
A later healing cadence may select a fresh snapshot destination. This bounded
yield is a native-adapter correction, not a donor pathfinding algorithm. It does
not claim successful obstacle traversal, continuous tracking or cast effects.
Regression coverage checks the timeout boundary and completed-motion case.

Windows worldserver compiled and all 202 registered checks passed. Linux
worldserver compiled; 202 cases ran with 201 passing and one existing expected
failure, with no unexpected failures. This closes the source review/build pass,
not the deferred client acceptance or human-led dungeon milestone.

## Bounded healing reach — 2026-10-02

Upstream master remains `037c01418b5d01506917a3db9b44fd56ac5f965c`.
Adapted the purpose and stay/casting guards of
`src/Ai/Base/Actions/ReachTargetActions.cpp`, `ReachPartyMemberToHealAction`
and `ReachTargetAction::isUseful`, with the far-range health penalty from
`PartyMemberToHeal::Calculate`. This is a conservative native session movement
adapter, not the donor's complete reach-action/prerequisite system.

A followed Priest with engine healing enabled may close a 30–40-yard gap to
a living same-map party member below 80 percent health, only while that member
is within 20 yards of the human leader and the bot remains within its 35-yard
owner leash. Learned basic healing, native friendliness/detection/line of sight
and incoming-heal policy are required. An injured eligible member already in
healing range takes precedence. The adapter uses native MovePoint/pathfinding
with a snapshot destination and stores only the target GUID for movement
ownership. It rechecks eligibility at the 750-ms healing cadence and stops upon
entering native healing range; it does not retain a target pointer or chase
hostile units.

Stay/stop/follow replacement, death and requested transfer clear owned reach
movement. Casting, rest/loot, owner/target loss and leash/party changes reject
or cancel reach. Passive engine ticks and ordinary formation catch-up do not
compete while reach owns active movement. Native casts, spell ranges and map
movement remain authoritative. A moving target, path failure, obstruction,
command cleanup and transfer still need integrated qualification; the snapshot
path is not full donor obstacle traversal or continuously tracking follow.

Windows worldserver compiled and all 201 registered checks passed. Linux
worldserver compiled; 201 cases ran with 200 passing and one existing expected
failure, with no unexpected failures. Reach policy tests cover injury, gap and
leader-distance boundaries and invalid distance. Command/transfer cleanup and
live pathing remain integrated runtime checks, not proven by these unit tests.

## Incoming healing coordination — 2026-10-02

Rechecked upstream master `037c01418b5d01506917a3db9b44fd56ac5f965c`:
`src/Ai/Base/Value/PartyMemberValue.cpp`, `IsTargetOfSpellCast`, and
`src/Ai/Base/Value/PartyMemberToHeal.cpp`, `Calculate`.

Direct and engine Priest healing now inspect other same-map living group
members' native current spells. An unfinished direct-heal cast explicitly
targeting the candidate defers routine healing, but emergency health below
55 percent and raid healing can overlap. The 55-percent boundary uses this
adapter's existing critical threshold, rather than pretending to import the
donor's configurable medium-health value. Instant casts, already-applied HoTs,
damage spells and casts at another target are not reservations. No pointer,
reservation timer or predicted heal amount survives the map-thread inspection.
Cast rejection fallback and native spell authority remain unchanged.

Also audited `src/Ai/Base/Actions/ReachTargetActions.cpp`:
`ReachTargetAction::isUseful` respects stay/channeling and heal reach uses a
separate action. That action is not yet ported. Current candidates stop at
30 yards; expanding discovery/reach needs owner-leash, transfer, cast and
movement cleanup integration, not an unrestricted follow/chase shortcut.
No new combat healing movement is claimed in this slice.

Windows worldserver compiled and all 200 registered checks passed after fixing
the native read-only group iterator type. New policy coverage checks the
55-percent boundary, emergency override, raid override and no-incoming-cast
case. These checks do not simulate concurrent native spell casts; integrated
client acceptance remains deferred.
Linux worldserver also compiled; 200 cases ran with 199 passing and one
existing expected failure, with no unexpected failures.

## Healing selection and caster formation — 2026-10-02

Audited upstream master `037c01418b5d01506917a3db9b44fd56ac5f965c`:
`src/Ai/Base/Value/PartyMemberToHeal.cpp`, `PartyMemberToHeal::Calculate`,
and `src/Ai/Base/Value/Formations.cpp`, `CircleFormation::GetLocation`.
Master was rechecked and unchanged for this slice.

Priest direct and engine healing now share the donor's health-plus-distance
probe (`health percentage + distance / 10`) for native eligible candidates.
Existing 30-yard/line-of-sight eligibility is retained, so the donor's far-range
penalty is unnecessary here. Stable ties, invalid-value filtering and fallback
after native cast rejection remain. Cures still use their own full-health-safe
ordering; spell thresholds and native cast authority are unchanged. The donor's
duplicate-heal cast inspection, focus-heal strategy and pet support are not
ported by this slice.

Mage/Priest follow positions now retain their roster angles but use a six-yard
radius. The role-dependent wider radius follows the donor formation concept;
six yards is a conservative companion-adapter choice, not the donor's configured
flee range or full CircleFormation implementation. Role spacing is included in
the formation signature so a change refreshes native follow movement. No new
combat repositioning, retreat, collision solver or healing-cast movement is
introduced. Integrated party acceptance remains pending.

Windows worldserver compiled and all 199 registered checks passed. Linux
worldserver compiled and ran 199 cases: 198 passed and one existing expected
failure, with no unexpected failures. New checks cover distance-weighted
selection, stable ties/cast fallback, invalid distances and caster formation
angles/signature preservation. They do not qualify live pathing or heal effects.

## Tank rescue integration — 2026-10-02

Rechecked upstream master `037c01418b5d01506917a3db9b44fd56ac5f965c`,
`src/Ai/Base/Value/TankTargetValue.cpp`: `FindTankTargetSmartStrategy` ranks
lost aggro before melee proximity and own threat; `TankTargetValue::Calculate`
recognizes a marked enemy attacking a non-tank player. The existing Cata
target-value adaptation retains its original import attribution.

The new map-thread session integration uses that existing ranking at auto-assist
acquisition and once per action cadence during engagement. Ongoing switches
are restricted to rescuing a same-party player from an already owner-engaged
enemy. They cancel old queued actions through the existing attack transition.
Explicit attack commands are not overridden; another recognized Protection
Warrior's target is not stolen. This conservative adapter is not full donor
multi-tank/role parity or autonomous pull logic. Healing and positioning remain
the next coordination dependencies; bundled client acceptance is pending.

Also inspected `src/Ai/Base/Value/PartyMemberToHeal.cpp` at the same revision:
its health/distance probe and duplicate-heal checks, not a blanket tank-first
rule, are the basis for the next healing slice. No new healing behavior is
claimed here. Windows worldserver compiled and all 196 registered automated
checks passed, including six tank-rescue policy guards. These policy tests do
not simulate native threat, movement or an integrated dungeon encounter.
Linux worldserver also compiled; its suite ran 196 cases with 195 passing
and one existing expected failure, with no unexpected failures.

## Shared decision states — 2026-10-02

Audited upstream master at `037c01418b5d01506917a3db9b44fd56ac5f965c`:
`src/Bot/PlayerbotAI.cpp` engine construction/ChangeEngine and
`src/Bot/Factory/AiFactory.cpp` combat/noncombat/dead factory registrations.
Compared against previous `7bae1b5c58c76a0aa20381155edc08096d1485b2`; the newer
AI changes do not alter this pattern. Existing spell/action imports retain
their own pinned provenance, not a retroactive master label.

`StateEngines` adapts the separate-engine ownership and active-engine switch to
the existing shared Cata context. State queues/strategy sets are separate.
The map-thread bridge selects native death, native combat/authorized engagement
or noncombat. State switches clear pending prerequisites/continuers in all
engines; steady-state selection preserves them. Stop/movement/target changes
clear pending work without requiring native combat flags to drain first.
Transfers suspend ticks and invalidate pending work. Spec refresh affects only
combat registrations. Priest support remains registered in both live states.

The dead engine is intentionally empty: native resurrection and follow recovery
retain the existing session ownership. Donor ghost travel, automatic graveyard
release and autonomous lifecycle behavior are not claimed. No database schema,
thread scheduler or production fixture-grant interface was added.

Windows worldserver/tests-common built and passed 195 registered checks.
Linux GCC 11 regenerated, built both targets and ran 195 Catch cases: 194 passed, one failed
as expected, no unexpected failures. This closes the prior outstanding Linux
curse/Spellsteal compile validation. Real queue/state regression tests passed;
native client transition wiring and role-qualified gameplay remain pending.
See the matching core's shared-state acceptance and repeatable party fixture.

### Native disposable fixture

Added default-off `Playerbots.Dev.Fixture20.Enabled` and a console-only
configured-slot `fixture20` request. A bounded GUID/role mailbox is consumed
on the existing map update; no Player pointer or new core scheduling seam is
introduced. Native LearnPrimaryTalentSpecialization/LearnTalent select the
level-20 Protection/Arms/Frost/Holy roles and spend only available points.
Native spell/inventory checks grant explicit test skills, preserve old equipped
items in bags, equip modest verified items and top up food/water. Completion
is separate from command acceptance and SaveToDB requests are not DB commit proof.
The tool rejects wrong class/tree/level, grouped, dead, combat or transfer states.
This is test scaffolding, not donor production character/equipment progression.
The existing harness can prepare/save/stop the fixture without a client.

Server-only preparation passed on 2026-10-02. The four native primary trees were
845/746/823/813 with zero free talent points. After native logout the harness
verified saved talent records, level, equipped weapons and Protection shield,
plus carried food/water. World/auth/database shut down cleanly. The stopped
copy is retained only as ignored local fixture data. Client combat/transition
acceptance is deferred by the user; the milestone candidate is not full gameplay
acceptance. The fixture audit caught absent legacy spell 168; it was removed.
Native Cata armor records start above this fixture's level and await later tests.

## Reading this history

Entries below record evidence at their dated revisions, newest first. Older
"pending" or inactive-engine statements are not the current project status;
later entries supersede them only for the behavior actually verified. Use the
module README and the matching core's roadmap and infrastructure acceptance
checklist for current scope. Windows build/tests and the server-only factory
batch have passed. Ordinary-player addon lifecycle also passed the bundled
check below. Linux build and automated tests passed as recorded here; Linux
runtime remains unverified. No full donor feature parity is claimed.

## Mage current-target Spellsteal - 2026-10-02, development

Adapt donor GenericMageStrategy spellsteal priority 40 and Mage SpellstealTrigger
at `7bae1b5c58c76a0aa20381155edc08096d1485b2`. Cata learned spell 30449 has
native steal-beneficial-buff effect 126, magic type 1 and learn level 70. The
current validated hostile Creature supplies native dispellable magic candidates;
exclude CANNOT_BE_STOLEN in addition to the native passive/polarity/resistance/
charge filters. Trigger and action recheck eligibility, retaining no aura pointer.
Native Spellsteal owns random selection, resistance, removal/transfer, charges,
duration and cost. Existing controlled targeting, facing/LOS/range and conservative
single-target threat guard remain. No enemy search, PvP routing, general purge,
manual aura transfer or new permission. Two policy tests added; Windows
worldserver/tests-common built and all 192 CTest cases passed. Core/module diff
checks passed. Docker still returns an internal API error; current Linux validation is
unavailable, including the previous curse slice. See research/SPELLSTEAL_PACKET.md.
Native buff transfer remains unverified.

## Mage curse utility and shared party support - 2026-10-01, development

Adapt donor MageCureStrategy and RemoveCurseTrigger/PartyMemberRemoveCurseTrigger
at `7bae1b5c58c76a0aa20381155edc08096d1485b2`: self/party Remove Curse at 41/40.
Cata spell 475 is a native curse dispel learned at level 30. Existing Priest
candidate gathering/unfiltered stable ordering now live in shared party support;
Priest wrappers retain their class/healing filters and native disease behavior.
Shared native dispel eligibility verifies the requested spell's dispel effect
type before consulting GetDispellableAuraList. No retained aura pointers or
manual removal. Mage cures require its existing default-off combat-engine flag,
an alive nearby controller, learned spell and native eligible friendly player.
Rest/loot/transfer/mounted/casting work defers cures. Register independent cure
strategy in both combat/noncombat states and reuse the existing passive timer
for enabled Mages; no new timer, scheduler, lifecycle or database seam. Other
class passive-tick gating remains unchanged. Three new policy/strategy tests;
Windows worldserver/tests-common built and all 190 CTest cases passed, including
existing Priest regressions. Core/module diff checks passed. Linux build was
started, but Docker's API returned an internal error during status inspection;
completion cannot currently be confirmed. Do not infer Linux validation from
the preceding 187-case batch. See research/MAGE_CURSE_PACKET.md. Native
curse removal and resistance/protection cases remain runtime-unverified.

## Mage defensive support - 2026-10-01, development

Adapt donor GenericMageStrategy.cpp and FrostMageStrategy.cpp at
`7bae1b5c58c76a0aa20381155edc08096d1485b2`: Ice Block 90 below 25 percent,
Mana Shield 85 below 45 percent, Frost Ice Barrier 29 below 65 percent or
attacked by the current enemy. These are donor default thresholds, not a port
of its configurable health values. Actions recheck learned spells/current aura
and native exclusion metadata (Ice Block Hypothermia), then request native self
casts. Self-defense is not enemy-facing/LOS gated, but remains in the existing
controlled combat/session path. No own-cast cancellation, Blink, early Ice Block
removal, aura injection or absorb/cost duplication. Native glyph/talent side
effects remain native, including reactive area effects; no new offensive AoE
strategy is added. Three policy tests added. Linux worldserver/tests-common
built successfully; 187 cases completed with 186 passed and the existing
expected failure. Windows worldserver/tests-common built and all 187 CTest
cases passed. Core/module diff checks passed.
See research/MAGE_DEFENSIVE_PACKET.md. Native effects remain runtime-unverified.

## Frost proc follow-up - 2026-10-01, development

Adapt donor FrostMageStrategy.cpp at
`7bae1b5c58c76a0aa20381155edc08096d1485b2`: Brain Freeze -> Frostfire Bolt and
frozen/Fingers of Frost eligibility -> Deep Freeze. Cata aura 57761 effect 1
must affect learned Frostfire Bolt 44614 and fully reduce cast time. Deep Freeze
44572 reads its native frozen target requirement through HasAuraState, including
affecting native ignore-state effects. No manual proc removal or damage/stun.
Priorities 23/22 preserve donor Brain Freeze-before-Deep Freeze order while
placing both above the existing Ice Lance priority 21. Native casting owns
cooldown failure/fallback and spell_mage_deep_freeze's immune-target damage.
No pet, new AoE, movement, pull or lifecycle changes. See the matching core's
research/FROST_PROC_PACKET.md. Two policy tests added. Linux worldserver and
tests-common built successfully; its 184-case suite completed with 183 passed
and the existing expected failure. Windows worldserver/tests-common also built,
and all 184 CTest cases passed. Core/module diff checks passed.
Native proc consumption and immune-target behavior remain runtime-unverified.

## Fire proc and native override support - 2026-10-01, development

Adapt donor FireMageStrategy hot streak -> Pyroblast (25) and improved scorch
-> Scorch (19) at `7bae1b5c58c76a0aa20381155edc08096d1485b2`. Native Hot Streak
48108 overrides learned base 11366 to 92315; shared TryCast authorizes the base
then follows Unit::GetCastSpellInfo and its native cost flags before ordinary
cast validation. No forced replacement, GCD waiver or proc removal. Scorch
requires an active Critical Mass applier and matching loaded native proc metadata;
missing/mismatched metadata fails closed. Do not infer proc correctness from raw
DBC masks or policy tests. Living Bomb is deferred because its expiry explosion
needs AoE safety. See research/FIRE_PROC_OVERRIDE_PACKET.md in the matching core.
Windows/Linux built worldserver/tests-common with the final proc-metadata check.
Windows passed all 182 CTest cases; Linux completed its 182-case suite
successfully (181 passed plus the existing expected failure). Native behavior
and loaded Scorch proc-data correctness remain unverified.

## Fire/Arcane starter routes and Arcane actions - 2026-10-01, development

Adapted donor FireMageStrategy.cpp / ArcaneMageStrategy.cpp at
`7bae1b5c58c76a0aa20381155edc08096d1485b2`: named `fire` and `arcane` siblings,
donor starter default order, Arcane Blast/Missiles/Barrage, and stack-cap Missiles
priority 15. Cata Missiles requires native aura 79808; read CasterAuraSpell and
Arcane Blast 36032's cap from SpellInfo. Existing single-target threat and
cast/channel validators remain; native casting owns mana/stacks/procs. Shared
native spec refresh replaces the old Mage/Frost split and preserves the generic
unassigned route. Fire DoTs/Hot Streak, mana phases, AoE and full movement parity
are deferred. Hot Streak specifically needs native action-bar override resolution;
do not force its replacement spell. See core research/MAGE_SPEC_ACTION_PACKET.md.
Windows/Linux built worldserver/tests-common. Windows passed all 180 CTest
cases; Linux completed its 180-case suite successfully (179 passed plus the
existing expected failure). Native Arcane behavior is unverified.

## Native Cata Warrior additions - 2026-10-01, development

Add Colossus Smash 86346 to Arms/Fury and Enrage-gated Raging Blow 85288 to
Fury through the existing donor strategy/action surfaces. These are explicit
Cata adaptations, not unchanged donor actions. Priorities 28/26 place them
below stance support and around the existing primary/proc attacks. Own Colossus
Smash aura prevents clipping; native SpellInfo/HasAuraState supplies Raging
Blow eligibility. Native spell_warr_sudden_death owns the Colossus cooldown
reset, replacing the donor Wrath Execute-proc assumption. No manual reset,
armor/weapon-hit duplication or above-20% Execute route. Source-checked against
matching Cata scripts/DBC and donor `7bae1b5c58c76a0aa20381155edc08096d1485b2`.
See core research/WARRIOR_CATA_ACTION_PACKET.md. Windows and Linux built
worldserver/tests-common. Windows passed all 178 CTest cases; Linux completed
its 178-case suite successfully (177 passed plus the existing expected failure).
Native behavior remains unverified.

## Warrior proc response - 2026-10-01, development

Adapted donor ArmsWarriorStrategy.cpp / FuryWarriorStrategy.cpp and trigger
registrations at `7bae1b5c58c76a0aa20381155edc08096d1485b2`. Arms responds to
target-bound native dodge reactions or affecting Taste for Blood (60503) with
Overpower (7384) at donor relevance 24. Fury responds to Bloodsurge (46916)
with Slam (1464) at relevance 25, checking the native casting-time operation,
spell mask and full reduction. No unprocced Slam filler or forced instant cast.
Existing controlled melee actions repeat eligibility at execution; native
casting owns proc consumption, damage, rage and final calculated cast time.
See matching core research/WARRIOR_PROC_PACKET.md. Windows and Linux built
worldserver/tests-common. Windows passed all 176 CTest cases; Linux completed
its 176-case suite successfully (175 passed plus the existing expected failure).
Native proc behavior is not client-confirmed.

## Arms and Fury starter strategies - 2026-10-01, development

Adapted donor ArmsWarriorStrategy.cpp / FuryWarriorStrategy.cpp and native
spec selection at `7bae1b5c58c76a0aa20381155edc08096d1485b2`. Add sibling
`arms` and `fury` strategies: Mortal Strike, Bloodthirst, sub-20% Execute and
the corresponding native Battle/Berserker stance. Preserve donor relevance;
Strike remains the starter fallback. Shared native-primary-tree routing now
replaces the session's old generic-DPS/tank split via existing spec refresh.
Support validators take an explicit required tree; Protection defenses remain
Protection-only. Native costs, damage, healing, cooldowns, stance and rage
consumption are unchanged. No AoE, proc-heavy rotation or full spec parity is
claimed. See matching core research/WARRIOR_DPS_SPEC_PACKET.md.
Windows and Linux built worldserver/tests-common. Windows passed all 174 CTest
cases; Linux completed its 174-case suite successfully (173 passed plus the
existing expected failure). Native behavior is not client-confirmed.

## Protection stance and defensive support - 2026-10-01, development

Adapted donor TankWarriorStrategy.cpp, WarriorActions.h and HealthTriggers.h at
`7bae1b5c58c76a0aa20381155edc08096d1485b2`: Defensive Stance, Shield Block,
Shield Wall below 45% health and Last Stand below 25%, preserving donor
priorities and strict health boundaries. Existing Warrior action/trigger classes
now have an explicit self-target mode; hostile actions retain their validators.
Self support stays inside an existing routed Protection fight, without requiring
facing/melee reach. Native casting owns stance, shield, cost, proc and cooldown
requirements. No idle stance manager or emergency tick outside existing combat
routing. Health triggers use Warrior-specific names to avoid the rest trigger
collision. See core research/WARRIOR_DEFENSIVE_PACKET.md.
Windows and Linux built worldserver/tests-common with the final triggered-aura
correction. Windows passed all 173 CTest cases; Linux's 173-case suite completed
successfully (172 passed plus the existing expected failure). Native behavior
is not client-confirmed.

## Warrior tank rotation slice - 2026-10-01, development

Adapted donor TankWarriorStrategy.cpp / WarriorActions.h/.cpp at
`7bae1b5c58c76a0aa20381155edc08096d1485b2`: Devastate filler, Revenge,
Sunder Armor refresh/fallback and Sword and Board -> Shield Slam. Cata native
SpellInfo supplies the three-stack Sunder debuff (58567) cap and Revenge reactive state;
native scripts own Sunder application and Shield Slam cooldown reset.
Existing Protection strategy, default-off combat flag and controlled melee
target boundary remain. Shield Slam default 5.4 precedes donor Devastate 5.3;
explicit Sunder candidate replaces the donor ActionNode alternative. See the
matching core `doc/local/playerbots/research/WARRIOR_TANK_ROTATION_PACKET.md`.
Windows and Linux worldserver/tests-common built. Windows CTest passed all 172
registered tests; Linux's 172-case suite completed successfully (171 passed and
the existing expected-failure case). Native tank behavior remains untested.

## Warrior fallback correction from bundled test - 2026-10-01, development

The bundled outdoor session recorded Mage Fireball/Fire Blast/Molten Armor,
Priest Smite/Renew/Heal, Warrior Strike and native corpse opening by all four
bots. It also recorded repeated distance-leash disengages. The player reported
Warriors failing to resume on a later pull; logs showed their initial engagement,
then leash disengagement. Both saved Warrior primary talent trees were unassigned.
The initial fallback port admitted only Protection Warriors, leaving this role
dependent on selected-target assist while casters could resume independently.

All enabled Warriors now use the named fallback: Protection uses tank target,
unassigned/Arms/Fury use the donor general DPS selector. Mage/Priest routing is
unchanged. Melee DPS ranking uses native melee reach rather than the caster
range band. A pure routing regression checks every implemented route, disabled
gates and unsupported classes. Windows and Linux worldserver/tests-common each
built and passed all 171 tests after the explicit native enum include correction.
This does not fix or
relax the distance leash, switch active fights or qualify runtime recovery.
The completed test used the original binary copy; correction is not live-tested.

## Named combat targets and Protection tank fallback - 2026-10-01, development

Adapted active TankTargetValue smart ranking from donor
7bae1b5c58c76a0aa20381155edc08096d1485b2: lost aggro, owned melee, owned distant;
distance ties for lost aggro and lower own threat otherwise. Only Protection
Warriors qualify. Icon priority recognizes current non-tank roles without
guessing unported roles or another tank bot's rti. Explicit-main-tank preference,
taunts, threat writes and the inactive old selector are not ported.

Shared `dps target` / `tank target` return fresh GUIDs through a borrowed const
engine accessor, bound/cleared by Engine lifetime. Session ownership destroys
engine before context/AI. Idle Protection fallback requires EngineWarriorCombat;
Mage/Priest keep their gates. Selected targets, explicit commands and active
attacks retain precedence. No new core seam, SQL or retained Unit pointers.
Three pure tests were added; Windows and Linux worldserver/tests-common each
built and passed all 170 tests, including the final role-filter recheck. See matching
core research/TANK_TARGET_PACKET.md for native verification gaps.

## Controlled-party DPS fallback - 2026-10-01, development

Adapted TargetValue exclusion/priority gathering, DpsTargetValue caster/general
ranking and RtiValue icon defaults from donor
7bae1b5c58c76a0aa20381155edc08096d1485b2. Strategy exclusion hooks were already
imported; Engine now gathers fresh typed exclusions from active providers.
Shared manual `prioritized targets`, `rti` (skull) and `rti cc` (moon) values
register in current class contexts. No new command, persistence or addon claim.

The native map adapter returns a GUID, applying exclusions, CC markers and
native validity/leash checks only to already-controller-engaged creatures.
Explicit raid icon precedes donor caster/general ranking. Unknown DPS uses
general health/distance ordering instead of unsafe division. Living-near-member
count and the existing 25-yard eligibility are conservative Cata boundaries;
the 30-yard ranking boundary is not the donor configurable default (33.5).

Mage/Priest auto-assist uses this only when idle with no valid selected combat
target, under existing default-off engine gates. Explicit commands and valid
selected targets retain precedence; active fights and Warrior selection are
unchanged. No retained Unit pointers, second target owner or SQL. The matching
core adds only a bounds-checked read-only Group::GetTargetIcon accessor; native
marker writes and persistence are unchanged.
This is not complete named DPS target value parity, dynamic switching or
autonomous target permission. Encounter-specific exclusion providers remain ahead.

Four pure ranking tests and one active Engine exclusion test were added.
Windows and Linux worldserver/tests-common built and each passed all 167 tests;
native selection/gameplay remains bundled.
See the matching core's research/DPS_TARGET_PACKET.md for adaptations and gaps.

## Engaged attackers and healer balance - 2026-10-01, development

Adapted AttackersValue, BalancePercentValue in AttackerCountValues and
HealerShouldAttackTrigger from donor 7bae1b5c58c76a0aa20381155edc08096d1485b2.
Shared `attackers` reads native PvE threatened-by-me references for self and
nearby living same-map group members, retains deduplicated GUIDs and applies
native target/claim checks. It never changes the controlled target. Priority/
skull/duel/arena candidates and independent pet contributors are not ported.

Shared `balance` keeps donor creature-rank weights, roster denominator,
ten-member cap and 0–200 ratio. The numerator uses only living in-world
same-map members, not cross-map lookup. Both values recompute on Get rather
than reusing the donor one-second attacker cache; no retained Unit pointers.
Priest scheduled damage now uses donor-default balance mana thresholds
85/65/40, preserving existing 90% healing priority and controller/command gates.
No solo bypass, new settings, SQL, native threat mutation or core seam.

Three pure policy regressions were added; Windows and Linux worldserver/
tests-common built and each passed all 162 tests.
Native traversal/filtering is source-reviewed, not gameplay-tested. This is a
level/rank heuristic, not full target selection or measured encounter balance.
See the matching core's research/COMBAT_BALANCE_PACKET.md for adaptations/gaps.

## Single-target caster threat - 2026-10-01, development

Adapted ThreatValues / ThreatStrategy from donor
7bae1b5c58c76a0aa20381155edc08096d1485b2. Shared `threat` and reset-on-read
`neglect threat` values use native read-only current-target threat. Tanks are
living same-map group Protection Warriors, including humans; other tank roles
are not guessed. Without a recognized tank, damage remains allowed.
The Mage/Priest scheduled engines veto Single damage at the donor 80% threshold;
healing, buffs, cures and interrupts remain unaffected. Existing gates stay
default-off. No core seam, thread, SQL or new configuration.

The ratio explicitly guards zero division, saturates before uint8 conversion
and defers on non-finite input, rather than retaining donor arithmetic hazards.
Startup/fleeing and the one-shot bypass retain donor policy. AoE qualifiers,
FocusStrategy, Warrior auto-attacks and direct ExecuteAction interception are
not ported. No complete tank/threat-management parity is claimed.

Four pure policy/value tests and one Engine scheduling regression were added.
Windows and Linux worldserver/tests-common built and each passed all 159 tests.
Native reads/wiring are source-reviewed, not gameplay-tested.
See the matching core's research/THREAT_ENGINE_PACKET.md for scope and follow-up.

## Shared combat estimate values - 2026-10-01, development

Ported EstimatedGroupDpsValue / the current-target subset of EstimatedLifetimeValue
and mixed-gear helpers from donor 7bae1b5c58c76a0aa20381155edc08096d1485b2.
Named values register in all three implemented class contexts. The existing
map thread reads native alive/same-map server-origin group bots, carried usable
gear and current role. Human players are excluded. Float-only DPS caching keeps
the donor 20-second interval; group identity/map/bot-level changes invalidate it.
The borrowed AiObjectContext accessor binds/clears with existing context ownership.
No retained Player/Item pointers, equipment mutations, thread or native core seam.

Donor level/gear curves, role weights, quality scaling, best-slot/top-twelve gear
aggregation and party/raid bonuses are retained. Cata robe and holdable mappings
are included. Only supported bot roles/levels 1–80 are modelled; 81–85 is unavailable,
not extrapolated. Unknown estimates return zero; the single-target lifetime value
does not implement arbitrary target qualifiers or the donor multi-attacker penalty.
Shadow Word: Pain now requires its donor eight-second health/DPS gate at execution;
unknown estimates skip the DoT while leaving other spells available. This is an
estimate, not measured damage or complete Cata role/profile parity.

Seven new pure tests cover curves/gear/roles/group bonuses/slots/lifetime policy.
Windows and Linux worldserver/tests-common built and each passed all 154 tests.
Native traversal, filtering and cache/context
ownership are source-reviewed; accuracy and gameplay remain bundled checks.

## Priest healer damage actions - 2026-10-01, development

Extended the same donor PriestHealerDpsStrategy at
7bae1b5c58c76a0aa20381155edc08096d1485b2 with Shadow Word: Pain (5.5), Holy
Fire (5.4), existing Smite (5.3) and Mind Blast (5.2). One spell table supplies
strategy wiring and action registration; each action rechecks its own learned
spell and the existing healing/mana/target gates. The common trigger is no
longer dependent on knowing Smite. No session, movement, core or SQL change.

Native Cata DBC rows confirm Priest class mask 16 for 589 (level 4, periodic
damage), 14914 (level 18, direct/periodic damage) and 8092 (level 9, direct damage).
Periodic actions check only their caster-owned native aura and do not reapply
while it remains. This follows donor owner-aware aura checks; no manual effects.
Holy Fire's donor minimum lifetime is zero. Shadow Word: Pain's eight-second
estimated-lifetime gate requires an unported group-DPS value and is explicitly
deferred; short-lived-target efficiency is not equivalent to the donor.

The wiring regression now checks all four donor priorities; two new tests check
the Cata identities and periodic-aura policy. Windows and Linux built
worldserver/tests-common and each passed all 147 tests. Gameplay remains part
of the existing bundled check. No wand shooting,
AoE, Shadow rotation, balance values or estimated-lifetime substitute added.

## Priest healer DPS - 2026-10-01, development

Adapted PriestHealerDpsStrategy / HealerShouldAttackTrigger from donor
7bae1b5c58c76a0aa20381155edc08096d1485b2. The `healer dps` strategy wires
`healer should attack` to real Smite at 5.3, below heal/cure priorities. Native
Cata DBC confirms spell 585, level 1, Priest mask 16 and school-damage effect 2.
The existing EnginePriestHeal gate remains default-off; no learning or SQL seam.

The map adapter's existing GUID target and assist flag own stationary Priest
support, without melee Attack or MoveChase. Commands/target invalidation precede
the same 750-ms healing tick; ending support restores idle formation follow.
Damage requires the controller fighting a native-valid nearby creature,
eligible healing candidates at >=90% health and mana >=85%. Trigger/action both
recheck. Donor balance is not ported: this explicitly uses its conservative mana
branch, without the solo bypass. Explicit attack cannot start a Priest-only pull.

Four new tests cover donor wiring, eligibility/mana, healing/target changes and
engine-queued damage yielding to changed eligibility. Windows and Linux built
and each passed 145 tests. Control/movement is source-reviewed;
damage, healing responsiveness and follow resumption remain for a bundled live
check. No own-cast cancellation, full Shadow rotation, AoE or balance parity.

## Party-cure healthy-target correction - 2026-10-01, development

Source review for the next healer-DPS slice caught a Cata adaptation error in
the disease port below: its reused TryInHealthOrder erased candidates at 90
percent health or above. Healthy players can still have dispellable diseases.
Ordering is now a separate TryInPriorityOrder helper; healing keeps its original
health filter, while cure target detection/execution use the unfiltered order.
Native life/map/range/LOS/disease checks remain unchanged.

Three added regressions retain healthy/full-health members, preserve stable
health ties and rejected-cast fallback, and handle empty cure lists. Existing
healing regressions still enforce the old healing-only cutoff. Windows and Linux
built and each passed all 141 tests. This fixes selection,
not proof of native aura removal. The next healer-DPS packet is mapped but not
implemented; Priest offensive target/control ownership must be completed first.

## Priest disease cure - 2026-10-01, development

Ports the disease subset of donor PriestCureStrategy, CureDiseaseTrigger and
PartyMemberCureDiseaseTrigger plus CurePriestStrategyActionNodeFactory's real
Cure fallback at 7bae1b5c58c76a0aa20381155edc08096d1485b2. Cata DBC inspection
confirmed Cure Disease 528, class mask 16, level 22, effect 38/dispel type 3;
Wrath Abolish Disease 552 is absent. The `cure` strategy therefore wires
`cure disease` / `party member cure disease` directly to `cure disease` /
`cure disease on party`, preserving donor priorities 31/30 without a dummy
Abolish creator.

The existing Priest engine/candidate helper owns self/controller/group targeting.
Party cures exclude self and try eligible members in health order; a native
cast rejection on one member does not starve the others. Triggers and actions
ask native GetDispellableAuraList for disease eligibility, preserving polarity,
passive/zero-charge/100-percent-resistance rejection and Unholy Blight protection.
Aura pointers stay inside the current call; native TryCast/EffectDispel owns
actual aura removal, resistance and resource effects.

Existing EnginePriestHeal gates execution. Cure defers during rest or queued/
pursued loot. Critical healing priorities remain higher. Magic/poison/curse
dispels, enemy purge, talent dispel changes and AoE cure are not included.
No new config key, scheduler, core seam or database change is added.
Three pure tests cover names/priorities and enabled/alive/learned/native-allow
policy. Windows and Linux worldserver/tests-common builds each passed all 138
tests; native disease removal and protection/
resistance behavior remain unverified in game.

## Heroic Strike and conditional Frost Ice Lance - 2026-10-01, development

Adapts donor FuryWarriorStrategy/TankWarriorStrategy rage triggers,
GenericTriggers::RageAvailable, FrostMageStrategy's Ice Lance default and
CastIceLanceAction at 7bae1b5c58c76a0aa20381155edc08096d1485b2. Generic Warrior
uses `medium rage available` (40 displayed rage), ACTION_DEFAULT + 0.1;
Protection uses `high rage available` (60 rage), ACTION_HIGH. Native Cata power
stores rage in tenths, so reserves are 400/600. The action rechecks the current
primary tree and learned spell before using existing native TryCast.

Read-only Cata DBC checks confirmed Heroic Strike 78, class mask 1, level 14;
Ice Lance 30455, mask 128, level 28. Fingers of Frost proc 44544 applies aura
type 262. Frost Mage uses the native Mage aura-state override and frozen-target
checks from Unit.cpp's Ice Lance damage path. Native casting/damage owns proc
consumption, rage spending and effects; none are set manually.

Frost retains Ice Lance's default slot at 5.3 and adds frozen/proc priority 21.
The action requires current frozen/proc eligibility, rejecting expired states.
This conditional usefulness/priority is an explicit Cata adaptation: donor
Wrath's Fingers of Frost trigger favors Deep Freeze/Frostbolt, and its default
Ice Lance permits movement filling. Unrestricted movement filling, Deep Freeze,
pet/proc rotations and Generic Mage changes are not part of this port.

Both actions use existing default-off combat-engine gates and existing class
contexts. No AoE scan, spell grant, talents, config/database/core changes or
new scheduler. Three pure regressions cover rage boundaries/reserves, known-spell
gates and frozen/proc conditions. Windows and Linux worldserver/tests-common
builds each passed all 135 tests; live resource
spending, proc consumption and damage remain unverified.

## Current-target interrupts - 2026-10-01, development

Adapts donor InterruptSpellTrigger, Warrior Pummel and Mage Counterspell
actions at 7bae1b5c58c76a0aa20381155edc08096d1485b2. Implemented Warrior and
Mage combat strategies register `pummel` / `counterspell` at ACTION_INTERRUPT
(40), under their existing default-off combat-engine flags. Both use the
already-selected GUID-based combat target; no enemy-healer scan is added.

Read-only Cata DBC checks confirmed Pummel 6552, learn level 38, class mask 1;
Counterspell 2139, learn level 9, class mask 128. Both use native interrupt
effect 68. Only learned spells are attempted. Trigger and action revalidate
the same preparing-with-cast-time/channeling and native CanBeInterrupted policy
used by Spell::EffectInterruptCast. Native TryCast still owns range/LOS,
cooldowns, resources, stance, GCD and spell execution. The module never calls
InterruptSpell or locks a spell school manually.

The Mage does not cancel its own current cast to interrupt. Cross-bot
coordination, Priest Silence, healer targeting and movement-to-interrupt remain
follow-ups. These are bounded current-target actions, not full donor parity.
Three pure tests cover cast-state/native-allow policy and trigger names/priority.
Windows and Linux worldserver/tests-common built and each passed all 132 tests
after refreshing build definitions for new files. Landed interrupts remain unverified.

## Active-spec combat strategy refresh - 2026-10-01, development

Adapts donor AiFactory::AddDefaultCombatStrategies and
PlayerbotAI::SelectiveResetStrategies at
7bae1b5c58c76a0aa20381155edc08096d1485b2. The existing single-engine map adapter
now reselects implemented combat routes from native active primary talent tree
before ticking: Protection Warrior -> tank, other Warrior -> warrior; Frost
Mage -> frost, other Mage -> mage. Priest retains the explicit heal fallback.
This fixes login-only selection, not missing spec rotations or talent spending.

Mage combat siblings now have their own context, matching Warrior, so switching
does not remove shared buff/rest/loot/armor strategies. Unchanged routes preserve
the queue. Changed routes use existing AddStrategy/Init to remove the old sibling
and clear obsolete queued actions. Current native casts are not cancelled.
No new scheduler, core hook, database or config setting is introduced.

Three pure regressions cover Cata-tree/fallback mapping, sibling/shared strategy
preservation, and unchanged-queue versus changed-route reset. Windows and Linux
worldserver/tests-common builds each passed all 129 tests. Live spec transitions remain
unverified. See the matching core's SPEC_STRATEGY_REFRESH_PACKET.md for scope.

## Bounded corpse collection and movement - 2026-10-01, development

Follow-up to the opening slice below, adapting donor LootObjectStack,
LootNonCombatStrategy and MovementActions.cpp::MoveToLootAction at
7bae1b5c58c76a0aa20381155edc08096d1485b2. The `far from loot target` trigger
and `move to loot` action keep donor relevance 7. Discovery is limited to
observed defeated targets and eligible controller selection; an eight-entry
map-owned GUID collection expires entries after 60 seconds and selects the
nearest currently eligible corpse. It retains no donor Player pointer.

Native pathfinding handles short detours only: corpse within 15 yards of the
bot and 20 yards of the controller. The existing session movement owner pauses
follow, cancels for combat/control/transfer/disable/ineligibility, and restores
formation on completion. One GUID/timestamp bounds pursuit to ten seconds;
timeout uses the existing per-corpse backoff. Rest/armor defer while pursuing.
No new scheduler, broad area search, teleport, database or config key is added.
The opening mailbox and native permissions/storage remain unchanged.

Three pure regressions cover collection capacity/deduplication, expiry/wrap and
single-pursuit timeout/reuse; strategy coverage includes both donor stages.
Final Windows and Linux worldserver/tests-common builds passed all 126 tests
on each platform, including the movement-control guard. Live detour/path-cancellation behavior remains
unverified and belongs in the next bundled recovery/loot check.

## Nearby corpse opening/storage - 2026-10-01, development

The next bounded slice adapts LootNonCombatStrategy/OpenLootAction/StoreLootAction
at donor 7bae1b5c58c76a0aa20381155edc08096d1485b2. It retains `loot`, `can loot`
and `open loot` names and opening relevance 8. The initial strategy deliberately
omits distant-loot movement, area discovery, gathering and skinning instead of
registering unimplemented actions. It considers the last defeated combat target,
or the controller's selected corpse if that target is not nearby. Selection is
cached for two seconds and considers at most two identities, not a nearby scan.

Playerbots.Loot.Corpses.Enabled defaults off and is cached on load/reload. Actors
must be alive, out of combat/casting/transfer, unmounted, near a live controller,
and not actively resting. Candidates must be lootable, within native interaction
distance/LOS, and assigned to the bot or its group. A bounded eight-entry attempt
history supplies a 30-second per-corpse backoff, including inventory failures;
this is not the donor's full cached item-usage/loot-strategy policy. All native
allowed/owner item slots and coins are attempted; currencies are deferred.

One GUID-only, generation-checked mailbox bridges map-thread engine decisions
to the existing WorldSession unsafe/world update. It is not another scheduler.
The world consumer rechecks current follow-controller identity, full-control
authorization, feature gate, actor/corpse eligibility and absence of an unrelated
active loot window. No raw player, corpse, loot or packet pointer survives a tick.
Explicit movement/combat requests, death, transfer or disable cancel pending
work; stale completion cannot clear a newer request. Follow pause/resume uses
an explicit receipt flag, including completion before the next map update.

The required native core seam adds an optional typed LootResponse output to
Player::SendLoot. Ordinary callers keep the default null path. A provided result
is reset at entry and filled only by the normal permission-checked loot response
builder; generation, ownership, roll creation and looter registration remain
native. This avoids reading discarded socketless outbound packets or duplicating
permission logic. The world consumer uses ordinary native money/autostore
handlers and release, preserving HandleLootOpcode's post-open cast/aura
interrupt effects. Only ALLOW_LOOT/OWNER slots are attempted; roll, locked and
master slots are skipped. No manual money restoration, item awards, loot-GUID
overwrite, Wrath response decoding or synchronous module SQL is used.

The action's true result means request accepted. PB-LOOT logging means native
opening accepted, not storage success; inventory failures and awards remain
native. Four pure tests cover strategy wiring, bounded mailbox lifecycle,
cancellation/stale completion, follow-resume signaling and backoff/wrap handling.
Native storage, sharing/rolls, full bags and rest/follow timing still
require further bundled evidence; earlier 119-test evidence is historical.
Windows and Linux worldserver/tests-common builds each passed all 123 tests
after the final interruption guard and test-only GUID link/constructor
corrections. Linux's completed larger rebuild was followed by an incremental
current-source check and another full test pass.

The 2026-10-01 outdoor mixed-party check recorded native corpse opening by all
four bots and 17 accepted Mage offensive casts. The party returned to follow
after kills, and bot save/logout and server shutdown completed cleanly. Opening
logs do not prove item awards or roll resolution. No accepted Priest healing
casts were recorded. Food/water had not been provisioned and the level-20 Mage
had no armor spell, so rest and armor were not exercised. The reused Warriors
were level one; the recovery harness now requests native offline level
normalization before admission. That fixture correction is not runtime-verified.

## Native companion group-loot preference - 2026-10-01, development

Loot source review refreshed upstream master at
7bae1b5c58c76a0aa20381155edc08096d1485b2 and inspected LootNonCombatStrategy,
AvailableLootValue/HasAvailableLootValue, LootObjectStack, LootAction and
LootRollAction. Full corpse looting and item-usage-based rolling are not ported.
In particular, donor StoreLootAction depends on an outbound loot response that
server-origin Cata sessions currently discard. No Wrath response layout or
unsafe map-thread Group vote calls were copied.

The first implementation reuses Cata's native Player pass-on-group-loot preference
behind Playerbots.Loot.PassOnGroupLoot (default off, cached on load/reload).
Session-owned state captures/restores the prior preference on enable/disable;
it holds no Player/Group/Roll pointers. It applies to new rolls, not existing
pending rolls, and does not collect money/items or evaluate equipment upgrades.
Ordinary players and other loot methods are not configured by this policy.

A matching native core correction is required: GroupLoot must count its initial
auto-pass votes exactly once. The follow-loot-rules quest branches in GroupLoot
and NeedBeforeGreed must also honor/count the native preference. This corrects
ordinary native opt-out as well, independently of the module flag. Reward
distance, eligibility, vote resolution and awards remain native. The older
published core pin lacks this correction; use matching development sources.

Four pure policy tests cover disabled noninterference, snapshot/restore,
reassertion and reenable. Native roll creation/resolution and all-pass timeout
behavior remain bundled runtime checks, not proven by these policy tests.
Windows and Linux worldserver/tests-common builds each passed all 119 tests
after the final core correction; whitespace checks passed in both repositories.

## Mage self-armor strategies - 2026-10-01, development

Adapts GenericMageNonCombatStrategy's MageBuffManaStrategy/MageBuffDpsStrategy
and MageArmorTrigger/MoltenArmorTrigger at donor
7bae1b5c58c76a0aa20381155edc08096d1485b2. The `bmana`, `bdps`, `mage armor`
and `molten armor` registry names and relevance 19 are retained. Both strategies
are registered; mutually exclusive triggers select from the current active
talent tree and learned spells on each evaluation, avoiding a stale login-time
armor choice. Arcane prefers Mage Armor; other/unspecialized bots prefer Molten
Armor, then learned Mage Armor, then Frost Armor. This is a bounded Cata policy,
not a port of full donor profiles or a claim of optimal endgame rotations.

Cata's unranked spell identities are Mage Armor 6117, Molten Armor 30482 and
Frost Armor 7302. The existing caster DBC checker covers these identities and
learn levels; Wrath's separate Ice Armor fallback is not copied. Unlike the
donor's any-armor guard, a changed spec may replace an existing different armor.
The native cast/exclusivity path owns replacement; no old aura is removed before
a successful cast. The action rechecks eligibility and uses ordinary cooldown,
GCD, mana and learned-spell validation. Combat, transfer, mounting/flight,
missing/dead/distant controller and active carried-food recovery suppress it.

Playerbots.Mage.Armor.Enabled defaults off and is cached at configuration
load/reload, with invalid module configuration forcing it off. It shares the
existing two-second noncombat engine cadence with rest, without adding a timer
or scheduler. Windows and Linux worldserver/tests-common builds each passed all
115 automated tests. The read-only local Cata DBC check also passed, confirming
Molten Armor at level 34, Frost Armor at 54 and Mage Armor at 68. Native armor
execution and spec-change replacement belong to the next bundled party check.

## Shared carried food/drink recovery - 2026-10-01, development

The next unpublished batch adapts UseFoodStrategy, scalar health/mana triggers,
and the inventory/usefulness relationship from UseItemAction at donor
7bae1b5c58c76a0aa20381155edc08096d1485b2. The donor `food`, `drink`, `low health`
and `low mana` registry names and relevance 3 are retained. The initial Cata
thresholds are 40% health and 20% mana; no cheat branch or inventory replenishment
is included. Playerbots.Rest.Enabled defaults off and is cached at configuration
load/reload, with invalid module configuration forcing it off.

Warrior/Mage/Priest contexts register the shared strategy. A two-second cached
inventory candidate stores only an item GUID; execution resolves it again and
checks that it remains carried, usable and eligible. Candidate selection reads
Cata's first valid on-use ItemTemplate effect, native food/drink categories and
matching regeneration auras. This first batch excludes combined-category feasts,
purchases, conjuring, potions and bank items. It does not infer regeneration from
Wrath category numbers alone.

Cata SpellCastRequest/PendingSpellCastRequest and native RequestSpellCast preserve
ProcessItemCast validation, binding, item-use hooks, cast-item consumption and
cooldowns. No Wrath wire layout, manual resource restoration or manual item
deletion is used. Requests are submitted only outside casting/GCD so no delayed
rest request is intentionally queued. A rest spell identity tracks the movement
pause; aura completion/failure, controller combat/range, death, transfer, module
disable and explicit movement/combat requests release it. Existing follow identity
is preserved; resume uses current formation/path logic. Only the recorded aura is
removed by rest cleanup. Native aura application supplies seated state.

The existing engine remains the decision owner. While rest is enabled, the map
adapter prevents duplicate engine decisions in the same update. Existing class
action gates prevent the new rest cadence from enabling unrelated combat/healing
routes. Priest healing and resurrection remain above rest relevance. Four new
pure tests cover strategy wiring, resource thresholds and eligibility, not native
item use or actual follow timing. Windows worldserver/tests-common built and all
113 tests passed. Linux worldserver/tests-common also built and all 113 tests
passed after the final cooldown/logging review. Live
execution remains for the next bundled mixed-party check. Earlier milestone
evidence below applies to the published snapshot, not this new batch.

## Linux build acceptance - 2026-10-01

Ubuntu 22.04, GCC 11.4, CMake 3.22 and Ninja built worldserver, authserver and
tests-common with Playerbots and AHBot enabled, Release configuration and both
core/script precompiled headers disabled. CTest passed all 109 tests. This run
required explicit array and map-store headers previously hidden by incidental
includes. Revision stamping was disabled for the mounted Windows worktree;
that build arrangement is not a change to runtime or source revision policy.
This verifies Linux compilation and headless tests, not a Linux realm/database
playtest, every toolchain, or full donor gameplay parity.

## Explicit map-store dependency - 2026-10-01

The GCC 11 build with core precompiled headers disabled exposed an undeclared
sMapStore and incomplete MapEntry in PlayerbotSessionBehavior's instance-entry
path. That source now includes DBCStores.h directly, which declares the store
and includes the native map-entry definition. No routing or gameplay behavior
changes. The subsequent incremental Linux build and all 109 tests passed.

## Ordinary-player managed lifecycle check - 2026-10-01

An ordinary client account created and logged into a Blood Elf Warrior. Native
character storage and the auth realm index confirmed the character and count.
With an explicit directional account link, the player discovered the configured
bot under MultiBot's My Bots roster and used its buttons to connect, disconnect
and reconnect. The harness observed native character online transitions and
cleared bot-account online state after disconnect; server logs recorded repeated
login/logout completion. The bot also accepted a party invitation and followed.
Human logout, explicit final bot cleanup and world/database shutdown completed
cleanly. No GM override was used.

An earlier fixture lost its processes without clean-shutdown evidence; its cause
remains unknown. Database recovery preserved the client-created character and
confirmed accounting before the corrected check reused it. The successful rerun
does not diagnose that stop. Harness password/name setup errors were corrected.
This check does not prove autonomous admission, factory player endpoints, every
permission-denial case, full donor UI compatibility or Linux build/runtime.

## Mage/Priest party buffs through the engine - 2026-09-30

Upstream master was rechecked and remains
7bae1b5c58c76a0aa20381155edc08096d1485b2. This adaptation follows
MageBuffStrategy in src/Ai/Class/Mage/Strategy/GenericMageNonCombatStrategy.cpp
and PriestBuffStrategy in src/Ai/Class/Priest/Strategy/PriestNonCombatStrategy.cpp,
plus their class buff triggers/actions. Registered `buff` strategies now select
`arcane intellect on party` and `power word: fortitude on party` actions through
the existing class engine. Wrath spell naming is retained for registry congruence;
Cata Brilliance/Fortitude spell and single/party aura IDs remain authoritative.
Priest buff relevance is reduced from donor 11 to 10 so existing light healing
and resurrection win. Divine Spirit, armor, consumables and wider buffs are absent.

The new default-off EnginePartyBuff gate selects this route instead of direct
Mage/Priest upkeep. Priest healing uses the same engine without a second buff
tick when its route is enabled. A per-trigger CalculatedValue caches only a
boolean for two seconds; actions recheck current targets, configuration and
native eligibility before casting. No Player pointer is retained across ticks,
no map-thread SQL or independent update loop is added. The existing direct
fallback uses the same aura definitions and candidate visitor, preserving its
target order and eligibility when the feature is off. Worldserver/tests-common
built with both modules enabled and all 109 automated tests passed. The three
new cases check strategy naming/handler priority and Cata aura definitions,
not native casts. The first link missed newly added files in the stale generated
source list; refreshing the existing CMake tree resolved it. Runtime confirmation
belongs to the next mixed-party check; this batch did not start a realm.

## Completed disposable factory batch - 2026-09-30

The corrected worldserver/authserver build passed all 106 automated tests.
The bundled CheckFactory runtime run then passed on a fresh disposable clone:
disabled mutations and absent schema rejected without aborting; missing dedication,
nonempty, privileged and online accounts rejected; enrollment confirmed the stored
record; native Mage creation returned a ready GUID after accounting completion.
An exact rerun repaired an intentionally stale realm count without creating or
changing the character. Conflicting intent rejected. Explicit managed admission
then completed login, save/logout and account-online cleanup, followed by clean
world/database shutdown. Only the clone received schema, account and config writes.

This supersedes the pending factory runtime statements in the earlier entries
below. The first failed run was a duplicate-key harness error; the second exposed
the fatal missing-table query corrected by metadata preflight. Ordinary client
creation, addon-managed lifecycle, broader gameplay, Debug runtime and Linux
build/runtime remain unverified by this server-only batch. No automatic account
creation, population, admission or player-control grants have been added.

## Optional-schema preflight correction - 2026-09-30

The missing-schema runtime case exposed a real server abort: MySQL error 1146
reaches TrinityCore's MySQLConnection::_HandleMySQLErrno fatal schema handler.
A missing optional table does not simply produce an empty query result. Factory
inspection and enrollment now check information_schema for the required column
names/types before referencing the module table. Missing/incompatible metadata
rejects the request without issuing that optional-table query. The check is not
cached, allowing an operator to apply the schema before a later command.

The core's fatal SQL error policy is unchanged. This preflight does not promise
to survive arbitrary concurrent schema changes or all database failures; apply
the documented schema before enabling mutations and do not alter it during
active requests. The previous runtime run proved creation/reuse/accounting but
stopped at a harness config error; the next run aborted at the missing-schema
case. Neither failed run established full managed admission success. The completed
preflight rebuild and fresh runtime rerun are recorded above.

## Factory runtime batch and typed SQL reads - 2026-09-30

The core's existing disposable lifecycle helper now has a bundled CheckFactory
scenario; no new service launcher is required by the module. It accepts an
explicit validated source checkout for a stopped seed, clones it, then uses the
current binaries and optional ownership schema only in that private clone.
Initial creation/reuse/accounting checks succeeded. The first run stopped at
managed admission because the harness generated duplicate Managed.Enabled INI
keys, causing native config reload to fail; it did not establish full admission
success. The harness now replaces the key and polls asynchronous factory status.

Field getters were also aligned with SQL metadata: account-online uses its native
byte getter, and aggregate/epoch/security/GUID numeric expressions are explicitly
CAST AS UNSIGNED with 64-bit getters. TrinityCore's TRINITY_DEBUG type checks
otherwise return zero for a mismatched getter; eligibility reads must not silently
permit an online/privileged account or ignore a decimal SUM. This is source-audited
Debug compatibility, not a completed Debug build/run. Corrected worldserver,
authserver and tests-common built and all 106 headless tests passed. A fresh
runtime rerun is pending; ordinary client creation/gameplay remain untested here.

## Explicit enrollment, native submission and reuse recovery - 2026-09-30

The next operator slice is now implemented in source under the same donor-shaped
RandomPlayerbotFactory owner. `Factory.Enabled` defaults off and requires valid
module configuration. Console-only `managed enroll`, `managed provision` and
`managed factory-status` use no addon/player endpoint. No credentials or native
account creation are introduced; accounts are pre-created through native admin.

Enrollment requires an empty ordinary account, no existing ownership record,
valid normalized intent, account expansion eligibility and successful native
account/character/name reads. It submits a plain auth INSERT...SELECT, rechecking
auth eligibility without replacing conflicting rows. World-thread callback
processing confirms the transaction and actual identity/intent row; a zero-row
successful transaction is not success. One evidence row per account is supported
in the initial optional schema. No schema migration or live writes were run.

Provisioning consumes current verified evidence/identity, then calls native typed
creation for an empty account. Exact reruns use World::BeginCharacterReconciliation:
native cache ownership and one matching database GUID are required, then a
dedicated world-owned context holds the account reservation through authoritative
realm-count reconciliation. It does not repeat character creation or save.
The core creates the receipt; the module only retains its const view. Status
reports native rejection, pending accounting, failed accounting with recovery
GUID, abandonment or confirmed readiness. Accepted requests are not completion.

Up to 16 account histories exist in process, with no overlapping pending attempt
on an account. Factory disable blocks new mutations but pending enrollment
callbacks continue. No auto-admission, roster/config mutation, player-control
grant, population, deletion or password change occurs. Enrollment is explicit
operator dedication, not a distributed ownership proof. It does not reserve
native sessions or certify cross-database atomicity; submission rechecks current
eligibility and obtains its own native reservation. Restart/rerun consults
persistent evidence and actual native identity rather than in-memory history.
See sql/README.md for commands and limitations. This slice built worldserver,
authserver and tests-common with both modules enabled and passed all 106 automated
tests, including the new bounded/overlap request-policy case. The preceding
reader slice passed 105 tests. Operator schema/queries/callback timing remain
runtime-unverified; pure policy tests do not certify native persistence.

## Persistent ownership reader and operator inspection - 2026-09-30

The module now provides an optional additive auth ownership schema and a native
read-only reader under the existing RandomPlayerbotFactory owner. This is Cata
adaptation around the donor's account selection, not a copied bulk creator.
Records bind an explicit operator dedication to native account ID/name/join epoch,
realm, evidence version and exact character intent. They contain no credentials.
No rows are inferred from name prefixes or config links. The schema is manual,
not registered in the core updater, so disabled/module-off builds need no table.

`Playerbots.Factory.InspectionEnabled` defaults off and also requires valid module
configuration. Console-only `server playerbotdev managed inspect <account ID>`
reads the record, validates account/profile eligibility and compares native
characters/name availability. Aggregate queries return a row even for zero
characters; missing results reject rather than silently meaning empty. SQL uses
numeric IDs and native escaping for the normalized name. Query failures never
permit creation or reuse. These are synchronous operator reads, not AI ticks.

The diagnostic never writes, enrolls, provisions, updates the roster or admits
bots. Create/reuse results remain drafts. Other-realm counts are auth-index
evidence, not verified remote character databases; administrator dedication and
native validation remain necessary. Native character limits/class unlocks and
submission-time race checks belong to the creation path. Enrollment and rerun
accounting recovery remain the next work. See sql/README.md for the schema and
validation boundary. No migration or account/character mutations were executed.
Two added headless cases cover evidence identity/version binding and missing
fields. This reader slice built worldserver/authserver/tests-common and passed
all 105 automated tests. Schema/query runtime validation remains unperformed.

## Provisioning accounting and ownership decisions - 2026-09-30

Creation-only contexts now keep their account reservation through a separate
Reconciling receipt phase. The original native realm-count transaction is awaited
before a corrective write, preventing the two writes from overtaking each other.
After native character success or rejection, the context reads authoritative
COUNT(guid) without GROUP BY (so zero characters still yields a result row), then
awaits a LOGIN_REP_REALM_CHARACTERS transaction. Missing queries, out-of-range
counts and failed corrective commits yield AccountingFailed, not ready success.
An original failed optimistic write can be repaired after the character result
is known. Ordinary client creation retains its existing transaction behavior.

The provisioning API returns a const receipt view. Ready GUID access stays zero
until character creation and accounting both succeed; a separate committed GUID
accessor retains evidence for recovery after accounting failure or abandonment.
This is not a cross-database atomic transaction or a distributed account lease.
Crash/restart reconciliation and a reuse-specific operator path remain unwired.

`src/Bot/Factory/PlayerbotFactoryOwnership.h` defines module-side creation/reuse
decisions: verified dedicated ownership and an eligible profile are required;
only an empty account may create, and only one exact existing account/name/race/
class/gender identity may reuse. Prefixes, conflicts and extra characters do not
grant ownership. Inputs must be verified native/persistent facts, not addon data.
The evidence reader/storage and operator flow are not implemented by these pure
policy helpers. No live provisioning or new account/character writes occurred.

Worldserver, authserver and tests-common built successfully with both modules
enabled; all 103 automated tests passed. Six added cases cover accounting
readiness/failure/abandonment and exact-identity ownership/reuse decisions.
These are headless policy/receipt checks, not native database timing evidence.

## World-owned native provisioning contexts - 2026-09-30

World::BeginCharacterProvisioning now owns an explicit CharacterCreation
WorldSession context, separate from clients and admitted server-origin bots.
It has no socket, character admission GUID or bot hooks, never enters m_sessions,
and skips account-online writes on destruction. It cannot be initialized or
admitted through normal session APIs. Native creation queries/transactions are
pumped on the world thread; the context remains owned until its receipt is
terminal. At most 16 contexts exist, with one account reservation each.

Existing sessions, duplicate reservations, shutdown, online account profiles,
privileged accounts in any realm and banned accounts reject new provisioning.
Human and bot admission cannot replace a pending context. The callback loop
snapshots account keys rather than holding unordered-map iterators through
scripts that could submit another request. World destruction abandons unfinished
receipts; it does not prove a submitted transaction was cancelled. KickAll does
not destroy an executing creation owner or prematurely release its reservation.

This is a trusted native service, not a player/addon/console endpoint. Its
caller must establish dedicated-account ownership and feature policy. No module
caller, account creation, configuration switch, identity adoption or live database
write was introduced in this batch. Factory ownership/reuse and realm-count
reconciliation remain prerequisites to exposing the default-off operator flow.
The account profile uses a prepared read of existing auth tables; no migration
or copied character-row writes are involved. Reservation/profile policy tests
do not validate native callback timing or persistence.
Worldserver, authserver and tests-common built successfully with both modules
enabled; all 97 automated tests passed, including the two new policy cases.
No runtime provisioning or database migration was performed.

## Typed native character creation and completion seam - 2026-09-30

Cata's HandleCharCreateOpcode now decodes the packet and calls
WorldSession::BeginCharacterCreation with a copied CharacterCreateInfo. The
existing validation/query/save body is retained in CreateCharacter; creation
rules, native Player cleanup, scripts, realm-count submission and character
cache ordering are not replaced by module SQL. CharacterCreateInfo has a typed
constructor matching the donor factory's input shape.

NativeCharacterCreationReceipt distinguishes pending, native rejection,
successful character commit/cache publication with GUID, and abandoned owner.
It is world-thread-only and contains no session/Player pointers. One request
cannot overwrite another pending request, and terminal outcomes do not change.
An abandoned transaction may still commit; this is not a rollback receipt or
an atomic character/login database result. The existing realm-count transaction
still has its separate native submission/completion semantics.

Active/loading players and existing server-origin bot sessions cannot begin
creation through this entry point. The factory does not call it yet: temporary
provisioning ownership, callback pumping, account policy, reuse/reconciliation
and the default-off operator endpoint remain the next slice. Do not construct
a fake client session or fabricate a create packet to bypass that work.
The ordinary client creation path now shares this seam and needs inclusion in
the next integrated disposable creation/lifecycle check. Receipt-only unit
checks do not prove a successful database transaction or normal client creation.
Combined worldserver/tests-common build passed with both modules enabled;
all 95 automated checks passed. A source-body comparison against the current
core HEAD confirmed that native creation validation/query/save logic differs
only in success GUID reporting. No live character creation was performed.

## Initial native factory draft - 2026-09-30

The new `src/Bot/Factory/RandomPlayerbotFactory.{h,cpp}` adapts the appearance
selection in upstream RandomPlayerbotFactory::CreateRandomBot at
`7bae1b5c58c76a0aa20381155edc08096d1485b2` (upstream master rechecked today).
It retains the donor factory name/placement and author notices, but deliberately
stops before Player/session allocation or database writes in this first slice.

Cata uses RaceID/SexID/BaseSection/VariationIndex/ColorIndex instead of the
donor CharSections field names. The adapter checks native race/class start data,
world expansion and disabled-creation masks, filters player/DK appearance flags,
matches face skin and hair/facial colors, and delegates final validity to
Player::ValidateAppearance with creation mode enabled. The no-facial-hair
exceptions match that native validator. Selection is deterministic for the
read-only draft, not a random-population implementation.

`PlayerbotAppearance.h` bounds the search and rejects missing/oversized data
instead of randomly indexing empty donor arrays. Five regression cases cover
missing data, field/color mapping, rejected alternatives, native-call bounds
and mismatched-color failure. The console-only `managed appearance` command
reports the draft without implying account/name/limit/DK-unlock eligibility.
No new runtime admission gate, creation endpoint or identity adoption is added.
The full provisioning path and Cata DBC runtime check remain pending.
Combined worldserver/tests-common build with both modules enabled passed;
all 93 automated checks passed, including the five factory regressions.

## Windows MultiBot startup and basic controls - 2026-09-30

The disposable Windows Cata client loaded the full patched donor addon. The
player confirmed no remaining Lua errors and working Stay, Follow and the
main Attack button. Initial attack requests reached the server but were
rejected outside the existing 25-yard owner/bot target gate; moving closer
confirmed engagement. That gate is temporary companion behavior, not the
intended final movement/range contract. This check does not establish managed
roster UI, lifecycle completion, ordinary-player permissions or other addon
feature families.

Two startup fixes were added to the reproducible compatibility patch:
LibDataBroker must load before LibDBIcon, and the spellbook uses Cata's
GetMacroIcons caller-owned table instead of removed GetNumMacroIcons and
GetMacroIconInfo globals. The latter follows the 4.3.4 Blizzard_MacroUI source
(RefreshPlayerSpellIconInfo). Candidate and installed modified Lua passed
Lua 5.1 syntax checks; patch application against the pinned donor passed.

## Managed capability advertisement and Windows client staging - 2026-09-30

HELLO now advertises ALT_ROSTER_V1 and BOT_LIFECYCLE_V1 only when the validated
player lifecycle service is enabled. Other capabilities remain absent. This
reports service availability, not requester permission: roster, start/stop and
poll authorization remain independently checked. Disabled/invalid module
settings still produce an empty capability set. The exact advertised list has
an automated regression check; combined worldserver build and all 88 checks
passed. Authserver was also built for the isolated runtime check.

The compatibility candidate was copied into a previously absent MultiBot
directory in the Windows Cata test client; no existing addon was overwritten.
Only addon source/assets/license were copied, not Git metadata or personal
saved variables. Actual client startup and integrated behavior remain pending.
Linux compatibility is required for the server/module, not the client addon.

## Portable handoff and donor response checks - 2026-09-30

The source review found no Windows API dependency in mod-playerbots/src or its
module CMake entry point. The core's modules/CMakeLists.txt already installs
module configs under CONF_DIR/modules on Unix. This is a source-level finding,
not a Linux build certification. The module README now distinguishes the
Windows-verified build from the pending Linux checkpoint and documents plain
CMake/CTest entry points. Addon preparation uses relative Git/Lua commands;
neither path depends on optional local PowerShell test helpers.

The Lua 5.1 mock now exercises the patched donor's actual response parser:
unexpected-sender rejection, encoded ALT_ROSTER entry decoding, batch-count
integrity, connect pending then ONLINE, and disconnect pending/STOPPING then
OFFLINE. Callbacks do not report completion for pending replies. Timers are
captured rather than run. Syntax and expanded mock checks passed. No C++ or
native runtime behavior changed in this documentation/test slice; the prior
combined build remains at 87 passing checks. Real client UI/native lifecycle
authorization and a clean Linux build remain separate validation work.

## Initial Cata client compatibility candidate - 2026-09-30

`addons/MultiBot/cata-compat.patch` applies to the refreshed upstream addon
`1eac0d9106b8cdf0a79da3974ee1f516f8ca3fbc`. It retains the complete donor
addon/UI/protocol implementation and upstream licensing, rather than replacing
it with a custom client. The patch changes Interface to 40300, keeps the older
party/raid roster events instead of GROUP_ROSTER_UPDATE, and registers MBOT
before sending. Missing/failed registration makes Comm.Send return false.
An isolated candidate checkout was prepared; installed client addons and saved
variables were not changed. The patch passes Git's application check against
the immutable donor revision.

The donor's main attack button in `UI/MultiBotAttackUI.lua` sends
`do attack my target`, not bare `attack`. That exact alias now routes to the
same existing attack request; role-filtered forms remain unsupported rather
than accidentally commanding every bot. Combined worldserver build and all
87 automated checks passed with the expanded command assertions.

Lua 5.1.5 syntax checks passed for all 127 candidate addon Lua files and the
new communication test. The test loads the actual patched donor Comm code
with mocked WoW globals and checks successful/failed/missing prefix registration,
self-whisper, party/raid selection, HELLO and PING. This does not prove real
frame-event compatibility, UI loading or client-server interaction. Prefix
registration and event choices still need confirmation in the Cata client.
Class/spec/talent data and unsupported feature UIs are not ported by this patch.
Keep optional capability advertisement off until that integrated pass.

## MultiBot basic group-control transport - 2026-09-30

Refreshed MultiBot-Chatless upstream default branch to
`1eac0d9106b8cdf0a79da3974ee1f516f8ca3fbc`. `Core/MultiBotEngine.lua`
ActionToTarget/ActionToGroup/ActionToTargetOrGroup route basic bot actions by
normal whisper/party/raid chat. `UI/MultiBotLeftCoreUI.lua` emits stay/follow
through this path. The bridge's COMBAT endpoint instead handles combat
strategy toggles and wait-for-attack settings; POSITION handles disperse.
Neither is repurposed as a generic attack/follow endpoint in this port.

The existing bot-whisper parser is now shared with a group-chat hook for
follow, stay/hold, attack and stop/cease. Only native party/raid channels,
in-world human senders and current group membership are eligible. /party in
a raid reaches only the sender's subgroup; /raid reaches the raid. BG and
addon-language messages are excluded. Each selected bot must be in the routed
group and pass existing full-control checks; PlayerbotControl::Dispatch checks
again before posting a request. Managed account links are not a substitute
for temporary party control. Normal human chat delivery is preserved.

The group reply summarizes queued requests and rejections, not completed
movement or combat. Strategy mutation, flee, disperse, loot and other donor
commands remain unsupported; unrecognized text does not dispatch anything.
No invented addon opcode, console invocation or new lifecycle path was added.
The combined worldserver build and all 87 automated checks passed, including
shared command-vocabulary and party-versus-raid routing checks. Integrated
runtime authorization and MultiBot client behavior remain unverified.

## MultiBot offline managed roster - 2026-09-30

Upstream bridge HEAD was rechecked at
`1da05982e478cb00e0b6c87314afe7e0e9653ffb`. The protocol basis is
SendAltRosterPackets and ConsumeAltRosterRequestRateLimit in
`src/MultiBotBridge.cpp`, checked against the addon's strict batch reader.
`GET~ALT_ROSTER` now returns donor-compatible BEGIN/ENTRY/END frames:
count/truncated boundaries and guid/encoded-name/class/level/ONLINE-or-OFFLINE
entries. The batch is bounded at 128 entries and each native message at 250
bytes; omissions and overflow set the truncation flag rather than pretending
the batch is complete. Entries are ordered by native character GUID.

Cata sources this list from PlayerbotManagedControl::ListFor, not the donor's
same-account SQL query. Only explicitly configured, account-linked characters
with current authorization appear, including offline identities; account IDs
are not transmitted. An empty authorized list produces a valid empty batch.
Loaded native receipts remain ONLINE while stop is pending and become OFFLINE
after closure. Loading is OFFLINE for this two-state discovery schema; lifecycle
polling supplies the more precise CONNECTING state.

The donor's four roster requests per two seconds is enforced using the bounded
account-keyed requester storage already used by mutation protection. Rejected
queries return ERR without starting a batch. Client adaptation, gameplay
endpoints and integrated live validation remain pending; optional capabilities
are still not advertised. No lifecycle scheduler, SQL or account provisioning
was added.
The combined worldserver build and all 85 automated checks passed. The new
checks exercise roster framing, truncation, presence and query-rate boundaries;
live authorization and client behavior still need the integrated test.

## MultiBot managed lifecycle endpoints - 2026-09-30

The same bridge upstream HEAD was rechecked and remains
`1da05982e478cb00e0b6c87314afe7e0e9653ffb`. Protocol basis:
HandleBridgeOpcode, SendBotLifecycleResultPacket, SendBotLifecycleStatePacket
and the donor mutation-token/rate contract in `src/MultiBotBridge.cpp`.
Requests now include `RUN~BOT_CONNECT~guid~token`,
`RUN~BOT_DISCONNECT~guid~token` and `GET~BOT_LIFECYCLE_STATE~guid~token`.
Replies retain donor BOT_LIFECYCLE/BOT_LIFECYCLE_STATE field order and encoded
names. The adapter calls PlayerbotManagedControl, never console commands.

Every mutation rechecks existing managed admission policy; every poll rebuilds
the authorized list. Revoked links and other-party control expose neither names
nor receipts. FORBIDDEN replies are access denials, not proof a bot logged out.
Accepted starts/stops report PENDING. Only native receipt transitions establish
ONLINE/OFFLINE. Pending teardown uses donor-compatible CONNECTING with STOPPING
reason because the donor poll parser has no DISCONNECTING state. Pre-teardown
login failure also remains pending until the session is closed.

Replay/rate storage is world-thread-only and account-keyed, without retained
Player/session pointers: 64 mutation attempts per two seconds, 320 retained
tokens per account for two minutes, at most 256 requester accounts. Inactive
accounts expire after ten minutes; full storage rejects rather than evicting
live replay protection. Native addon throttling still applies to all requests.
Unlike donor manager state, Cata receipts supply completion and reserve native
session capacity; no parallel pending-login scheduler is introduced. Repeat
connects with a different token currently use native admission rejection rather
than the donor's ALREADY_ONLINE/ALREADY_CONNECTING success shortcuts.

BOT_LIFECYCLE_V1 is deliberately not advertised yet: offline ALT_ROSTER,
client adaptation and integrated validation remain pending. Gameplay endpoints
are also pending. The automated checks cover parsing, receipt-state mapping and
bounded replay/rate behavior, not live lifecycle authorization.
The combined worldserver build and all 81 automated checks passed for this slice.

## Initial MultiBot transport - 2026-09-29

Refreshed `Wishmaster117/mod-multibot-bridge` upstream default branch at
`1da05982e478cb00e0b6c87314afe7e0e9653ffb`. The protocol basis is
`MultiBotBridge.cpp` HandleBridgeOpcode, SendAddonPacket, BuildRosterPayload
and the 255-byte envelope budget. This is a small Cata adapter of that contract,
not a wholesale copy of the WotLK bridge implementation.

Cata decodes the MBOT prefix separately; requests are `HELLO~1`, `PING~token`
and `GET~ROSTER`. Replies use native Cata addon packets sent privately to the
requester, including for party/raid requests. The core seam runs on the world
thread after native channel, throttle and routing checks. Self-whispers and
valid non-BG party/raid channels are supported; other recipients/channels
keep native routing. Module-off builds have a no-op seam.

`Playerbots.MultiBot.Enabled` is default-off and fails closed with invalid
module configuration. Handshake advertises an empty capability set. Roster
uses existing full-control authorization and the donor's name/class/level/
map/alive/health/mana columns; it does not disclose accounts or offline
identities. Oversized rosters fail explicitly rather than silently truncating.
No database or lifecycle mutation is reachable here. Donor command endpoints,
managed lifecycle polling, larger roster framing and the Cata client addon
adaptation remain pending. Automated parser checks do not prove client behavior.
The combined worldserver build and all 77 automated checks passed for this slice.

## Managed player lifecycle authority - 2026-09-29

`Bot/Cmd/PlayerbotManagedControl` supplies world-thread ListFor/Start/Stop for
later player transports. It re-resolves the configured identity, native
character cache, requester session and current party relationship on each call.
Accepted native requests return an immutable view of their lifecycle receipt.
No Player/session pointers survive a request. The list omits managed account IDs.

The source basis is `PlayerbotMgr.cpp` account-link admission and
`Mgr/Security/PlayerbotSecurity.cpp` master/GM relationships at upstream master
`7bae1b5c58c76a0aa20381155edc08096d1485b2`, refreshed 2026-09-29. The Cata
adaptation uses a bounded directional config list of humanAccount:botAccount
pairs instead of importing the donor's linked-account database table. Same-account
multicharacter admission is excluded because native Cata owns one session per
account. Guild/random-account, gearscore and population eligibility are deferred.

Player lifecycle access is separately default-off. Ordinary access requires a
valid managed identity, an in-world human, trusted link and same faction; listing
or stopping a grouped bot additionally requires existing full party control.
GM overrides do not bypass the feature gate, native identity or human checks.
Managed.Enabled gates new starts, while authorized list/stop remains possible
with admission disabled. Existing console operations retain their separate
authority. An account link never grants gameplay control or adopts a master.

Four new regression cases cover policy prerequisites, link/party separation,
direction and revocation, and malformed/excessive links. The combined worldserver
build and all 75 tests passed. Client transport and native runtime authorization
checks remain pending; this slice introduces no new player command or endpoint.

## Managed lifecycle receipts - 2026-09-29

The latest managed admission now retains a session-owned receipt containing
atomic flags and no Player/session pointers. Core publishes native login
completion, known login failures, explicit stop requests and final session
closure after logout/save returns. The configured roster retains the receipt
after session deletion and across reloads of the same account/character binding.
A new admission gets a new receipt, isolating late observations of old attempts.
`managed list` uses this state instead of interpreting any non-null Player as
a completed login. Stopping an existing development session also captures its
receipt; stop remains available with managed admission disabled.

The source basis is `PlayerbotMgr.cpp` and
`Script/WorldThr/{PlayerbotOperations.h,PlayerbotWorldThreadProcessor.cpp}` at
upstream `7bae1b5c58c76a0aa20381155edc08096d1485b2`. The audit found the donor's
load, queued registration and native logout to be separate boundaries; an
immediate command `ok` is not an admission result. This receipt is a Cata-native
adaptation of those boundaries, not a copied donor operation queue or a durable
database ledger. It does not add account creation or ordinary-player connect
permission. Native asynchronous database commit success is outside its contract.

Four regression cases cover successful login/exit, failure/cancellation during
loading, unexpected closure/shutdown and receipt identity across reload/retry.
Build and automated validation are recorded in the core's current roadmap.
The configured lifecycle path still needs an integrated runtime check.

Current status, 2026-09-29: the scheduling kernel, session adapter, bounded
Warrior/Mage/Priest contexts, active roster and normal whisper controls are
integrated. The release check below confirmed Mage offensive casts, Priest
healing and basic commands. The dated entries describe their state when added;
older "pending" wording is superseded only for the behavior explicitly tested.

The recorded local donor `8827dd6fcbb2bb25988787a40f06fc93daf8e02d` contains
upstream master `7bae1b5c58c76a0aa20381155edc08096d1485b2` plus five local
commits. The upstream commit is available from the public donor repository;
the local custom revision is not assumed to exist in that repository. Check
upstream master before new imports and document local additions separately.

The matching core's `doc/local/playerbots/PLAYERBOTS_REFERENCE_GUIDE.md`
records source authority and secondary Cata comparisons. Modern Playerbots
remains the architecture donor; ArkCORE NPC bots are a behavior reference only.
That reference review imported no code and does not establish feature parity.

Foundation extraction: 2026-09-26, from this fork's existing dirty Cata prototype
on base `efcf6ac83d11fdf4ce86a1b6f95c3b22dfaee14f`. The preserved pre-extraction
files are under ignored `build/module-foundation-baseline-20260926`. Source notices
remain on the moved helpers. The local GPLv2 mod-playerbots donor is pinned at
`8827dd6fcbb2bb25988787a40f06fc93daf8e02d`; no wholesale engine import occurred in
this foundation change.

Current donor relationships are adaptations of mechanisms, not source parity:

| Cata prototype | Upstream responsibility |
|---|---|
| Party invitation handling | `src/Ai/Base/Actions/AcceptInvitationAction.cpp` |
| Follow positions/catch-up and melee positioning | Base follow actions and movement/formation values |
| Starter class priorities | `src/Ai/Class/{Warrior,Mage,Priest}` strategies/actions |
| Spell attempts | Native Cata Spell validation; to become the imported cast action adapter |
| Session behavior bridge | Replacement seam for donor PlayerbotAI/context/state engines |

The extraction retains map-update behavior and world-update transfer acknowledgement.
After a worldport acknowledgement, it resolves the current Player again. It removes
one ineffective remembered-owner assignment immediately before a queued hold that
would clear that value anyway. No class priorities, spell IDs, health thresholds,
follow distances, invitation security policy or dungeon entry policy are changed.

Still to port: donor Engine/context registrations, packet/event adapters, normal
chat/security/controller policy, active-spec profiles, factories, managed population,
world progression and optional module APIs. Preserve names and responsibilities
while recording each Cata-specific change and original author/source when code is
actually imported. The module directory alone is not full upstream compatibility.

## First upstream registry import - 2026-09-26

Imported `src/Bot/Engine/NamedObjectContext.{h,cpp}` from the pinned donor above.
This is actual donor code, not a similarly named reimplementation. The upstream
notices are preserved, and the donor `AUTHORS.md` is retained here. Donor history
for this file includes Keleborn, NoxMax, Crow and bashermens; its latest modifying
commit is `917a22bc` by Keleborn. This is provenance, not a claim that one author
owns the entire file. Preserve donor authorship in any later publication/commit.

Cata deviations:

- Replace the broad `Playerbots.h` include with direct standard headers; keep
  native `Common.h` types. No full runtime, database, travel or population
  dependencies are needed for this registry slice.
- Delete copy construction/assignment for the four owning context/factory
  containers. A copied owning raw-pointer cache would double-delete its objects.
  Creator signatures, names, qualifiers, overrides, sibling lookup and cache
  behavior remain donor-compatible. These containers are not moveable either.
- Use `size_t`, not `uint8`, in `MultiQualify` iteration so 256+ components
  cannot wrap the loop counter. No broad qualifier parsing redesign is included.

Registration ownership remains upstream-shaped: shared lists own creator
contexts; each per-bot list owns its created objects; action-node factory list
results are uncached and caller-owned. Complete registration before map-thread
use, and keep shared lists alive until all their per-bot lists are destroyed.
No live registration mutation or cross-thread object sharing is added.

Seven new registry tests exercise qualification, separate per-bot caches, sibling
groups, later creator overrides, clear/destruction ownership, caller-owned nodes
and >255-component formatting. Enabled worldserver/tests-common build and all
41 CTest cases passed. The donor codestyle checker passed on an isolated copy of
the three new C++ files. clang-format was unavailable locally.

This registry is not wired into active gameplay yet. Next: import the event,
action/trigger/value/strategy and relevance queue dependencies, then Engine and
the per-bot state runtime. Activate one decision owner only after the closure
builds and lifetime/event adapters are reviewed.

## Upstream Event and NextAction import - 2026-09-26

Imported `Bot/Engine/WorldPacket/Event.{h,cpp}` and extracted `NextAction` from
`Bot/Engine/Action/Action.h` at the same donor pin. Original notices are preserved.
Latest modifying donor commits inspected: Event.h `9f386de7` (Keleborn), Event.cpp
`9e8fe5e5` (NoxMax), Action.h `dfbbbf84` (Keleborn). The module's retained AUTHORS.md
continues to supply full donor attribution; these commit names are provenance,
not exclusive authorship claims.

Cata deviations:

- Replace umbrella dependencies with native packet/GUID and standard headers.
- Keep the upstream Player-pointer constructor surface, but capture the owner's
  GUID immediately. Add GUID overloads for queued producers. `EventOwner.cpp`
  isolates native pointer capture and ObjectAccessor::FindPlayer resolution.
  No Player pointer is retained in the event, and no socket/client-origin session
  or authorization bypass is introduced. Resolved pointers are borrowed for the
  current call only; callers must obey native execution-context/lifetime rules.
- Native Cata uncompressed GUID serialization is uint64. `getObject` reads from
  an independent payload copy starting at zero, preserving the event's cursor;
  short payloads return an empty GUID. This is an internal GUID event contract,
  not a WotLK packet decoder or general validation of incoming Cata packets.
- Extract NextAction without importing incomplete Action/AiObject/Value classes.
  Name, relevance, ordered concatenation and duplicate retention are preserved.

Six new tests cover empty/named events, command copies, independent packet data
and cursors, owner/target identity, short GUID payloads, and NextAction merging.
Tests compile real native packet/GUID types through `game-interface`, but omit
the native owner adapter: no fake Player or replacement ObjectAccessor is used.
FindPlayer's in-world filter is source-inspected and the adapter compiles into
worldserver; logout resolution and cross-thread safety are not runtime-proven here.

Enabled RelWithDebInfo worldserver/tests-common builds and 47/47 CTest cases pass.
The initial test compile found ambiguous integer GUID constructors; fixtures now
use explicit uint32 values. Donor codestyle checks pass on the five new C++ files.
The module-off build was not rerun in this slice; optional test sources/libraries
are registered only in the enabled module branch. No server, DB, client, gameplay
decision owner, class priority or donor source was changed by this verification.

Next: complete the AiObject/value/action/trigger/strategy/relevance-queue dependency
closure and per-bot context before activating Engine. Events are not yet wired
to native chat or outgoing packet producers.

## Scheduling kernel import - 2026-09-27

The next donor slice imports the scheduling behavior from
`Engine/Action/Action.{h,cpp}`, `Engine/Trigger/Trigger.{h,cpp}`,
`Engine/Value/Value.h`, `Engine/Strategy/Strategy.{h,cpp}`,
`Engine/PlayerbotAIAware.h`, `Engine/AiObjectContext.{h,cpp}`, and
`Script/WorldThr/Queue.{h,cpp}` at the
same pinned donor revision. The default Strategy action-node factory is copied
from the donor; it retains names and fallback links. The code is built into the
optional module but is not yet the live decision owner.

This is the runtime-independent scheduling portion, not the complete upstream
classes. `Action`, `Trigger`, and `UntypedValue` retain donor-facing names
and scheduling methods but do not yet inherit `AiNamedObject`; player-bound
target/context/chat access waits for the per-bot `PlayerbotAI` and
live-context lifetime seam. `AiObjectContext` now owns separate per-bot
strategy, action, trigger, and typed-value registries. Its shared creator
tables are deliberately empty until class contexts are ported. Specialized
Unit, CreatureData and GUID
values, memory/log calculated values, performance monitoring and the full
`Engine` are also still absent. Donor class action/trigger implementations
cannot be dropped in unchanged yet.

Cata/local adaptations in this slice:

- `Event` remains the earlier GUID-owned Cata event, so queued baskets do
  not retain an owner Player pointer.
- The queue receives action expiry as a setting rather than consulting the
  donor global `sPlayerbotAIConfig`; future Engine setup must supply it.
  Its destructor and `Clear` release pending nodes. `Pop` transfers a node
  to the caller, and duplicate names keep the first event while raising
  relevance, matching donor behavior. Negative relevance can still be popped
  rather than becoming an unreachable queued node.
- Trigger force-rebuff inputs are supplied at `needCheck` by the future
  runtime, rather than dereferencing a missing donor `PlayerbotAI`. The
  out-of-combat buff-trigger bypass is retained.
- Cached values use Cata's native millisecond timer without donor
  PerfMonitor. A separate cache-valid flag avoids recalculating repeatedly
  when the first read occurs at millisecond zero.
- Unbound action nodes return their configured handlers instead of
  dereferencing a null Action. Real Engine registration must still bind
  actions before execution.

Focused tests cover ordering/duplicates, queue ownership, strategy fallback
links, trigger timing/rebuff gates, value cache/reset behavior, per-bot
registry isolation, typed lookup, and value save/load. This is a
build-verified preparatory slice; it does not change combat rotations or prove
the forthcoming `PlayerbotAI`/Engine lifetime bridge.

## Decision engine import - 2026-09-27

`Engine.{h,cpp}` and `Multiplier.h` now carry the donor's strategy assembly,
trigger/default-action scheduling, action-node factories, relevance multipliers,
prerequisites, alternatives, continuers, direct action execution, and execution
listeners. They use the context and queue above. Focused tests exercise action
ordering, failure fallback, trigger firing, and listener vetoes.

The Cata adapter takes a per-bot context and supplies queue expiry, rebuff
state, combat state, and iteration budget at its boundary. It does not
dereference `PlayerbotAI`, write donor perf logs, or own a `Player`/session.
The trigger interval is not reset after every tick, so the interval gate can
actually throttle checks. A minimal tick stops when the highest queued action
is below its threshold rather than looping on the same basket. Engine owns
its nodes, multipliers, and listeners; the context owns Action, Trigger, and
Strategy instances.

This engine is compiled but is **not active in gameplay**. At this milestone
the shared class creator tables were still empty. No Cata core hook was
changed in this slice.

## Session adapter and first Warrior context - 2026-09-27

The session-owned Cata `PlayerbotAI` adapter now gives donor-facing objects a
`GetBot` and current-target lookup. It stores a `WorldSession` reference and a
target GUID, not a `Player*`. `GetBot` resolves the current in-world Player and
checks the server-origin character GUID. Access is map-thread only.
`PlayerbotSessionBehavior` constructs the adapter before login, then lazily
creates the class context and engine when a Warrior Player is available. The
engine is destroyed before its context and adapter. Target GUID changes track
the current attack/cease path. The existing companion movement, healing, and
combat code remains the live owner. By default the old buff call also remains
active; `Playerbots.Dev.EngineWarriorBuff = 1` swaps only the out-of-combat
Warrior Battle Shout maintenance call to the new engine for a narrow live test.

`WarriorAiObjectContext` follows the donor's shared-creator/per-bot-cache
pattern for the first `nc` strategy, `battle shout` trigger, and `battle shout`
action. Names and trigger/action relationships come from the donor; the
Warrior's 6673 spell/aura check and cast use the existing Cata spell adapter.
This is a foothold, not the full upstream Warrior context or rotation. The
server build compiles the live Cata code. Unit tests do not link native Player
and spell-casting implementations, so the real cast required the gated live
test recorded below.

## Gated Warrior buff runtime check - 2026-09-28

A disposable local realm with `Playerbots.Dev.EngineWarriorBuff = 1` recorded
the engine route, an accepted Battle Shout cast on Testone, and auto-assist
engagement/return-to-follow during nearby combat. World/auth/MySQL shut down
cleanly. The user also saw Battle Shout in the client. This validates the
narrow live route; no broader Warrior rotation was enabled.

## Bounded Mage engine profile - 2026-09-28

`MageAiObjectContext` now registers donor-shaped `mage` and `frost` combat
strategies, an `enemy is close` trigger, and Cata-native Frost Nova, Fire Blast,
Frostbolt and Fireball actions. The active Cata talent tree selects `frost` only
for `TALENT_TREE_MAGE_FROST`; an unset, Arcane or Fire tree uses a deliberately
generic fallback, not a claimed full specialization rotation. The shared Cata
cast adapter still decides learned spell, resource, cooldown and cast legality.
The session switches the Mage combat decision owner only when the default-off
`Playerbots.Dev.EngineMageCombat` option is enabled.

This is build-verified, not gameplay-verified. CMake was regenerated to include
the new source, worldserver built successfully, and the existing 65 tests passed.
No client test or Mage gameplay claim follows from this patch. Batch-test it
with subsequent Priest/group work.

## Bounded Priest healing engine and living-party recovery - 2026-09-28

`PriestAiObjectContext` registers the donor's `heal` strategy, party-health
trigger names, and party-targeted Shield, Flash Heal, Heal and Renew actions.
The actions use the Cata spell validator and the existing lowest-health-first
candidate rule. `Heal` spell 2050 replaces the donor's WotLK Greater Heal
assumption in this narrow level range. The engine route is default-off behind
`Playerbots.Dev.EnginePriestHeal`; normal companion healing remains the default.

The session adapter now keeps a controller GUID rather than a Player pointer.
When the controller dies, the Priest can still consider living reachable group
members and itself while recovery is pending. An explicit hold clears the
controller and stops that automatic healing. This is not resurrection,
mana/rest management, or a complete Discipline/Holy/Shadow profile. The source
compiled into worldserver and all 65 existing tests passed; the gated healing
route and owner-death behavior are not client-tested yet. Combine those checks
with Mage and group control in the next representative party playtest.

## Invitation controller lifetime - 2026-09-28

An accepted human party invitation now records an invitation-owned controller
separately from the console-only follow command. The map update checks that
the inviter remains a party member; if the bot leaves or is removed, or the
inviter leaves, it requests hold and drops the party controller. Explicit
console follow overrides this lease and remains a development-only path.
Recovery after death and the existing dungeon transfer retain the invitation
lease when they reissue follow. This maps the donor's accept/leave-group
actions onto the current Cata session bridge; it is not the donor's complete
`PlayerbotSecurity`, new-master election, or normal chat command surface.
Compile verification is separate from the pending mixed-party client test.

## Priest out-of-combat resurrection - 2026-09-28

The donor's `PriestNonCombatStrategy`/`party member dead`/`resurrection`
relationship is now represented in the Priest context. The action attempts
Cata Priest Resurrection (2006) only for a nearby, visible, dead party member
with no outstanding request while the Priest is alive and out of combat. It
uses the existing Cata spell cast validator; no direct player revival is done.
When a server-origin party bot receives a resurrection request from a current
living party member, its session answers through Cata's normal resurrection
response handler, which retains the core's request and raid-charge checks.
Both parts are covered by the default-off `Playerbots.Dev.EnginePriestHeal`
switch. This is source/build validation pending the mixed-party client test;
released ghosts outside range, movement-to-corpse, and complete dead-state
strategy parity are not solved by this slice.

## Bounded Warrior combat engine - 2026-09-28

`WarriorAiObjectContext` now also registers donor-shaped generic `warrior` and
Protection `tank` combat strategies, with mutually exclusive combat strategy
selection by the active Cata talent tree. Both retain the current low-level
Victory Rush proc, Rend upkeep and Strike fallback. The Protection variant
adds Shield Slam when learned and Taunt only when the current creature targets
a party member other than the Warrior. All casts pass through Cata's native
spell validator; spell IDs are 34428, 772, 88161, 23922 and 355.

The new combat decision owner is default-off behind
`Playerbots.Dev.EngineWarriorCombat`; the existing Warrior combat helper
remains live otherwise. It is independent of the separately gated Battle
Shout route. This is not a complete Cata Protection rotation, stance manager,
pull strategy, multi-target threat planner or proof of dungeon tanking. The
next mixed-party playtest should exercise the gated Warrior, Mage and Priest
paths together instead of adding another one-spell client check.
The worldserver compiled and all 65 existing tests passed; none of those
tests establish actual Warrior damage or threat behavior in the client.

## Party-member death recovery - 2026-09-28

The follow-resume guard now requires the adopted human controller to remain
in the bot's party, rather than requiring that human to still be its leader.
This matches the invitation controller lease and permits leadership changes
without stranding a recovered bot. It still requires both players alive,
nearby, on the same map, and an actual human controller. The next party test
must verify resume after resurrection and hold after party removal.

## Mage ranged opener correction - 2026-09-29

The mixed-party Ragefire run routed Botmage through the new engine but logged
zero accepted offensive casts. The Mage action's target predicate required
`IsInCombat()` before the first spell. In this Cata core, a player's ranged
`Attack(target, false)` selects an attack target without itself entering combat;
the opening spell is what establishes combat. The engine predicate now allows
that opening cast while keeping native attack-target, distance, line-of-sight,
facing, learned-spell and cast-legality checks. This is a source-level correction
pending a later batched client check, not proof that the Mage rotation works.

## First PlayerbotSecurity relationship slice - 2026-09-29

`Mgr/Security/PlayerbotSecurity.{h,cpp}` adapts the donor security-level names
and its distinction between invitation and full control. The pinned donor is
`src/Mgr/Security/PlayerbotSecurity.{h,cpp}` at
`8827dd6fcbb2bb25988787a40f06fc93daf8e02d`; original notices and
`AUTHORS.md` remain. Recent donor history for those files includes NoxMax,
Keleborn and Crow; this is provenance, not exclusive authorship. The Cata
class evaluates current `Player`/session state
without retaining pointers or making a map-thread database query. The session
bridge exposes the invitation-adopted controller GUID to the policy.

This slice deliberately does not import the donor's random-account, gearscore,
guild, linked-account, battleground or chat-denial branches: their supporting
manager/config/data APIs have not been ported. For the existing development
roster, a normal same-faction human may invite an ungrouped bot, while only the
current human invitation controller in the bot's party receives full control.
Other group members receive talk level. The donor's GM override is retained.
The existing invitation flow now checks the invite level before accepting;
normal core party checks still decide whether the invitation can be formed.
The four-slot dev commands remain console-only and are not an addon-control
surface. Subsequent chat/bridge commands must call this same policy, not trust
client-provided bot names or group membership alone.

## Identity lookup and control dispatch - 2026-09-29

`World::FindServerOriginPlayerbot` resolves an active session by the server's
character GUID and verifies server origin; the old numbered slot lookup now
uses it plus the configured account check. This is a world-thread lookup over
active sessions, not a managed character roster or a character factory.

`Bot/Cmd/PlayerbotControl` provides a world-thread command boundary for a
future chat or MultiBot transport. It resolves that GUID, requires the full
`PlayerbotSecurity` level, and queues follow, hold, attack or cease into the
existing session mailbox. Attack requires the bot to be following the
requester. Separate party-controller follow/hold requests preserve the
invitation lease, unlike the explicit console overrides. At this step no addon
packet or chat command called the dispatcher; the following whisper transport
now uses it. A managed roster, richer donor security, the full donor command
parser and Cata addon-message handling still need their own ports.

## Active roster and first normal command transport - 2026-09-29

`PlayerbotRoster::ListActiveFor` enumerates current server-origin sessions and
returns only in-world bots for which `PlayerbotSecurity` grants the requester
full control. Entries carry server GUID, name, class and level and are sorted
for stable presentation. This is an active roster, not the donor's offline
account/character manager, factory, guild/friend roster, or random population.
It deliberately does not infer ownership from a shared group alone.

A PlayerScript observes ordinary whispers to admitted bots. Exact `follow`,
`stay`/`hold`, `attack`, and `stop`/`cease` words use `PlayerbotControl`; `list`
shows the requester's active controllable bots. Other whispers and addon-language
messages are untouched. Core chat checks still run before the script, and the
handler is registered only with the optional module. The command vocabulary is
a narrow Cata transport mapped to upstream Playerbots concepts, not a port of
the donor's full command parser. The addon bridge should consume the same
roster/control services through structured requests, not parse these replies.

## Integrated client checkpoint - 2026-09-29

The combined worldserver build passed before this check; the preceding existing
automated suite passed 65 cases. With the Warrior-combat, Mage-combat and
Priest-heal engine routes enabled in a disposable realm, four bots accepted
invitations and entered Ragefire Chasm. Logs recorded 13 accepted Mage offensive
casts, six Priest healing casts and one Fortitude cast. The human reported good
overall behavior and basic whisper/movement controls; all test processes shut
down cleanly after logout. This confirms the Mage opener correction and bounded
healing route. Warrior routing was observed without establishing tank threat.
The fixture human had GM privileges; this session did not independently verify
ordinary-player authorization/denial boundaries.

One attack request after stop was rejected with the bot selected instead of a
hostile unit. A later explicit Earthborer attack and casts were logged; the
specific resume interaction remains a command-UX follow-up. Resurrection,
controller death, removal/re-invite, full rotations and a dungeon clear were
not verified in this session. No additional playtest is required merely to
continue the manager/roster and addon infrastructure work.

## Managed existing-character roster, first slice - 2026-09-29

The source basis is upstream `src/Bot/PlayerbotMgr.{h,cpp}` at
`7bae1b5c58c76a0aa20381155edc08096d1485b2`: separate bot identity from
its temporary master, reject duplicates, and enter through native session
loading. This is an adaptation of that ownership contract, not a copy of its
query holder, guild/account-link permissions, command parser or random manager.
Those donor services depend on WotLK-specific systems not yet present here.

`PlayerbotManagedRoster` parses a bounded, unique set of configured
account ID/character GUID pairs. The settings are default-off. Core
`World::TryStartServerOriginPlayerbot` checks the existing character cache,
account ownership, security, current online state, session and player limits,
then uses the already established asynchronous Cata login path. It refuses to
replace a human session. The numbered development slots now call this same
entry point. A matching world-thread stop request uses the normal save/logout
path.

The combined worldserver build and 67 automated checks passed for this slice.
The configured admission path itself still needs a disposable runtime check
before describing its start/stop behavior as client-confirmed.

The first management transport is console-only
`server playerbotdev managed <list|start GUID|stop GUID>`. Listing reads the
native character cache and active session; it never treats a party controller
as an account owner. No ordinary player or addon connect permission is exposed
by this command. Accepted start and stop requests report admission or queued
exit, not confirmed completion. The enable flag gates new admissions; listing
and stopping configured bots remain available when it is off. A later
lifecycle manager needs durable
pending/completed/failed outcomes, human-facing authorization, account
provisioning and MultiBot mapping. This first slice has not had a live
managed-roster session test.
