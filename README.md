# hotlinks.library

Desktop publishing in the 1990s meant assembling magazines, manuals, and presentations from pieces — 
body text, logos, charts, scanned photos — each owned by a different program. Copying static files between apps loses live updates; embedding everything in one monolithic format loses tool choice.

Personal computing platforms solved this in several similar ways: Macintosh **Publish & Subscribe**
(System 7), Windows **DDE** and later **OLE**, NeXT **typed pasteboard linking**. Amiga had the IFF standard and clipboard, but no OS-level live-link layer for DTP and other similar use cases.

Soft-Logik filled that gap with **HotLinks** for their PageStream ecosystem: a shared **edition catalog**
where publishers commit IFF documents and subscribers keep **links** that refresh when content changes.

This new version of **hotlinks.library** from amigazen project is a BSD-licensed, ABI-compatible reimplementation of Soft-Logik's 1992 HotLinks systems. It preserves the original function table, `PubBlock` / `PubRecord` layouts, and message protocol while replacing the proprietary resident broker with a self-contained library — one `OpenLibrary()` call, no background daemon, with cross-process state brokered by Exec NamedObjects instead of a resident process.

## [amigazen project](http://www.amigazen.com)

*A web, suddenly*

*Forty years meditation*

*Minds awaken, free*

**amigazen project** is using modern software development tools and methods to update and rerelease classic Amiga open source software. Projects include a new AWeb, a new Amiga Python 2, and the ToolKit project — a universal SDK for Amiga development. All *amigazen project* releases are guaranteed to build against the ToolKit standard so that anyone can download and begin contributing straightaway without having to tailor the toolchain for their own setup.

This implementation of hotlinks.library is an original work of the amigazen project, released under the BSD 2-Clause License (see [LICENSE.md](LICENSE.md)). The 1992 HotLinks library interface, on-disk format specifications, and header files were opened to the Amiga community by Soft-Logik to freely use and copy; this reimplementation was written from those public SDK materials only.

The amigazen project philosophy is based on openness:

*Open* to anyone and everyone — *Open* source and free for all — *Open* your mind and create!

PRs for all projects are gratefully received at [GitHub](https://github.com/amigazen/). While the focus now is on classic 68k software, it is intended that all amigazen project releases can be ported to other Amiga-like systems including AROS and MorphOS where feasible.

## History

**HotLinks** was created in the early 1990s by **Soft-Logik Publishing Corp.** (Deron Kazmaier et al.)
for the Amiga desktop-publishing stack around **PageStream** (layout), **PageLiner** (text), and **BME**
(Bitmap Editor). The April 1992 **HotLinks Developers Kit** documented 27 LVOs, `HLMsg` message types,
IFF edition formats (`FORM DTXT`, `FORM ILBM`), access control, and on-disk layout. Soft-Logik compared
HotLinks openly to Macintosh Publish & Subscribe, Windows DDE/OLE, and NeXT linking — and opened the
**interface and specification** to the Amiga community to adopt freely. However, SoftLogik did not open 
source their implementation, impeding wider adoption of the standard.

The original product was two-tier:

1. **`hotlinks.library`** on disk — a thin LVO trap that packaged messages and forwarded them to…
2. A **resident HotLinks broker** — owned the edition database, requesters, login, locks, and notifications.

If the resident was not running, `OpenLibrary("hotlinks.library", 0)` failed. Editions lived in a
private HotLinks directory; clients used the API rather than hard-coded paths.

## The publish/subscribe pattern

HotLinks implements the **publish/subscribe** pattern (PubSub) at the **edition** level — not
command interchange (that is ARexx), not embedded COM objects (that is OLE), but **named, versioned
data snapshots** that other programs link to and refresh on change.

Soft-Logik's FAQ (*What is HotLinks?*, 1992) defined the metaphor this way:

| Role | What you do | HotLinks API |
|------|-------------|--------------|
| **Publisher** | Make a block of data available to other programs | `PutPub`, `OpenPub(OPEN_WRITE)`, `WritePub`, `Publish` |
| **Publisher** | Push revised content to existing subscribers | `Publish` again (version bumps; subscribers notified) |
| **Publisher** | Retire an edition | `RemovePub` |
| **Subscriber** | Link to an edition and ask for change notification | `Subscribe`, `Notify(INFORM, …)` |
| **Subscriber** | Link without notification; check on reopen | `Subscribe`, `Notify(NOINFORM, …)`, later `PubStatus` |
| **Subscriber** | One-shot import, no ongoing link | `Subscribe`, read edition, `UnSubscribe` |
| **Subscriber** | Drop the link | `UnSubscribe` |

The key separation: the **subscriber document** stores link metadata (edition ID, placement, scale,
rotation, flow geometry). The **edition file** stores canonical content (IFF body under
`HotLinks:Editions/`). When BME touch-ups a scanned ILBM and calls `Publish`, PageStream receives
`HLMSGID_NOTIFY`, re-reads the bitmap, and **does not lose** where the frame sat on the page.

Three subscribe modes from the 1992 documentation map directly to `Notify` flags:

- **Live link + notify** — `Notify(pb, INFORM, …)`; `HLMsg` arrives on the port registered with `HLRegister`
- **Live link, poll on demand** — subscribe without inform; check `PubStatus` for `CHANGED` when reopening or on a timer
- **Import once** — read the edition body and `UnSubscribe`; no ongoing association

Unlike ARexx, HotLinks sends no arbitrary commands to peer programs — only **edition changed**
notification (`HLMSGID_NOTIFY`). Unlike DDE, payloads are whole IFF files accessed with DOS-like calls
`OpenPub` / `ReadPub` / `WritePub` / `SeekPub` / `ClosePub`, suited to multi-megabyte graphics and
formatted text rather than spreadsheet cell handles.

## About HotLinks

`hotlinks.library` is a **standalone publish/subscribe catalog** for classic Amiga desktop publishing.
Publishers write self-contained **edition files** into a shared catalog; subscribers keep placement and
formatting in their layout document while **content** refreshes when the edition version bumps.

This reimplementation by amigazen project collapses the 1992 two-tier design into one library:

| Original Soft-Logik | This project |
| ------------------- | ------------ |
| Disk library + proprietary resident | Single self-contained library |
| `OpenLibrary` fails without resident | `OpenLibrary` succeeds when installed |
| Private broker database in resident | `HotLinks:Index` on disk; in-memory catalog via `utility.library` NamedObject (`HotLinks.catalog`) — same broker idea as `datatypes.library` Token |
| Intuition edition requesters (`GetPub` / `PutPub` / `PubInfo`) | gadtools requesters + `HotLinks:Users` password file |

**What HotLinks is not:** hyperlinks or URIs inside a paragraph (use DOCS `FORM URIL` or plain URLs);
real-time multi-user editing — editions are local (network catalog support was described as a future
direction in 1992 documentation but never appeared in the shipped API).

**HotLinks** comprises:

- **`hotlinks.library`** — edition registry, publish/subscribe, cooperative locks, access control,
  gadtools requesters, async change notification via `MsgPort`
- **Examples** (`SDK/Examples/`, `Source/test/`)
  - **edlist** — Shell catalog browser
  - **publish_text** — CLI publisher for `FORM DTXT` editions
  - **read_edition** — export an edition body to a file
  - **subscribe_demo** — `HLRegister` + `Notify` change watcher
  - **hl_test** — console API smoke harness
- **SDK**
  - Public headers under `Source/include/` (`libraries/hotlinks.h`, pragmas, protos)
  - `SDK/Doc/hotlinks.doc` — 1992 autodocs (LVO reference)
  - `SDK/Examples/README.md` — build and session recipes

## Inspirations

HotLinks sits in a long line of cross-application live-data mechanisms. The design chose file-native IFF
editions and an explicit catalog API over component object models:

| Inspiration | What HotLinks took from it |
|-------------|----------------------|
| **Macintosh Publish & Subscribe** (System 7) | Explicit publish and subscribe with layout separate from content. HotLinks adds a visible catalog (`FirstPub` / `NextPub`), version numbers, owner/group access flags, and read/write locks — closer to a small document database than Mac's desk-scrap edition manager. |
| **Windows DDE** | Conversation-oriented `topic`/`item` updates for small, frequent data. HotLinks streams large IFF bodies instead. DDE advise loops resemble `Notify`, but HotLinks standardises only **edition changed** messages. |
| **OLE / OLE32** | COM servers, structured storage, in-place activation. HotLinks keeps **file-native IFF editions** readable without a server process, an **explicit catalog API**, and **Amiga message ports** for notification. |
| **NeXTSTEP linking** | Typed pasteboard / bundle notifications, mapped onto IFF containers and a shared on-disk catalog. |
| **Soft-Logik PageStream stack** | PageLiner publishes `FORM DTXT`; BME publishes `FORM ILBM`; PageStream subscribes and reflows. |

In practice HotLinks is **System 7 PubSub plus a DTP-aware database**, whereas OLE32 is **"the document
is a bag of COM objects."** HotLinks never tried to host a spreadsheet's UI inside a word processor;
it ensured **PageStream still knew where the ILBM sat on the page** when BME saved a new bitmap version.

## Why a publish/subscribe catalog?

Classic Amiga DTP workflows passed copies of text files and ILBMs between PageLiner, BME, and PageStream —
workable for one-off jobs, but every re-edit meant manual re-import and lost placement geometry. A catalog
layer that separates **content** (edition file) from **presentation** (layout document) was the missing glue.

| Goal | How HotLinks addresses it |
|------|---------------------------|
| **Live updates** | `Publish` bumps version; subscribers receive `HLMSGID_NOTIFY` or poll `PubStatus` for `CHANGED` |
| **Tool choice** | Any publisher that writes valid IFF can `PutPub`; any subscriber reads via `OpenPub` / `ReadPub` |
| **Large payloads** | Edition bodies are normal IFF files on disk — not clipboard handles or DDE strings |
| **Cooperative editing** | `LockPub(LOCK_WRITE)` excludes other writers; `LOCK_READ` allows concurrent readers |
| **Multi-user catalogs** | `SetUser`, `Access` flags (`ACC_OREAD`, `ACC_GWRITE`, …), owner/group fields on each edition |
| **1992 ABI fidelity** | 27 LVOs, unchanged offsets — existing PageStream-era clients and SDK examples compile and run unchanged |

## Architecture tiers

| Tier | Component | Role |
|------|-----------|------|
| 1 | `hotlinks.library` | LVO trap, catalog broker, locks, requesters, notification dispatch |
| 2 | NamedObject token | `"HotLinks.catalog"` — shared in-memory catalog, locks list, user context across all open clients |
| 3 | On-disk storage | `HotLinks:Index` metadata + `HotLinks:Editions/` IFF bodies |
| 4 | Client apps | PageStream, BME, PageLiner, CLI tools, ARexx scripts |

### Catalog lifecycle

The first `OpenLibrary("hotlinks.library", 0)` in the system creates (or joins) the shared catalog token.
Every open participant shares one in-memory catalog. When the last client closes, the catalog is saved to
`HotLinks:Index` and unloaded; the NamedObject registration persists for the next open.

```
Publisher app                    hotlinks.library                 Subscriber app
     |                                  |                                |
     | PutPub / OpenPub(WRITE)          |                                |
     | WritePub (FORM DTXT bytes)       |                                |
     | ClosePub / Publish               |                                |
     |---------------- HLMSGID_NOTIFY -->|-------- HLMsg to MsgPort ---->|
     |                                  |                                | PubStatus → CHANGED
     |                                  |                                | GetInfo / re-read edition
```

## API tiers

Public LVOs follow Amiga shared-library conventions: opaque `PubBlock` handles, `HLMsg` request/reply over
message ports, and DOS-like edition I/O. All 27 functions from the 1992 Developers Kit are implemented.

| Layer | LVOs | Use case |
|-------|------|----------|
| **Registration** | `HLRegister`, `UnRegister`, `AllocPBlock`, `FreePBlock` | Every client registers a creator ID and optional notify port |
| **Catalog browse** | `FirstPub`, `NextPub`, `GetInfo`, `RemovePub` | Edition pickers, shell listers, batch tools |
| **Publish** | `PutPub`, `OpenPub`, `WritePub`, `SeekPub`, `ClosePub`, `Publish` | Create or update edition bodies and commit catalog metadata |
| **Subscribe** | `Subscribe`, `UnSubscribe`, `Notify`, `PubStatus` | Track editions; async or polled refresh |
| **Edition I/O** | `OpenPub`, `ReadPub`, `WritePub`, `SeekPub`, `ClosePub` | Stream IFF bytes like a file handle |
| **Concurrency** | `LockPub` | Cooperative read/write exclusion |
| **Security** | `SetUser`, `ChgPassword`, `NewPassword`, `Access` via `PubRecord` | Multi-user catalogs; passwords in `HotLinks:Users` |
| **UI helpers** | `GetPub`, `PutPub`, `PubInfo` | gadtools edition requesters |

### Typical client session

Every client follows the same registration pattern:

```c
HotLinksBase = OpenLibrary("hotlinks.library", 0);
HLRegister(HotLinksBase, creator_id, notify_port, screen);
pb = AllocPBlock(HotLinksBase, handle);
/* … edition operations … */
FreePBlock(HotLinksBase, pb);
UnRegister(HotLinksBase);
CloseLibrary(HotLinksBase);
```

### Publish flow

```c
PutPub(pb, NULL);              /* fill PRec.Name, Desc, Access; ID[] = 0 for new */
OpenPub(pb, OPEN_WRITE);       /* assigns ID, bumps version, opens body file */
WritePub(pb, buf, len);        /* write IFF FORM DTXT bytes */
SeekPub(pb, offset, SEEK_CURRENT);
ClosePub(pb);                  /* flush metadata if modified */
Publish(pb);                   /* commit catalog entry, notify subscribers */
```

### Subscribe flow

```c
Subscribe(pb);                 /* mark local interest in this edition */
Notify(pb, INFORM, ...);       /* request HLMsg on change (needs HLRegister port) */
/* … later, on HLMSGID_NOTIFY or timer … */
if (PubStatus(pb) == CHANGED) {
    GetInfo(pb);               /* refresh PRec after a change */
    OpenPub(pb, OPEN_READ);
    ReadPub(pb, buf, len);
    ClosePub(pb);
}
Notify(pb, NOINFORM, ...);
UnSubscribe(pb);
```

## Use cases

- **Text + layout split** — PageLiner publishes the story; PageStream subscribes and reflows on change
- **Image + layout split** — BME publishes `ILBM`; DTP subscribes, scales and rotates; touch-up updates the placed frame without losing geometry
- **Multi-pass editorial loop** — write → check → layout → re-edit → re-flow via the same edition, not file copies
- **Modular projects** — chapters, logos, charts as separate editions assembled in a master document
- **Batch pipelines** — importers via `PutPub` + `WritePub`; exporters via `OpenPub(READ)` + `ReadPub`
- **Background watchers** — small task with a `MsgPort` and `Notify` wakes the main application
- **Presentations** — text, images, and sound as editions; reload before show-time if source material changed

## Edition storage

The catalog directory is resolved at init time:

1. logical assign `HotLinks:` (preferred)
2. fallback `SYS:HotLinks` (created if missing)

```
LIBS:hotlinks.library
HotLinks:Index
HotLinks:Editions/<id>.edition
HotLinks:Users
C:edlist
C:publish_text
```

Packagers add `ASSIGN HotLinks: SYS:HotLinks` (or wherever they prefer) to `User-Startup`. Edition bodies
are plain IFF files — any tool that understands `FORM DTXT` or `FORM ILBM` can read them directly from
`HotLinks:Editions/` even without calling the API.

## Contact

- At GitHub https://github.com/amigazen/hotlinks.library/
- On the web at http://www.amigazen.com/ (Amiga browser compatible)
- Or email toolkit@amigazen.com

## Acknowledgements

HotLinks was originally designed by **Soft-Logik Publishing Corp.** (Deron Kazmaier et al.) and documented
in the 1992 HotLinks Developers Kit. Soft-Logik opened the **interface and specification** to the Amiga
community while retaining copyright on their shipping binaries. See [LICENSE.md](LICENSE.md#acknowledgements)
for the full attribution statement.

*Amiga* is a trademark of **Amiga Inc**.
