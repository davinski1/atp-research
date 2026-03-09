"""
ATP Research — Blog CRUD API routes.
"""
from fastapi import APIRouter, Request
from pydantic import BaseModel
from typing import Optional
import time
import re
import markdown
from database import get_db
from auth import get_current_user

router = APIRouter(prefix="/api/blog")

class BlogCreate(BaseModel):
    title: str
    content: str
    excerpt: str = ""
    tag: str = "General"

class BlogUpdate(BaseModel):
    title: Optional[str] = None
    content: Optional[str] = None
    excerpt: Optional[str] = None
    tag: Optional[str] = None

@router.get("")
def list_posts(limit: int = 100, offset: int = 0):
    db = get_db()
    posts = db.execute("""
        SELECT bp.*, u.display_name as author_name
        FROM blog_posts bp LEFT JOIN users u ON bp.author_id = u.id
        WHERE bp.published = 1
        ORDER BY bp.created_at DESC
        LIMIT ? OFFSET ?
    """, (limit, offset)).fetchall()
    total = db.execute("SELECT COUNT(*) FROM blog_posts WHERE published=1").fetchone()[0]
    db.close()
    result = []
    for p in posts:
        d = dict(p)
        d["content_html"] = markdown.markdown(d["content"], extensions=["fenced_code"])
        result.append(d)
    return {"posts": result, "total": total}

@router.get("/{slug}")
def get_post(slug: str):
    db = get_db()
    post = db.execute("""
        SELECT bp.*, u.display_name as author_name
        FROM blog_posts bp LEFT JOIN users u ON bp.author_id = u.id
        WHERE bp.slug = ?
    """, (slug,)).fetchone()
    db.close()
    if not post:
        return {"error": "記事が見つかりません"}
    d = dict(post)
    d["content_html"] = markdown.markdown(d["content"], extensions=["fenced_code"])
    return {"post": d}

@router.post("")
def create_post(body: BlogCreate, request: Request):
    user = get_current_user(request)
    if not user:
        return {"error": "ログインが必要です"}
    if not body.title or not body.content:
        return {"error": "タイトルと本文は必須です"}
    slug_base = re.sub(r"[^a-z0-9\u3040-\u9fff]+", "-", body.title.lower()).strip("-") or "post"
    slug = slug_base + "-" + hex(int(time.time()))[-4:]
    excerpt = body.excerpt or body.content[:150]
    db = get_db()
    cur = db.execute(
        "INSERT INTO blog_posts (title, slug, content, excerpt, tag, author_id) VALUES (?,?,?,?,?,?)",
        (body.title, slug, body.content, excerpt, body.tag, user["id"]),
    )
    db.commit()
    post = db.execute("SELECT * FROM blog_posts WHERE id=?", (cur.lastrowid,)).fetchone()
    db.close()
    return {"post": dict(post)}

@router.put("/{post_id}")
def update_post(post_id: int, body: BlogUpdate, request: Request):
    user = get_current_user(request)
    if not user:
        return {"error": "ログインが必要です"}
    db = get_db()
    post = db.execute("SELECT * FROM blog_posts WHERE id=?", (post_id,)).fetchone()
    if not post:
        db.close()
        return {"error": "記事が見つかりません"}
    db.execute("""
        UPDATE blog_posts SET title=?, content=?, excerpt=?, tag=?, updated_at=CURRENT_TIMESTAMP WHERE id=?
    """, (body.title or post["title"], body.content or post["content"],
          body.excerpt or post["excerpt"], body.tag or post["tag"], post_id))
    db.commit()
    updated = db.execute("SELECT * FROM blog_posts WHERE id=?", (post_id,)).fetchone()
    db.close()
    d = dict(updated)
    d["content_html"] = markdown.markdown(d["content"], extensions=["fenced_code"])
    return {"post": d}

@router.delete("/{post_id}")
def delete_post(post_id: int, request: Request):
    user = get_current_user(request)
    if not user:
        return {"error": "ログインが必要です"}
    db = get_db()
    db.execute("DELETE FROM blog_posts WHERE id=?", (post_id,))
    db.commit()
    db.close()
    return {"ok": True}
