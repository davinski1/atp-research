"""
ATP Research — Authentication API routes.
"""
from fastapi import APIRouter, Request, Response
from pydantic import BaseModel
import bcrypt
from database import get_db

router = APIRouter(prefix="/api/auth")

SESSION_COOKIE = "atp_session"

class LoginReq(BaseModel):
    username: str
    password: str

class RegisterReq(BaseModel):
    username: str
    password: str
    display_name: str = ""

def _set_session(response: Response, user_id: int):
    from itsdangerous import URLSafeSerializer
    s = URLSafeSerializer("atp-research-secret-2024")
    token = s.dumps({"uid": user_id})
    response.set_cookie(SESSION_COOKIE, token, max_age=7*24*3600, httponly=True, samesite="lax")

def get_current_user(request: Request):
    token = request.cookies.get(SESSION_COOKIE)
    if not token:
        return None
    try:
        from itsdangerous import URLSafeSerializer
        s = URLSafeSerializer("atp-research-secret-2024")
        data = s.loads(token)
        db = get_db()
        user = db.execute("SELECT id, username, display_name, role FROM users WHERE id=?", (data["uid"],)).fetchone()
        db.close()
        return dict(user) if user else None
    except Exception:
        return None

@router.get("/me")
def me(request: Request):
    user = get_current_user(request)
    return {"user": user}

@router.post("/login")
def login(body: LoginReq, response: Response):
    if not body.username or not body.password:
        return {"error": "ユーザー名とパスワードを入力してください"}
    db = get_db()
    user = db.execute("SELECT * FROM users WHERE username=?", (body.username,)).fetchone()
    db.close()
    if not user or not bcrypt.checkpw(body.password.encode(), user["password_hash"].encode()):
        return {"error": "ユーザー名またはパスワードが正しくありません"}
    _set_session(response, user["id"])
    return {"user": {"id": user["id"], "username": user["username"], "display_name": user["display_name"], "role": user["role"]}}

@router.post("/register")
def register(body: RegisterReq, response: Response):
    if not body.username or not body.password:
        return {"error": "ユーザー名とパスワードは必須です"}
    if len(body.password) < 4:
        return {"error": "パスワードは4文字以上にしてください"}
    db = get_db()
    existing = db.execute("SELECT id FROM users WHERE username=?", (body.username,)).fetchone()
    if existing:
        db.close()
        return {"error": "このユーザー名は既に使われています"}
    pw = bcrypt.hashpw(body.password.encode(), bcrypt.gensalt()).decode()
    display = body.display_name or body.username
    cur = db.execute("INSERT INTO users (username, password_hash, display_name) VALUES (?,?,?)", (body.username, pw, display))
    db.commit()
    uid = cur.lastrowid
    user = db.execute("SELECT id, username, display_name, role FROM users WHERE id=?", (uid,)).fetchone()
    db.close()
    _set_session(response, uid)
    return {"user": dict(user)}

@router.post("/logout")
def logout(response: Response):
    response.delete_cookie(SESSION_COOKIE)
    return {"ok": True}
