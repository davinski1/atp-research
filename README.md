# ATP Research - Automated Theorem Proving Platform

An interactive automated theorem proving (ATP) web application that combines educational content with real machine-based proof capabilities. Features three theorem provers — propositional logic (C++), first-order logic (C++), and **Prover9** — along with AI-powered proof translation, a blog system, user authentication, and a mathematical textbook.

## Features

### Theorem Provers

| Mode | Engine | Language | Scope |
|------|--------|----------|-------|
| **Propositional Logic** | Resolution refutation | C++ | Tautology checking via CNF + resolution |
| **First-Order Logic** | Herbrand + unification | C++ | FOL with quantifiers, Skolemization |
| **Prover9** | William McCune's prover | System binary | Full FOL with equality, paramodulation |

- All computation runs **server-side** — the browser is a pure UI client
- Chat-style interactive prover UI on the **Member** page
- One-click example theorems (Modus Ponens, De Morgan, extensionality, etc.)
- Step-by-step proof output with formal notation

### AI Proof Translation

- Prover9 output is automatically translated into human-readable Japanese explanations
- Powered by **Claude API** (Anthropic)
- Raw proof and AI explanation displayed simultaneously
- Activated when `ANTHROPIC_API_KEY` environment variable is set

### Blog System

- Markdown-based blog posts stored in SQLite
- Homepage displays the **latest 4 posts** dynamically
- Full blog archive page with all published posts
- Members-only blog editor with create / edit / delete
- Tag-based categorization (Theory, Implementation, Tutorial, History)

### Authentication

- Session-based login / registration system
- Passwords hashed with **bcrypt**
- Signed cookie sessions via **itsdangerous**
- Role-based access control (admin / member)
- Default admin account: `admin` / `admin123`

### Textbook

- 5 chapters covering mathematical foundations for ATP
- Sidebar table of contents with scroll tracking
- Formal mathematical notation throughout

### Theme System

- **Dark** / **Light** / **System** toggle on every page
- Persisted in `localStorage`

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Backend | **Python** (FastAPI + uvicorn) |
| Provers | **C++17** (clang++, compiled binaries) |
| Database | **SQLite** (stdlib sqlite3, WAL mode) |
| Auth | **bcrypt** + **itsdangerous** (signed cookies) |
| AI Translation | **Anthropic Claude API** (claude-haiku-4-5) |
| Markdown | **python-markdown** |
| Frontend | Vanilla HTML/CSS/JS |
| Fonts | Inter, JetBrains Mono, Noto Sans JP |

---

## Getting Started

### Prerequisites

- **Python** 3.10+
- **C++ compiler** (clang++ or g++ with C++17 support)
- **Prover9** installed and available in `$PATH`
- (Optional) **Anthropic API key** for AI proof translation

#### Installing Prover9 (macOS)

```bash
brew install prover9
```

### Installation

```bash
cd atp-research

# Install Python dependencies
pip install -r server/requirements.txt

# Compile C++ prover binaries
make -C server/prover
```

### Running

```bash
# Start the server
uvicorn main:app --host 0.0.0.0 --port 3333 --reload --app-dir server
```

The server starts at **http://localhost:3333**.

#### With AI Translation

```bash
ANTHROPIC_API_KEY=sk-ant-... uvicorn main:app --host 0.0.0.0 --port 3333 --reload --app-dir server
```

---

## Project Structure

```
atp-research/
├── server/                    # Python (FastAPI) backend
│   ├── main.py               # FastAPI app, routers, static file serving
│   ├── auth.py               # Auth endpoints + signed cookie sessions
│   ├── blog.py               # Blog CRUD endpoints
│   ├── prover_api.py         # Prover endpoints (all 3 modes) + AI translation
│   ├── database.py           # SQLite setup, schema, seed data
│   ├── requirements.txt      # Python dependencies
│   ├── atp.db                # SQLite database (auto-created)
│   └── prover/               # C++ prover engine
│       ├── resolution.cpp    # Propositional: tokenizer → parser → CNF → resolution
│       ├── fol_prover.cpp    # FOL: Skolemization → Herbrand → unification → resolution
│       ├── Makefile          # Compile both binaries
│       ├── resolution        # Compiled binary (propositional)
│       └── fol_prover        # Compiled binary (FOL)
│
├── web/                       # Static frontend (pure UI client)
│   ├── index.html            # Landing page with hero + prover demo
│   ├── member.html           # Chat-style interactive prover
│   ├── textbook.html         # Mathematical textbook (5 chapters)
│   ├── blog.html             # Blog listing page
│   ├── blog-editor.html      # Blog editor (auth required)
│   ├── login.html            # Login / registration
│   ├── css/
│   │   ├── style.css         # Global styles + theme variables
│   │   ├── member.css        # Chat prover layout + AI explanation styles
│   │   ├── textbook.css      # Textbook layout + TOC
│   │   └── blog.css          # Blog pages
│   ├── js/
│   │   ├── api.js            # Centralized API client (all fetch calls)
│   │   ├── chat.js           # Chat UI — sends formulas to server, renders results
│   │   ├── app.js            # Homepage UI — prover demo via server API
│   │   └── theme.js          # Dark/light/system theme toggle
│   └── images/
│
├── README.md
└── .claude/
    └── launch.json            # Dev server configuration
```

---

## API Reference

### Provers

| Method | Endpoint | Body | Description |
|--------|----------|------|-------------|
| `POST` | `/api/prove/propositional` | `{ formula }` | Propositional logic proof (C++ binary) |
| `POST` | `/api/prove/fol` | `{ formula }` | First-order logic proof (C++ binary) |
| `POST` | `/api/prove/prover9` | `{ goals, formulas?, timeout? }` | Prover9 proof + AI translation |

### Authentication

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/auth/me` | Get current logged-in user |
| `POST` | `/api/auth/login` | Login with `{ username, password }` |
| `POST` | `/api/auth/register` | Register with `{ username, password, display_name }` |
| `POST` | `/api/auth/logout` | Logout (destroys session) |

### Blog

| Method | Endpoint | Auth | Description |
|--------|----------|------|-------------|
| `GET` | `/api/blog?limit=4` | No | List published posts |
| `GET` | `/api/blog/{slug}` | No | Get single post by slug |
| `POST` | `/api/blog` | Yes | Create new post |
| `PUT` | `/api/blog/{id}` | Yes | Update post |
| `DELETE` | `/api/blog/{id}` | Yes | Delete post |

---

## How the Provers Work

### Propositional Logic (C++ — `server/prover/resolution.cpp`)

1. **Tokenize** — Lex the input into tokens (operators, variables, parens)
2. **Parse** — Recursive-descent parser builds an AST with correct precedence
3. **Negate** — Assume the negation of the formula (refutation)
4. **CNF** — Convert to Conjunctive Normal Form (De Morgan, distribution)
5. **Resolve** — Apply the resolution rule on complementary literals (max 5000 iterations)
6. **Result** — If the empty clause □ is derived → tautology proved

### First-Order Logic (C++ — `server/prover/fol_prover.cpp`)

1. **Parse** — FOL formula with quantifiers (`all x`, `exists x`) and predicates
2. **NNF** — Negation Normal Form conversion
3. **Skolemize** — Replace `∃` with Skolem functions/constants
4. **CNF** — Convert to clause form
5. **Herbrand instantiation** — Ground the formula with terms from the Herbrand universe
6. **Unification** — Robinson's unification algorithm with occurs check
7. **Resolve** — Apply resolution on unifiable complementary literals

### Prover9 (System binary + AI translation)

1. User input is wrapped in `formulas(goals). ... end_of_list.`
2. Written to a temporary file
3. The `prover9` binary is executed with a configurable timeout (max 30s)
4. Output is parsed for `THEOREM PROVED` / `SEARCH FAILED`
5. Proof steps and statistics are extracted
6. (If API key set) Claude AI translates the proof into human-readable Japanese

---

## Database Schema

```sql
CREATE TABLE users (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  username      TEXT UNIQUE NOT NULL,
  password_hash TEXT NOT NULL,
  display_name  TEXT NOT NULL,
  role          TEXT DEFAULT 'member',
  created_at    DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE blog_posts (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  title       TEXT NOT NULL,
  slug        TEXT UNIQUE NOT NULL,
  content     TEXT NOT NULL,          -- Markdown
  excerpt     TEXT,
  tag         TEXT DEFAULT 'General',
  author_id   INTEGER REFERENCES users(id),
  published   INTEGER DEFAULT 1,
  created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
  updated_at  DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

---

## License

This project is for educational and research purposes.

## Acknowledgments

- **Prover9** by William McCune (Argonne National Laboratory)
- **ATP Research Association** — https://atp-research.com/
- Resolution method by J.A. Robinson (1965)
- **Anthropic Claude** for AI proof translation
