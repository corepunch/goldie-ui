# VibeOffice artefact artwork

Use **Microsoft Fluent Emoji 3D PNGs** for new VibeOffice artefacts. This is the
project's established playful, full-colour object style; do not mix it with
outline, monochrome, or flat icon sets.

## Source and license

- Upstream: https://github.com/microsoft/fluentui-emoji
- Artwork variant: the `3D` PNG inside each named asset directory
- License: MIT; retain [`LICENSE.txt`](LICENSE.txt) with redistributed artwork
- Raw URL pattern:
  `https://raw.githubusercontent.com/microsoft/fluentui-emoji/main/assets/<Asset Name>/3D/<asset_name>_3d.png`

## Preparing additions

1. Choose the closest object from the upstream `assets` directory.
2. Download its transparent `3D/*_3d.png`, not an SVG or flat variant.
3. Resize proportionally into a transparent **64x64 RGBA PNG** using `contain`;
   never crop, add a background tile, tint, or draw an outline around it.
4. Save it here with a short lowercase semantic name such as `ticket.png`.
5. Add the enum and filename mapping to `examples/vibeoffice/main.c`, then add
   the upstream asset name to the mapping in `LICENSE.txt`.

The Icon control displays artefacts at up to 32x32 and may shrink them further
to fit an agent's artefact strip. Keeping the source files at 64x64 provides a
2x master for the maximum rendered size while keeping the set lightweight.

## Current mapping

| VibeOffice name | Fluent Emoji asset |
|---|---|
| Web request | Globe with meridians |
| Chat | Speech balloon |
| Calendar | Calendar |
| Plan | Clipboard |
| File | Page facing up |
| Project | Card index dividers |
| Ticket | Ticket |
| Bug | Bug |
| Report | Chart increasing |
| Folder | File folder |
| Email | Envelope |
| Database | File cabinet |

### Agent status mapping

Status icons are locally generated anti-aliased circles, not Fluent Emoji
artefacts. They render immediately before the agent name, keeping the
right-hand strip reserved for transferable work objects. Regenerate them with
`tools/gen_status_icons.py`.

| Agent state | Circle colour |
|---|---|
| Available | Green |
| Busy | Amber |
| Pending | Gray |
| Error | Red |
