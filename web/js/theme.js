/**
 * Theme Toggle: dark / light / system
 * Cycles: dark -> light -> system -> dark ...
 */
(function() {
  const STORAGE_KEY = 'atp-theme';

  function getSystemTheme() {
    return window.matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark';
  }

  function getStoredTheme() {
    return localStorage.getItem(STORAGE_KEY); // 'dark', 'light', or null (system)
  }

  function applyTheme(theme) {
    const effective = theme || getSystemTheme();
    document.documentElement.setAttribute('data-theme', effective);
    // Update toggle icon visibility
    document.querySelectorAll('.theme-icon-dark').forEach(el => el.style.display = effective === 'dark' ? 'block' : 'none');
    document.querySelectorAll('.theme-icon-light').forEach(el => el.style.display = effective === 'light' ? 'block' : 'none');
  }

  function init() {
    const stored = getStoredTheme();
    applyTheme(stored);

    // Listen for system changes
    window.matchMedia('(prefers-color-scheme: light)').addEventListener('change', () => {
      if (!getStoredTheme()) applyTheme(null);
    });

    // Toggle button
    document.addEventListener('click', (e) => {
      const btn = e.target.closest('.theme-toggle');
      if (!btn) return;
      const current = getStoredTheme();
      let next;
      if (current === 'dark') next = 'light';
      else if (current === 'light') next = null; // system
      else next = 'dark'; // system -> dark (since default is dark, go to explicit dark then light)

      // Actually: null (system/dark default) -> light -> dark -> null
      if (current === null) next = 'light';
      else if (current === 'light') next = 'dark';
      else next = null;

      if (next) localStorage.setItem(STORAGE_KEY, next);
      else localStorage.removeItem(STORAGE_KEY);
      applyTheme(next);
    });
  }

  // Apply immediately (before DOMContentLoaded) to prevent flash
  const stored = getStoredTheme();
  const effective = stored || getSystemTheme();
  document.documentElement.setAttribute('data-theme', effective);

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
