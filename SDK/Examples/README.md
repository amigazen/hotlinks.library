# hotlinks.library SDK examples

Small CLI programs showing how to call the 1992 HotLinks API against this
open-source `hotlinks.library`.  Build on Amiga with SAS/C after installing
the library and headers:

```text
cd Source/hotlinks && smake install && smake headers
cd SDK/Examples && smake
```

Assign `HotLinks:` to your edition storage directory (or rely on the
`SYS:HotLinks` fallback created by the library).

## Programs

| Tool | API illustrated |
| ---- | ---------------- |
| **edlist** | `FirstPub` / `NextPub`, `OpenPub` + `ReadPub` for FORM size |
| **publish_text** | `PutPub`, `OpenPub(OPEN_WRITE)`, `WritePub`, `SeekPub`, `ClosePub`, `Publish` |
| **read_edition** | edition lookup, `OpenPub(OPEN_READ)`, streaming `ReadPub` |
| **subscribe_demo** | `HLRegister` + `MsgPort`, `Subscribe`, `Notify`, `PubStatus`, `GetInfo` |

## Typical session flow

Every client follows the same registration pattern (see `hl_common.c`):

1. `OpenLibrary("hotlinks.library", 0)`
2. `HLRegister(creator_id, notify_port, screen)` — pass a `MsgPort` if you use `Notify`
3. `AllocPBlock(handle)` — always allocate `PubBlock` via the library
4. … edition operations …
5. `FreePBlock`, `UnRegister`, `CloseLibrary`

### Publish (subscriber producer)

```
PutPub(pb, NULL)          ; fill PRec.Name/Desc/Access, ID[] = 0 for new
OpenPub(pb, OPEN_WRITE)   ; assigns ID, bumps version, opens body file
WritePub / SeekPub        ; write IFF FORM DTXT bytes
ClosePub(pb)              ; flush metadata if modified
Publish(pb)               ; commit catalog entry, notify subscribers
```

### Subscribe (consumer)

```
Subscribe(pb)             ; mark local interest in this edition
Notify(pb, INFORM, ...)   ; request HLMsg on change (needs HLRegister port)
... later ...
PubStatus(pb)             ; returns CHANGED if edition updated on disk
GetInfo(pb)               ; refresh PRec after a change
Notify(pb, NOINFORM, ...) ; cancel
UnSubscribe(pb)
```

## Try it

```text
echo "Hello, HotLinks" > RAM:sample.txt
publish_text MyFirstEdition RAM:sample.txt
edlist
read_edition MyFirstEdition RAM:out.txt
type RAM:out.txt
```

Run `subscribe_demo MyFirstEdition` in one shell, then publish an update from
another (`publish_text` again with the same name creates a new version).

## Shared code

- `hl_common.h` / `hl_common.c` — open, register, `PubBlock` allocation, name lookup

Autodocs for each function: `SDK/Doc/hotlinks.doc`.
