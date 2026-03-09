"""
ATP Research — FastAPI application entry point.
"""
from fastapi import FastAPI, Request
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse, HTMLResponse
from fastapi.middleware.cors import CORSMiddleware
import os

from database import init_db
from auth import router as auth_router
from blog import router as blog_router
from prover_api import router as prover_router

app = FastAPI(title="ATP Research", version="2.0")

# CORS (allow frontend dev server if needed)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# API routers
app.include_router(auth_router)
app.include_router(blog_router)
app.include_router(prover_router)

# Static files directory
WEB_DIR = os.path.join(os.path.dirname(__file__), "..", "web")

# Mount static sub-directories
for subdir in ("css", "js", "images"):
    subpath = os.path.join(WEB_DIR, subdir)
    if os.path.isdir(subpath):
        app.mount(f"/{subdir}", StaticFiles(directory=subpath), name=subdir)

# HTML page routes — serve from web/
@app.get("/{path:path}")
async def serve_page(request: Request, path: str = ""):
    # API routes are handled by routers above
    if path.startswith("api/"):
        return HTMLResponse(status_code=404, content="Not Found")

    # Try exact file
    file_path = os.path.join(WEB_DIR, path)
    if os.path.isfile(file_path):
        return FileResponse(file_path)

    # Try with .html extension
    html_path = file_path + ".html"
    if os.path.isfile(html_path):
        return FileResponse(html_path)

    # Default: index.html
    index_path = os.path.join(WEB_DIR, "index.html")
    if os.path.isfile(index_path):
        return FileResponse(index_path)

    return HTMLResponse(status_code=404, content="Not Found")


@app.on_event("startup")
def startup():
    init_db()
    print("ATP Research server started")
    print(f"Web directory: {os.path.abspath(WEB_DIR)}")
    # Check C++ binaries
    prover_dir = os.path.join(os.path.dirname(__file__), "prover")
    for binary in ["resolution", "fol_prover"]:
        bpath = os.path.join(prover_dir, binary)
        if os.path.isfile(bpath):
            print(f"  [OK] {binary}")
        else:
            print(f"  [!!] {binary} not found — run: make -C server/prover")
    # Check Prover9
    import shutil
    if shutil.which("prover9"):
        print("  [OK] prover9")
    else:
        print("  [!!] prover9 not found in PATH")
    # Check Anthropic API key
    if os.environ.get("ANTHROPIC_API_KEY"):
        print("  [OK] ANTHROPIC_API_KEY set (AI translation enabled)")
    else:
        print("  [--] ANTHROPIC_API_KEY not set (AI translation disabled)")
