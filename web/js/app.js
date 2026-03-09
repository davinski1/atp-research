/**
 * ATP Research - UI Logic (index.html)
 * Proves via server API calls.
 */
document.addEventListener('DOMContentLoaded', () => {
  const formulaInput   = document.getElementById('formulaInput');
  const formulaPreview = document.getElementById('formulaPreview');
  const proveBtn       = document.getElementById('proveBtn');
  const clearBtn       = document.getElementById('clearBtn');
  const proofOutput    = document.getElementById('proofOutput');
  const proofStatus    = document.getElementById('proofStatus');
  const syntaxToggle   = document.getElementById('syntaxToggle');
  const syntaxHelp     = document.getElementById('syntaxHelp');
  const menuToggle     = document.getElementById('menuToggle');
  const nav            = document.getElementById('nav');

  // Mobile menu
  menuToggle.addEventListener('click', () => nav.classList.toggle('open'));
  document.querySelectorAll('.nav-link').forEach(link => {
    link.addEventListener('click', () => nav.classList.remove('open'));
  });

  // Active nav on scroll
  const sections = document.querySelectorAll('section[id]');
  const navLinks = document.querySelectorAll('.nav-link');
  function updateActiveNav() {
    const scrollY = window.scrollY + 100;
    sections.forEach(section => {
      const top = section.offsetTop;
      const height = section.offsetHeight;
      const id = section.getAttribute('id');
      if (scrollY >= top && scrollY < top + height) {
        navLinks.forEach(link => {
          link.classList.remove('active');
          if (link.getAttribute('href') === '#' + id) link.classList.add('active');
        });
      }
    });
  }
  window.addEventListener('scroll', updateActiveNav);

  // Syntax help toggle
  syntaxToggle.addEventListener('click', () => {
    syntaxHelp.classList.toggle('open');
    const icon = syntaxToggle.querySelector('.toggle-icon');
    icon.textContent = syntaxHelp.classList.contains('open') ? '\u25B2' : '\u25BC';
  });

  // Symbol buttons
  document.querySelectorAll('.sym-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const symbol = btn.dataset.symbol;
      const start = formulaInput.selectionStart;
      const end = formulaInput.selectionEnd;
      const text = formulaInput.value;
      formulaInput.value = text.substring(0, start) + symbol + text.substring(end);
      formulaInput.focus();
      formulaInput.selectionStart = formulaInput.selectionEnd = start + symbol.length;
      updatePreview();
    });
  });

  // Preview (just show input as-is, no client-side parsing)
  function updatePreview() {
    const val = formulaInput.value.trim();
    formulaPreview.textContent = val;
  }
  formulaInput.addEventListener('input', updatePreview);

  // Example buttons
  document.querySelectorAll('.example-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      formulaInput.value = btn.dataset.formula;
      updatePreview();
      formulaInput.focus();
    });
  });

  // Theorem cards
  document.querySelectorAll('.theorem-prove-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const card = btn.closest('.theorem-card');
      formulaInput.value = card.dataset.formula;
      updatePreview();
      document.getElementById('prover').scrollIntoView({ behavior: 'smooth' });
      setTimeout(() => runProof(), 500);
    });
  });

  // Clear
  clearBtn.addEventListener('click', () => {
    formulaInput.value = '';
    formulaPreview.textContent = '';
    proofStatus.textContent = '';
    proofStatus.className = 'proof-status';
    proofOutput.innerHTML = `
      <div class="proof-placeholder">
        <div class="placeholder-icon">&#x22A2;</div>
        <p>左パネルに論理式を入力し、「証明する」をクリックしてください。</p>
        <p class="placeholder-hint">例題ボタンからサンプルを選ぶこともできます。</p>
      </div>`;
  });

  // Prove
  proveBtn.addEventListener('click', runProof);
  formulaInput.addEventListener('keydown', (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
      e.preventDefault();
      runProof();
    }
  });

  async function runProof() {
    const formula = formulaInput.value.trim();
    if (!formula) return;

    proofOutput.innerHTML = '<div style="text-align:center;padding:40px;color:var(--text-muted)">証明中...</div>';
    proofStatus.textContent = '';
    proofStatus.className = 'proof-status';

    try {
      const result = await API.prove('propositional', formula);
      if (result.error) {
        proofOutput.innerHTML = `<div class="proof-error">エラー: ${escapeHtml(result.error)}</div>`;
        proofStatus.textContent = 'Error';
        proofStatus.className = 'proof-status invalid';
        return;
      }
      renderProof(result);
    } catch (err) {
      proofOutput.innerHTML = `<div class="proof-error">エラー: ${escapeHtml(err.message)}</div>`;
      proofStatus.textContent = 'Error';
      proofStatus.className = 'proof-status invalid';
    }
  }

  function renderProof(result) {
    proofOutput.innerHTML = '';
    let delay = 0;
    const steps = result.steps || [];

    steps.forEach(step => {
      const el = document.createElement('div');
      el.style.animationDelay = delay + 'ms';
      delay += 80;

      switch (step.type) {
        case 'parse':
          el.className = 'proof-step step-parse';
          el.innerHTML = `
            <div class="proof-step-header">${escapeHtml(step.title)}</div>
            <div class="proof-step-content">
              ${escapeHtml(step.content)}<br>
              解析結果: <span class="formula">${escapeHtml(step.formula)}</span>
            </div>`;
          break;

        case 'negate':
          el.className = 'proof-step step-negate';
          el.innerHTML = `
            <div class="proof-step-header">${escapeHtml(step.title)}</div>
            <div class="proof-step-content">
              ${escapeHtml(step.content)}<br>
              否定: <span class="formula">${escapeHtml(step.formula)}</span>
            </div>`;
          break;

        case 'cnf':
          el.className = 'proof-step step-cnf';
          el.innerHTML = `
            <div class="proof-step-header">${escapeHtml(step.title)}</div>
            <div class="proof-step-content">
              ${escapeHtml(step.content)}<br>
              CNF: <span class="formula">${escapeHtml(step.formula)}</span>
            </div>`;
          break;

        case 'clauses':
          el.className = 'proof-step step-clauses';
          const clauseHtml = (step.clauses || [])
            .map(c => `<span class="clause">C${c.index}: ${escapeHtml(c.text)}</span>`)
            .join('');
          el.innerHTML = `
            <div class="proof-step-header">${escapeHtml(step.title)}</div>
            <div class="proof-step-content">
              ${escapeHtml(step.content)}<br>
              <div style="margin-top:8px">${clauseHtml}</div>
            </div>`;
          break;

        case 'resolve':
          el.className = 'proof-step step-resolve';
          let resolveHtml = '';
          const rs = step.resolutionSteps || [];
          if (rs.length > 0) {
            const maxShow = 30;
            resolveHtml = rs.slice(0, maxShow).map(r =>
              `<div class="resolution-step">
                C${r.index}: Res(C${r.from[0]}, C${r.from[1]}) on <span class="formula">${escapeHtml(r.literal)}</span> = <span class="clause">${escapeHtml(r.result)}</span>
              </div>`
            ).join('');
            if (rs.length > maxShow) {
              resolveHtml += `<div class="resolution-step" style="color:var(--text-muted);font-style:italic">... 他 ${rs.length - maxShow} ステップ</div>`;
            }
          } else {
            resolveHtml = '<div style="color:var(--text-muted)">導出ステップなし</div>';
          }
          el.innerHTML = `
            <div class="proof-step-header">${escapeHtml(step.title)}</div>
            <div class="proof-step-content">
              ${escapeHtml(step.content)}
              <div style="margin-top:8px">${resolveHtml}</div>
            </div>`;
          break;

        case 'result':
          el.className = `proof-result ${step.valid ? 'valid' : 'invalid'}`;
          el.innerHTML = step.valid
            ? `&#x2713; ${escapeHtml(step.content)}`
            : `&#x2717; ${escapeHtml(step.content)}`;
          proofStatus.textContent = step.valid ? 'VALID (恒真)' : 'NOT VALID';
          proofStatus.className = `proof-status ${step.valid ? 'valid' : 'invalid'}`;
          break;
      }

      proofOutput.appendChild(el);
    });

    proofOutput.scrollTop = 0;
  }

  function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  }
});
