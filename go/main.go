// ConcurrentChat - Go implementation (Deliverable 2: core functionality)
//
// Design notes for the cross-language comparison:
//
//   - Go has no data-carrying sum type, so a message is a single struct with a
//     Kind tag plus the superset of fields any kind might need. Nothing forces
//     a switch on Kind to be exhaustive; a missing case compiles and is simply
//     skipped at run time.
//   - Concurrency uses goroutines and channels, following the CSP model: the
//     hub goroutine owns the history and is the only one that touches it, so
//     no mutex is needed. That discipline is a convention here, not something
//     the compiler enforces.
//   - Replies come back on a per-request channel supplied by the caller.
package main

import (
	"fmt"
	"sync"
	"time"
)

// MessageKind tags what sort of event a Message represents.
type MessageKind int

const (
	KindText MessageKind = iota
	KindJoin
	KindLeave
)

func (k MessageKind) String() string {
	switch k {
	case KindText:
		return "text"
	case KindJoin:
		return "join"
	case KindLeave:
		return "leave"
	}
	return "unknown"
}

// Message is one entry in the chat history.
//
// Body is meaningful only when Kind is KindText. In Rust this is expressed by
// attaching the payload to the variant; in Go the field exists on every value
// and the programmer has to remember when it applies.
type Message struct {
	ID        int64
	Timestamp time.Time
	UserID    int
	Username  string
	Kind      MessageKind
	Body      string
}

// Render formats one history line.
func (m Message) Render() string {
	when := m.Timestamp.Format("15:04:05.000")
	switch m.Kind {
	case KindText:
		return fmt.Sprintf("[%s] #%-3d %-8s (uid %d): %s", when, m.ID, m.Username, m.UserID, m.Body)
	case KindJoin:
		return fmt.Sprintf("[%s] #%-3d %-8s (uid %d) joined the room", when, m.ID, m.Username, m.UserID)
	case KindLeave:
		return fmt.Sprintf("[%s] #%-3d %-8s (uid %d) left the room", when, m.ID, m.Username, m.UserID)
	}
	// Reachable if a new kind is added and this switch is not updated. The
	// compiler gives no warning, which is the trade-off against Rust's
	// exhaustive match.
	return fmt.Sprintf("[%s] #%-3d %-8s (unhandled kind %v)", when, m.ID, m.Username, m.Kind)
}

// postCmd asks the hub to append a message.
type postCmd struct {
	userID   int
	username string
	kind     MessageKind
	body     string
}

// historyCmd asks the hub for a snapshot. reply carries exactly one value, but
// that is a convention here rather than something the type system states.
type historyCmd struct {
	reply chan []Message
}

// hub owns the history slice. Only this goroutine reads or writes it, which is
// the Go idiom: share memory by communicating, not by locking.
func hub(posts <-chan postCmd, queries <-chan historyCmd, done <-chan struct{}, wg *sync.WaitGroup) {
	defer wg.Done()

	history := make([]Message, 0, 64)
	var nextID int64 = 1

	for {
		select {
		case p := <-posts:
			history = append(history, Message{
				ID:        nextID,
				Timestamp: time.Now(),
				UserID:    p.userID,
				Username:  p.username,
				Kind:      p.kind,
				Body:      p.body,
			})
			nextID++

		case q := <-queries:
			snapshot := make([]Message, len(history))
			copy(snapshot, history)
			q.reply <- snapshot

		case <-done:
			return
		}
	}
}

// userSession is one simulated participant, running as its own goroutine.
func userSession(posts chan<- postCmd, wg *sync.WaitGroup, userID int, username string, lines []string) {
	defer wg.Done()

	posts <- postCmd{userID: userID, username: username, kind: KindJoin}

	for _, line := range lines {
		// Yielding between sends interleaves the goroutines so the
		// concurrency is visible in the output.
		time.Sleep(time.Millisecond)
		posts <- postCmd{userID: userID, username: username, kind: KindText, body: line}
	}

	posts <- postCmd{userID: userID, username: username, kind: KindLeave}
}

func main() {
	fmt.Println("=== ConcurrentChat (Go) - core functionality ===")
	fmt.Println()

	posts := make(chan postCmd, 128)
	queries := make(chan historyCmd)
	done := make(chan struct{})

	var hubWG sync.WaitGroup
	hubWG.Add(1)
	go hub(posts, queries, done, &hubWG)

	participants := []struct {
		id    int
		name  string
		lines []string
	}{
		{1, "alice", []string{"morning all", "has anyone looked at the borrow checker notes?", "thanks"}},
		{2, "bob", []string{"hey alice", "yes, reading them now", "channels make more sense to me"}},
		{3, "carol", []string{"joining late", "what did I miss?", "ok catching up"}},
	}

	var userWG sync.WaitGroup
	for _, p := range participants {
		userWG.Add(1)
		go userSession(posts, &userWG, p.id, p.name, p.lines)
	}
	userWG.Wait()

	reply := make(chan []Message)
	queries <- historyCmd{reply: reply}
	history := <-reply

	fmt.Printf("--- chat history (%d entries) ---\n", len(history))
	for _, m := range history {
		fmt.Println(m.Render())
	}

	close(done)
	hubWG.Wait()

	fmt.Printf("\nhub shut down cleanly; %d messages recorded\n", len(history))
}
