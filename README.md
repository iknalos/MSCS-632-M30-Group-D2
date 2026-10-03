# ConcurrentChat — Deliverable 2: Core Functionality

**MSCS-632-M30 Advanced Programming Language · Group Project (Option 2: Simple Chat Application)**
**Languages:** Rust and Go · **Instructor:** Jay Thom · **University of the Cumberlands**

| Team member | Role |
|---|---|
| Rahul Solanki | Rust implementation |
| Krinal Soni | Go implementation |
| Bijay Raj KC | Specification and documentation |

This repository holds the **Saturday** deliverable: the core functionality
working end to end in both languages. Filtering, search, direct messages, the
REPL and the benchmark arrive in the Day 3 repository.

## What works at this stage

- Multiple simulated users sending messages concurrently (requirement R1)
- Message history with `id`, timestamp, user ID and username (requirement R2)
- A hub that solely owns the history, so neither implementation needs a lock
- Clean shutdown reporting how many messages were recorded

Both implementations record **15 messages** for the scripted session: three
users × (1 join + 3 messages + 1 leave). The interleaving differs between runs,
which is the concurrency being visible.

## Layout

```
rust/
  Cargo.toml
  src/main.rs          model, hub and driver (single file at this stage)
go/
  go.mod
  main.go              model, hub and driver (single file at this stage)
```

## Build and run

Both were built and run in WSL2 Ubuntu 24.04 (rustc/cargo 1.95.0, go 1.27.1).

### Rust

```bash
cd rust
cargo build --release
./target/release/concurrent_chat
```

### Go

```bash
cd go
go build -o chat .
./chat
```

## Language-specific features demonstrated

| Language | Feature | Where |
|---|---|---|
| Rust | Data-carrying `enum MessageKind`, exhaustive `match` | `render()` in `src/main.rs` |
| Rust | Async tasks on Tokio; `mpsc` for many-to-one, `oneshot` for a single reply | `hub()`, `user_session()` |
| Rust | History owned by one task, so no `Mutex` anywhere | `hub()` |
| Go | Goroutines plus channels (CSP) | `userSession()`, `hub()` |
| Go | `select` multiplexing posts, queries and shutdown | `hub()` |
| Go | `sync.WaitGroup` for lifecycle | `main()` |
| Go | Tag-plus-fields stand-in for a sum type, with no exhaustiveness check | `Message`, `Render()` |

## Known limitations at this stage

- No filtering, search, direct messages or statistics yet (Day 3).
- No unit tests yet (Day 3).
- The Go hub uses separate channels per command type. Day 3 replaces this with
  a single ordered command channel after testing showed `select` can serve a
  query before queued posts are appended.
