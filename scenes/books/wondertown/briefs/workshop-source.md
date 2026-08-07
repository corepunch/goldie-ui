# Workshop Floor canonical source brief

## Identity

- Source format and project: ZIL adventure, *Wondertown — The Last Toymaker*
- Room ID and display name: `WORKSHOP-FLOOR`, “Workshop Floor”
- Canonical description: Grandfather Tolliver's workshop is a giant, cluttered work environment seen at Pip's tiny scale. Golden sawdust covers the floorboards; an enormous climbable workbench holds tools and half-finished toys; a missing key is signaled by an empty brass hook and frayed string; a pet door glows with moonlight; a cuckoo clock conceals the study route; and a rusty folding ladder controls access to the storage loft.
- Source root: `/Users/igor/Developer/orca/samples/Book/libs/zilscript/books/wondertown`
- Extraction date: 2026-08-07
- Primary sources: `dungeon.zil:49-58`, `dungeon.zil:160-254`, `actions.zil:255-279`, `actions.zil:347-425`, `work/PROSE.md:5-8`

## Exits and adjacency

| Direction | Destination | Condition | Presentation consequence | Source |
|---|---|---|---|---|
| East | `TOOL-BENCH` | Always | The workshop continues into an enormous tool-and-climbing zone. | `dungeon.zil:53`, `work/PROSE.md:8` |
| North | `SNOWY-ALLEY` | Always through pet door | Main door must contain a Pip-sized flap with moonlight/cold-air cue. | `dungeon.zil:54`, `actions.zil:381-385` |
| Up | `STORAGE-LOFT` | `LADDER-OILED` | Folding ladder and lifting mechanism need visibly distinct stuck and usable states. | `dungeon.zil:55`, `actions.zil:406-425` |
| In | `TOLLIVER-STUDY` | `STUDY-ACCESS` | Workshop clock swings outward and reveals a narrow stair behind it. | `dungeon.zil:56`, `actions.zil:222-232`, `actions.zil:391-398` |

## Canonical inventory

| ID | Name | Parent/location | Visibility | Interactions | Required states | Scale/relationship facts | Sources |
|---|---|---|---|---|---|---|---|
| `KEY-HOOK` | Brass key hook | Fixed to wall | Room landmark | Examine; cannot take | Empty throughout workshop opening | Wall-mounted; frayed string attached | `dungeon.zil:162-169`, `actions.zil:349-353` |
| `KEY-STRING` | Frayed string | Hanging from key hook | Clue detail | Examine; take | Present or taken | Small, dangling, chewed ends | `dungeon.zil:228-236`, `actions.zil:402-404` |
| `WORKBENCH` | Enormous wooden workbench | Workshop floor | Hero landmark and climb route | Examine, search, look under, climb | Persistent; oil can location changes | Towers over Pip; surface is a landscape; carved leg, drawer handles and joints are footholds | `dungeon.zil:171-179`, `dungeon.zil:60-67`, `actions.zil:355-367` |
| `OIL-CAN` | Tiny copper oil can | Under workbench, then Pip inventory | Discoverable reward | Examine, take, shake, oil mechanism | Under bench; carried; potentially consumed narratively but retained mechanically | No bigger than Pip's thumb; half full; catches light in shadow | `dungeon.zil:181-189`, `actions.zil:355-373` |
| `SAWDUST` | Soft golden sawdust | Floorboards | Room-wide atmosphere | Examine; cannot take | Persistent | Covers floor like a blanket; cedar curls/shavings occur on bench | `dungeon.zil:191-198`, `actions.zil:375-379`, `work/PROSE.md:5-8` |
| `PET-DOOR` | Small wooden pet door | Cut into bottom of main workshop door | Exit landmark | Examine, open, traverse | Flap closed or swinging | Just right for Pip or a fox toy; moonlight and cold air seep through | `dungeon.zil:200-207`, `actions.zil:381-385` |
| `SWEEP-BROOM` | Pip's tiny broom | Leaning against workbench | Visible personal prop | Examine, take | Leaning or carried | Pip-sized, worn bristles, smooth handle | `dungeon.zil:209-217`, `actions.zil:387-389` |
| `CLOCK-FACE` | Old wooden cuckoo clock | Workshop wall | Hero wall landmark and secret entrance | Examine, listen, wind via Old Tick logic | Closed; urgent after key found; swung open after study access | Nearly midnight; hides narrow stair and latch behind it | `dungeon.zil:219-226`, `actions.zil:214-233`, `actions.zil:391-400` |
| `LOFT-LADDER` | Folding wooden loft ladder | Workshop floor to storage loft | Major traversal landmark | Examine, oil, climb/raise | Rust-stuck; oiled and raised/usable | Broad Pip-sized rungs; leads upward to loft | `dungeon.zil:238-245`, `actions.zil:268-277`, `actions.zil:406-411` |
| `LADDER-MECH` | Rusty iron lifting mechanism | Attached to loft ladder | Puzzle mechanism | Examine, turn, oil | Frozen with rust; oiled and moving freely | Pip operates it by hand; raises folding ladder | `dungeon.zil:247-254`, `actions.zil:257-267`, `actions.zil:413-425` |

## Story beats to support

| Beat | Actor and target | Preconditions | Result and staging requirement | Sources |
|---|---|---|---|---|
| Empty-hook investigation | Pip examines hook/string | Initial state | Hook and string must read at Pip scale without competing with clock/window. | `actions.zil:349-353`, `companion.zil:125-145` |
| Oil-can discovery | Pip looks beneath workbench | Oil can still in room | Bench must have readable under-space and a small copper glint. | `actions.zil:355-363` |
| Workbench climb | Pip climbs workbench | Always | Carved leg, drawer handles and joints must form plausible footholds to tabletop. | `actions.zil:364-367` |
| Ladder repair | Pip oils mechanism | Carries oil can; not yet oiled | Mechanism changes from rust-stuck to movable; ladder becomes a valid route upward. | `actions.zil:257-277`, `companion.zil:107-123` |
| Loft ascent | Pip raises/climbs ladder | `LADDER-OILED` | Both base mechanism and loft destination must fit in a single readable traversal composition. | `companion.zil:92-105` |
| Clock reveal | Nutmeg or tin soldier opens hidden latch | Key found plus appropriate helper; no study access | Clock swings outward, revealing narrow stairs behind it. | `actions.zil:214-233`, `companion.zil:57-90` |
| Workshop return/endgame | Pip returns after key winding | `KEY-WOUND` | Same room becomes warmer with returning magic without losing geography. | `companion.zil:1029-1043` |

## Global visual facts

- Pip is tiny relative to ordinary workshop furniture; the bench “towers” and its top becomes a navigable landscape. `dungeon.zil:64`, `dungeon.zil:174-177`
- The workshop belongs to a toymaker and contains tools, half-finished toys, repair books, shavings and adjacent tool/counter zones. `dungeon.zil:64`, `dungeon.zil:73`, `work/PROSE.md:5-18`
- It is near midnight. Moonlight enters through the pet door, while the clock ticks and points near midnight. `actions.zil:383`, `actions.zil:391-400`
- The room is a tutorial hub whose visual hierarchy must reveal workbench, empty hook, oil can discovery, pet door, clock and ladder without making them look equally important. `work/MAP.md:54-60`
- The storage loft and later study imply substantial vertical space above the workshop floor. `dungeon.zil:55-56`, `work/PROSE.md:8`

## State matrix

| State | Trigger | Visible changes | Object locations | Exit changes | Sources |
|---|---|---|---|---|---|
| Initial | Game start | Empty hook/string; ladder down or visibly stuck; clock closed; sawdust; oil can under bench | All canonical props at initial locations | North/east open; up/in blocked | `dungeon.zil:49-58`, `companion.zil:125-146`, `companion.zil:1041-1043` |
| Oil can taken | Take oil can | Copper can no longer beneath bench | `OIL-CAN` in player inventory | No exit change | `actions.zil:355-363`, `companion.zil:107-123` |
| Ladder oiled | Oil ladder/mechanism | Mechanism reads lubricated/free; folding ladder raised or usable | Oil can carried; ladder/mechanism remain | Up to loft opens | `actions.zil:257-277`, `actions.zil:406-425`, `companion.zil:1038-1040` |
| Key found | Recover workshop key elsewhere | Clock urgency becomes narratively important; room geometry otherwise unchanged | Key in player inventory | Enables clock-reveal action | `actions.zil:394-395`, `companion.zil:72-90` |
| Study accessible | Wind Old Tick and helper releases latch | Clock swings outward; narrow stair is revealed behind it | Clock moves on hinge | In to study opens | `actions.zil:214-233`, `actions.zil:396-398`, `companion.zil:1035-1037` |
| Endgame return | Workshop key winds heart | Warm returning-magic treatment | Canonical room relationships persist | Study route remains available | `companion.zil:1032-1034` |

## Conflicts and unknowns

- ZIL says the ladder “can rise now” and later “rises smoothly,” but does not fully specify the initial folded pose, hinge location or loft hatch position. Art direction must choose a mechanically plausible before/after arrangement.
- The map calls the key hook the landmark while prose gives the enormous bench greater physical dominance. Treat hook as narrative focal detail and bench as spatial hero.
- `CLOCK-FACE` behavior is implemented through `OLD-TICK` winding logic even though the downstairs object is separately named. Preserve the visible clock reveal without inventing a second downstairs clock.
- The current 3D scene merges workshop floor, tool bench and countertop geography into one room. This is compatible with their adjacency but must preserve distinct zones.
- Exact architectural style, room dimensions, wall finish, window design, furniture count and noninteractive clutter are not canonical.

## Source coverage

- `dungeon.zil:18-22`, `dungeon.zil:49-86`, `dungeon.zil:160-254`
- `actions.zil:200-233`, `actions.zil:255-279`, `actions.zil:347-425`
- `companion.zil:44-161`, `companion.zil:1027-1055`
- `work/OBJECTS.md:12-32`
- `work/PROSE.md:1-23`
- `work/MAP.md:52-68`
- `work/STORY_STATE.md:25-38`
- `work/PUZZLES.md:181-191`
- `work/TRANSCRIPT_TESTS.md:11-35`, `work/TRANSCRIPT_TESTS.md:74-83`

Coverage checklist: room form checked; direct objects checked; recursive containment checked; exits checked; action routines checked; companion scenes checked; prose checked; object registry checked; map checked; puzzle and state docs checked; transcript expectations checked.
