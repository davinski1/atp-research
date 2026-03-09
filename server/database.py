"""
ATP Research — SQLite database setup, schema, and seed data.
"""
import sqlite3
import os
import bcrypt

DB_PATH = os.path.join(os.path.dirname(__file__), "atp.db")

def get_db() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA foreign_keys=ON")
    return conn

def init_db():
    conn = get_db()
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            display_name TEXT NOT NULL,
            role TEXT DEFAULT 'member',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS blog_posts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            slug TEXT UNIQUE NOT NULL,
            content TEXT NOT NULL,
            excerpt TEXT,
            tag TEXT DEFAULT 'General',
            author_id INTEGER REFERENCES users(id),
            published INTEGER DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    """)
    conn.commit()
    _seed_data(conn)
    conn.close()

def _seed_data(conn: sqlite3.Connection):
    user_count = conn.execute("SELECT COUNT(*) FROM users").fetchone()[0]
    if user_count == 0:
        pw = bcrypt.hashpw(b"admin123", bcrypt.gensalt()).decode()
        conn.execute(
            "INSERT INTO users (username, password_hash, display_name, role) VALUES (?, ?, ?, ?)",
            ("admin", pw, "Administrator", "admin"),
        )
        conn.commit()
        print("Default admin created: admin / admin123")

    post_count = conn.execute("SELECT COUNT(*) FROM blog_posts").fetchone()[0]
    if post_count == 0:
        admin = conn.execute("SELECT id FROM users WHERE username='admin'").fetchone()
        aid = admin["id"]
        posts = [
            ("Resolution法の完全性証明", "resolution-completeness", "Theory",
             "命題論理におけるResolution法がなぜ完全であるかを解説します。任意の充足不能な節集合から必ず空節が導出可能であることの証明。",
             "## Resolution法の完全性\n\n命題論理におけるResolution法がなぜ完全であるかを解説します。\n\n### 定理\n任意の充足不能な節集合 S に対して、Resolution法により有限回の導出で空節 □ が得られる。\n\n### 証明の概略\n1. S が充足不能であることを仮定する\n2. 変数の数に関する帰納法を用いる\n3. 任意の変数 P について、S を P = T で簡約した集合と P = F で簡約した集合を考える\n4. 帰納法の仮定により、両方の簡約から空節が導出できる\n5. これらの導出を「持ち上げる」ことで、元の S からも空節が導出できることを示す\n\n### 意義\nResolution法の完全性は、命題論理の恒真性判定が機械的に可能であることを保証する重要な結果です。"),
            ("CNF変換アルゴリズムの実装", "cnf-conversion", "Implementation",
             "任意の命題論理式を連言標準形に変換するアルゴリズムの段階的な実装方法と最適化テクニック。",
             "## CNF変換アルゴリズム\n\n任意の命題論理式を連言標準形に変換するアルゴリズムを段階的に解説します。\n\n### ステップ1: 含意の除去\n```\nA → B  ≡  ¬A ∨ B\nA ↔ B  ≡  (A → B) ∧ (B → A)\n```\n\n### ステップ2: 否定の内側移動（ド・モルガン）\n```\n¬(A ∧ B)  ≡  ¬A ∨ ¬B\n¬(A ∨ B)  ≡  ¬A ∧ ¬B\n¬¬A       ≡  A\n```\n\n### ステップ3: 分配法則\n```\nA ∨ (B ∧ C)  ≡  (A ∨ B) ∧ (A ∨ C)\n```"),
            ("自動定理証明の歴史", "atp-history", "History",
             "1950年代のLogic Theoristから現代のSATソルバーまで、自動定理証明の発展の歴史を概観します。",
             "## 自動定理証明の歩み\n\n### 1950年代: 黎明期\n- **1956**: Logic Theorist（Newell, Shaw, Simon）\n- **1958**: Geometry Theorem Prover（Gelernter）\n\n### 1960年代: 基礎理論の確立\n- **1965**: J.A. Robinson が Resolution法を発表\n\n### 1970–90年代: 実用化\n- **1972**: Prolog言語の誕生\n- **1996**: EQP が Robbins予想を解決\n\n### 2000年代以降\n- **Prover9/Mace4**: William McCune\n- **Z3**: Microsoft Research\n- **Lean/Coq/Isabelle**: 対話型証明支援系"),
            ("Prover9入門: 一階論理の自動証明", "prover9-intro", "Tutorial",
             "Prover9の基本的な構文と使い方を解説。一階論理の定理を自動的に証明する方法を学びます。",
             "## Prover9とは\n\nProver9はWilliam McCuneによって開発された一階論理の自動定理証明器です。\n\n### 基本的な構文\n```\nall x P(x)       — 全称量化\nexists x P(x)    — 存在量化\n-P(x)            — 否定\nP(x) & Q(x)     — 論理積\nP(x) | Q(x)     — 論理和\nP(x) -> Q(x)    — 含意\n```\n\n### 使用例\n```\nformulas(goals).\n(all x (in(x,X) <-> in(x,Y))) -> X = Y.\nend_of_list.\n```"),
            ("スコーレム化と Herbrand の定理", "skolemization-herbrand", "Theory",
             "存在量化子をスコーレム関数に置換する手続きと、Herbrandの定理の解説。",
             "## スコーレム化\n\n一階論理式から存在量化子を除去する手続きです。\n\n### 手順\n1. NNFに変換\n2. 各 ∃x について:\n   - 外側に ∀ がなければ: x をスコーレム定数 c に置換\n   - 外側に ∀y₁,...,yₙ があれば: x をスコーレム関数 f(y₁,...,yₙ) に置換\n\n## Herbrandの定理\n\n一階論理式が充足不能であることと、そのHerbrand展開の有限部分集合が命題論理として充足不能であることは同値。"),
        ]
        for title, slug, tag, excerpt, content in posts:
            conn.execute(
                "INSERT INTO blog_posts (title, slug, content, excerpt, tag, author_id) VALUES (?,?,?,?,?,?)",
                (title, slug, content, excerpt, tag, aid),
            )
        conn.commit()
        print(f"Seeded {len(posts)} blog posts")
