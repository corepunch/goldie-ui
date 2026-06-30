# Debug macros

These macros control verbose debug logging. Set to `1` at build time to enable,
`0` to disable. Default is `0` (off).

| Macro | File | Description |
|---|---|---|
| `GITCLIENT_DEBUG` | `examples/gitclient/gitclient.h` | gitclient app logging |
| `TABLEVIEW_DEBUG` | `commctl/tableview.c` | tableview control logging |
| `SOCIALFEED_DEBUG` | `examples/socialfeed/socialfeed.h` | socialfeed app logging |
| `IMAGEEDITOR_DEBUG` | `examples/imageeditor/imageeditor.h` | imageeditor app logging |
| `TASKMANAGER_DEBUG` | `examples/taskmanager/taskmanager.h` | taskmanager app logging |

Example: `make CFLAGS="-DGITCLIENT_DEBUG=1 -DTABLEVIEW_DEBUG=1"`
