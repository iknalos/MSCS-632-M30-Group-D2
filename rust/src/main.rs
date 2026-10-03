//! ConcurrentChat - Rust implementation (Deliverable 2: core functionality)
//!
//! Design notes for the cross-language comparison:
//!
//! * Message variants are modelled with a data-carrying `enum`. Each variant
//!   holds exactly the fields it needs, and `match` is checked for
//!   exhaustiveness by the compiler, so adding a variant breaks the build
//!   until every site handles it.
//! * Concurrency uses async tasks on the Tokio runtime rather than OS threads.
//!   The chat history is owned by a single hub task; no other task can reach
//!   it, so no lock is required. Ownership makes that guarantee static.
//! * Replies travel back on a `oneshot` channel, which encodes "exactly one
//!   response" in the type system.

use chrono::{DateTime, Local};
use tokio::sync::{mpsc, oneshot};

/// The kind of a chat event. Each variant carries its own payload.
#[derive(Debug, Clone)]
pub enum MessageKind {
    /// An ordinary message to the room.
    Text(String),
    /// A user entered the room.
    Join,
    /// A user left the room.
    Leave,
}

/// One entry in the chat history.
#[derive(Debug, Clone)]
pub struct Message {
    pub id: u64,
    pub timestamp: DateTime<Local>,
    pub user_id: u32,
    pub username: String,
    pub kind: MessageKind,
}

impl Message {
    /// Render one history line. The `match` is exhaustive: if a new variant is
    /// added to `MessageKind`, this function stops compiling.
    pub fn render(&self) -> String {
        let when = self.timestamp.format("%H:%M:%S%.3f");
        match &self.kind {
            MessageKind::Text(body) => {
                format!("[{}] #{:<3} {:<8} (uid {}): {}", when, self.id, self.username, self.user_id, body)
            }
            MessageKind::Join => {
                format!("[{}] #{:<3} {:<8} (uid {}) joined the room", when, self.id, self.username, self.user_id)
            }
            MessageKind::Leave => {
                format!("[{}] #{:<3} {:<8} (uid {}) left the room", when, self.id, self.username, self.user_id)
            }
        }
    }
}

/// Messages the hub accepts. Every interaction with the history goes through
/// one of these, which is what lets a single task own the data.
#[derive(Debug)]
pub enum Command {
    Post {
        user_id: u32,
        username: String,
        kind: MessageKind,
    },
    /// Ask for the full history. The hub replies once, on the oneshot channel.
    History {
        reply: oneshot::Sender<Vec<Message>>,
    },
    Shutdown,
}

/// The hub task. It is the sole owner of `history`, so the borrow checker
/// guarantees no other task can read or write it concurrently. That is why
/// there is no Mutex anywhere in this program.
async fn hub(mut rx: mpsc::Receiver<Command>) {
    let mut history: Vec<Message> = Vec::new();
    let mut next_id: u64 = 1;

    while let Some(cmd) = rx.recv().await {
        match cmd {
            Command::Post { user_id, username, kind } => {
                history.push(Message {
                    id: next_id,
                    timestamp: Local::now(),
                    user_id,
                    username,
                    kind,
                });
                next_id += 1;
            }
            Command::History { reply } => {
                // `send` consumes the sender, so a reply can happen only once.
                let _ = reply.send(history.clone());
            }
            Command::Shutdown => break,
        }
    }
}

/// One simulated chat participant, running as its own async task.
async fn user_session(tx: mpsc::Sender<Command>, user_id: u32, username: &str, lines: Vec<&str>) {
    let name = username.to_string();

    // `.await` on an error means the hub is gone; there is nothing to recover
    // to, so the session simply ends.
    if tx
        .send(Command::Post { user_id, username: name.clone(), kind: MessageKind::Join })
        .await
        .is_err()
    {
        return;
    }

    for line in lines {
        // Yielding between sends interleaves the tasks, which is what makes
        // the concurrency visible in the output.
        tokio::task::yield_now().await;
        if tx
            .send(Command::Post {
                user_id,
                username: name.clone(),
                kind: MessageKind::Text(line.to_string()),
            })
            .await
            .is_err()
        {
            return;
        }
    }

    let _ = tx
        .send(Command::Post { user_id, username: name, kind: MessageKind::Leave })
        .await;
}

#[tokio::main]
async fn main() {
    println!("=== ConcurrentChat (Rust) - core functionality ===\n");

    let (tx, rx) = mpsc::channel::<Command>(128);
    let hub_handle = tokio::spawn(hub(rx));

    let participants: Vec<(u32, &str, Vec<&str>)> = vec![
        (1, "alice", vec!["morning all", "has anyone looked at the borrow checker notes?", "thanks"]),
        (2, "bob", vec!["hey alice", "yes, reading them now", "channels make more sense to me"]),
        (3, "carol", vec!["joining late", "what did I miss?", "ok catching up"]),
    ];

    // Each session gets its own clone of the sender. Cloning a Sender is how
    // multiple producers address one consumer.
    let mut handles = Vec::new();
    for (id, name, lines) in participants {
        let tx = tx.clone();
        handles.push(tokio::spawn(async move {
            user_session(tx, id, name, lines).await;
        }));
    }

    for h in handles {
        let _ = h.await;
    }

    // Ask for the history, then shut the hub down.
    let (reply_tx, reply_rx) = oneshot::channel();
    let _ = tx.send(Command::History { reply: reply_tx }).await;
    let history = reply_rx.await.unwrap_or_default();

    println!("--- chat history ({} entries) ---", history.len());
    for m in &history {
        println!("{}", m.render());
    }

    let _ = tx.send(Command::Shutdown).await;
    let _ = hub_handle.await;

    println!("\nhub shut down cleanly; {} messages recorded", history.len());
}
