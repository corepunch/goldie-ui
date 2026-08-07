# Workshop implementation coverage

## Implemented initial-state canon

| Requirement | Implementation | Reading camera |
|---|---|---|
| Enormous climbable workbench | Drawer workbench with thick legs, apron, pulls, lower shelf and vise | `WorkshopEstablishing`, `ClimbWorkbenchAction` |
| Empty hook and string clue | Isolated wall hook/string with cabinet below | `EmptyHookReveal` |
| Copper oil can under bench | Small copper can in readable under-bench niche | `OilCanCloseup` |
| Sawdust and shavings | Perimeter patches, curls and bench-surface clusters | `WorkshopEstablishing`, `WorkshopWallEstablishing` |
| Main exit and Pip-sized flap | Full plank door with casing, braces, hardware and brass-framed flap | `WorkshopWallEstablishing` |
| Cuckoo clock | Closed wall clock with breathing space and near-midnight hands | `WorkshopWallEstablishing`, `EmptyHookReveal` |
| Folding loft ladder and mechanism | Ladder, rungs, winch and crank in initial stuck configuration | `LadderTraversal` |
| Pip's broom | Broom retained near the main bench route | `WorkshopEstablishing` |
| Tool-bench climb geography | Existing crate/chair/book route retained and surrounded by tool-wall context | `ToolBenchEstablishing` |

## Implemented art-direction passes

| Pass | Representation |
|---|---|
| Structural articulation | Full door, arched window trim, wall posts/braces, ceiling beams and cross rafters |
| Hero furniture | Drawer workbench, tool bench, cabinet, door, clock and ladder |
| Functional storage | Three populated wall shelves, tool rack, cabinet and bench lower shelf |
| Work and floor clusters | Tools, books, jars, boxes, toy parts, offcuts and sawdust patches |
| Hardware and accents | Drawer pulls, vise, door straps/ring, ladder winch, clock parts and window rosette |

## Deliberate spacing audit

- Window mullions reach and terminate inside the arch.
- Clock, window and cabinet read as separate wall landmarks with visible plaster buffers.
- Hook/string has a quiet halo and does not merge with cabinet hardware.
- Door swing/read area and ladder base remain distinct.
- Workbench props form clusters but leave the climb landing and vise clear.
- The open center floor preserves Pip's circulation.

## Deferred state assets

- Oil-can-removed variant and residual dust silhouette.
- Oiled/raised ladder and operational mechanism.
- Clock swung open with concealed stair.
- Pet-flap movement variant.
- Endgame lighting variant.

These are intentionally not conflated with the initial-state scene. Their mechanical and adaptation decisions remain in `workshop-design.md` under `Unresolved decisions`.
