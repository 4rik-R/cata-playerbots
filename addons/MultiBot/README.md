# MultiBot Cata compatibility candidate

This is a small reproducible patch, not a replacement addon or a completed
Cata port. It applies to the full upstream
[MultiBot-Chatless](https://github.com/Wishmaster117/MultiBot-Chatless) addon at
`1eac0d9106b8cdf0a79da3974ee1f516f8ca3fbc`. Keep its LICENSE, author notices,
libraries and assets. Do not apply this to the installed WotLK addon.

Use a separate checkout named MultiBot. Check out the pinned revision, then
apply `cata-compat.patch` with Git's patch checker first. The patch changes
the interface declaration to 40300, removes the newer GROUP_ROSTER_UPDATE
event while keeping PARTY_MEMBERS_CHANGED/RAID_ROSTER_UPDATE, and registers
the MBOT prefix. It also loads LibDataBroker before LibDBIcon and replaces the
removed indexed macro-icon API with Cata's GetMacroIcons table API.
Missing or failed registration disables Comm.Send. No global
WoW API is replaced, and upstream UI and protocol parsers remain intact.

## Portable preparation

Git and Lua 5.1 are sufficient; PowerShell is not required. The following uses
relative paths, with this module checkout named `mod-playerbots` in the working
directory and a new sibling `MultiBot` checkout. Use a fresh destination; do not
reuse or overwrite an installed addon.

```text
git clone https://github.com/Wishmaster117/MultiBot-Chatless.git MultiBot
git -C MultiBot checkout 1eac0d9106b8cdf0a79da3974ee1f516f8ca3fbc
git -C MultiBot apply --check ../mod-playerbots/addons/MultiBot/cata-compat.patch
git -C MultiBot apply ../mod-playerbots/addons/MultiBot/cata-compat.patch
luac -p MultiBot/Core/MultiBot.lua MultiBot/Core/MultiBotComm.lua MultiBot/UI/MultiBotSpellBookFrame.lua
lua mod-playerbots/addons/MultiBot/tests/test-cata-comm.lua MultiBot/Core/MultiBotComm.lua
```

Use Lua 5.1, not merely the system's default Lua version. Acquiring the Git/Lua
tools is separate from addon preparation. The Windows checkout verified here
does not establish Linux server compatibility or every WoW client platform.

Run Lua 5.1 `luac -p` on the three modified Lua files listed above. Run
`lua tests/test-cata-comm.lua <patched Core/MultiBotComm.lua path>` using the
test from this directory to check registration and channel selection with
mocked WoW globals. These checks do not validate real frame events or client UI.
The mock also drives the actual donor response reader through expected-sender
filtering, encoded offline roster entries, invalid batch rejection and pending
versus completed connect/disconnect responses. Its strategy case invokes the
five-second callback and verifies that a preceding aggregate ACK is accepted,
while a late ACK after client timeout is rejected. Other timers are only captured.
This is a protocol check, not a simulation of server latency or client rendering.

Before copying this candidate into a disposable Cata client's AddOns/MultiBot,
preserve any existing addon. No installer is provided and nothing is copied to
the client automatically. Leave unrelated addons and saved variables alone.

The 2026-09-30 Windows client check confirmed clean startup, Stay, Follow and
the main Attack button. Attack currently requires both bot and controller
within 25 yards of the selected hostile target; this is temporary server
behavior. The 2026-10-01 ordinary linked-player check then verified the My Bots
roster and button-driven native connect/disconnect/reconnect, with clean shutdown.
Left-click an offline bot to connect; right-click an online bot to disconnect.

For reproducing the integrated check: enable the server MultiBot transport, verify
addon loading and HELLO/PING, active/offline roster and authorized lifecycle
requests, then exercise basic group follow/stay/attack. The main attack command
`do attack my target` is now accepted by the server; role-filtered attack
commands remain unsupported. The enabled player lifecycle service advertises
ALT_ROSTER_V1/BOT_LIFECYCLE_V1. Optional strategy gates additionally advertise
STATE_FRAMING_V1 and STRATEGY_MUTATION_V1. Other UI requests remain unsupported or
disabled. WotLK class/spec/talent data, strategy and
inventory features are not made Cata-compatible by this loading patch.

Source and mock-test validation must be reported separately from client success.
The client target for this project is Windows Cata. Linux portability checks
apply to the server/module; Linux addon/client support is not a project goal.
