/**
 * ATP Research — API Client
 * All server communication goes through this module.
 */
const API = {
  // ─── Prover ───
  async prove(format, formula) {
    const res = await fetch(`/api/prove/${format}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(
        format === 'prover9'
          ? { goals: formula }
          : { formula }
      ),
    });
    return res.json();
  },

  // ─── Auth ───
  async getUser() {
    const res = await fetch('/api/auth/me');
    return res.json();
  },

  async login(username, password) {
    const res = await fetch('/api/auth/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username, password }),
    });
    return res.json();
  },

  async register(username, password, display_name) {
    const res = await fetch('/api/auth/register', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username, password, display_name }),
    });
    return res.json();
  },

  async logout() {
    const res = await fetch('/api/auth/logout', { method: 'POST' });
    return res.json();
  },

  // ─── Blog ───
  async getBlogs(limit = 100) {
    const res = await fetch(`/api/blog?limit=${limit}`);
    return res.json();
  },

  async getBlog(slug) {
    const res = await fetch(`/api/blog/${slug}`);
    return res.json();
  },

  async createBlog(title, content, excerpt, tag) {
    const res = await fetch('/api/blog', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ title, content, excerpt, tag }),
    });
    return res.json();
  },

  async updateBlog(id, body) {
    const res = await fetch(`/api/blog/${id}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    return res.json();
  },

  async deleteBlog(id) {
    const res = await fetch(`/api/blog/${id}`, { method: 'DELETE' });
    return res.json();
  },
};
