/**
 * ATP Research - Chat-style Prover UI
 * All proving is done server-side via API calls.
 */
document.addEventListener('DOMContentLoaded', () => {
  const chatMessages = document.getElementById('chatMessages');
  const chatInput    = document.getElementById('chatInput');
  const chatSendBtn  = document.getElementById('chatSendBtn');
  const menuToggle   = document.getElementById('menuToggle');
  const nav          = document.getElementById('nav');
  const formatTabs   = document.querySelectorAll('.format-tab');
  const propHelp     = document.getElementById('propHelp');
  const folHelp      = document.getElementById('folHelp');
  const prover9Help  = document.getElementById('prover9Help');

  let currentFormat = 'prover9';
  updateHelpPanels();

  // Mobile menu
  menuToggle.addEventListener('click', () => nav.classList.toggle('open'));

  // Format tabs
  formatTabs.forEach(tab => {
    tab.addEventListener('click', () => {
      formatTabs.forEach(t => t.classList.remove('active'));
      tab.classList.add('active');
      currentFormat = tab.dataset.format;
      updateHelpPanels();
    });
  });

  function updateHelpPanels() {
    if (propHelp) propHelp.style.display = currentFormat === 'propositional' ? '' : 'none';
    if (folHelp) folHelp.style.display = currentFormat === 'fol' ? '' : 'none';
    if (prover9Help) prover9Help.style.display = currentFormat === 'prover9' ? '' : 'none';
  }

  // Example buttons
  document.querySelectorAll('.chat-example').forEach(btn => {
    btn.addEventListener('click', () => {
      currentFormat = btn.dataset.format;
      formatTabs.forEach(t => {
        t.classList.toggle('active', t.dataset.format === currentFormat);
      });
      updateHelpPanels();
      chatInput.value = btn.dataset.formula;
      sendMessage();
    });
  });

  // Send
  chatSendBtn.addEventListener('click', sendMessage);
  chatInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      sendMessage();
    }
  });

  // Auto-resize
  chatInput.addEventListener('input', () => {
    chatInput.style.height = 'auto';
    chatInput.style.height = Math.min(chatInput.scrollHeight, 120) + 'px';
  });

  async function sendMessage() {
    const formula = chatInput.value.trim();
    if (!formula) return;

    addMessage('user', formula);
    chatInput.value = '';
    chatInput.style.height = 'auto';

    const typingEl = addTypingIndicator();

    try {
      const data = await API.prove(currentFormat, formula);
      typingEl.remove();

      if (data.error) {
        addMessage('system', `
          <p>エラーが発生しました:</p>
          <div class="msg-result error">${escapeHtml(data.error)}</div>`);
        return;
      }

      if (currentFormat === 'prover9') {
        addMessage('system', renderProver9Result(data, formula));
      } else {
        addMessage('system', renderProofResult(data, formula));
      }
    } catch (err) {
      typingEl.remove();
      addMessage('system', `
        <p>サーバーへの接続に失敗しました:</p>
        <div class="msg-result error">${escapeHtml(err.message)}</div>
        <p>サーバーが起動しているか確認してください。</p>`);
    }
  }

  function addMessage(role, content) {
    const msg = document.createElement('div');
    msg.className = `chat-msg ${role}-msg`;
    if (role === 'user') {
      msg.innerHTML = `
        <div class="msg-avatar user-avatar">U</div>
        <div class="msg-body">
          <div class="msg-name">You</div>
          <div class="msg-text"><p>${escapeHtml(content)}</p></div>
        </div>`;
    } else {
      msg.innerHTML = `
        <div class="msg-avatar system-avatar">&#x22A2;</div>
        <div class="msg-body">
          <div class="msg-name">ATP Prover</div>
          <div class="msg-text">${content}</div>
        </div>`;
    }
    chatMessages.appendChild(msg);
    scrollToBottom();
    return msg;
  }

  function addTypingIndicator() {
    const msg = document.createElement('div');
    msg.className = 'chat-msg system-msg';
    msg.innerHTML = `
      <div class="msg-avatar system-avatar">&#x22A2;</div>
      <div class="msg-body">
        <div class="msg-name">ATP Prover</div>
        <div class="typing-indicator">
          <div class="typing-dot"></div>
          <div class="typing-dot"></div>
          <div class="typing-dot"></div>
        </div>
      </div>`;
    chatMessages.appendChild(msg);
    scrollToBottom();
    return msg;
  }

  // ─── Prover9 result (with AI translation) ───
  function renderProver9Result(data, formula) {
    let html = '';
    html += `<p>Prover9 で式を証明します:</p>`;
    html += `<div class="msg-proof-step step-parse">
      <div class="step-title">入力 (goals)</div>
      <div class="step-content"><span class="formula">${escapeHtml(formula)}</span></div>
    </div>`;

    if (data.proved) {
      html += `<div class="msg-result valid">&#x2713; THEOREM PROVED — 定理が証明されました</div>`;

      if (data.proof) {
        const proofLines = data.proof.split('\n').filter(l => l.trim());
        let proofHtml = proofLines.map(l => escapeHtml(l)).join('<br>');
        html += `<div class="msg-proof-step step-resolve">
          <div class="step-title">証明 (Proof) — Prover9 出力</div>
          <div class="step-content" style="white-space:pre-wrap;font-size:0.78rem;">${proofHtml}</div>
        </div>`;
      }

      // AI translation of proof
      if (data.aiExplanation) {
        html += `<div class="msg-proof-step step-ai-explanation">
          <div class="step-title">AI による証明の解説</div>
          <div class="step-content ai-explanation">${markdownToHtml(data.aiExplanation)}</div>
        </div>`;
      }

      if (data.stats) {
        html += `<div style="font-size:0.75rem;color:var(--text-muted);margin-top:6px;">${escapeHtml(data.stats)}</div>`;
      }
    } else if (data.searchFailed) {
      html += `<div class="msg-result invalid">&#x2717; SEARCH FAILED — 証明が見つかりませんでした</div>`;
      html += `<p style="font-size:0.84rem;color:var(--text-secondary)">この式は恒真ではないか、Prover9の探索限界に達した可能性があります。</p>`;
    } else {
      html += `<div class="msg-result error">${escapeHtml(data.output || '結果を取得できませんでした')}</div>`;
    }

    return html;
  }

  // ─── Propositional / FOL result (C++ binary JSON) ───
  function renderProofResult(data, formula) {
    let html = '';
    const steps = data.steps || [];
    const parsedFormula = steps[0]?.formula || formula;

    html += `<p>式 <strong>${escapeHtml(parsedFormula)}</strong> の恒真性を検証します。</p>`;

    steps.forEach(step => {
      switch (step.type) {
        case 'parse':
          html += `<div class="msg-proof-step step-parse">
            <div class="step-title">${escapeHtml(step.title)}</div>
            <div class="step-content">解析結果: <span class="formula">${escapeHtml(step.formula)}</span></div>
          </div>`;
          break;

        case 'negate':
          html += `<div class="msg-proof-step step-negate">
            <div class="step-title">${escapeHtml(step.title)}</div>
            <div class="step-content">否定: <span class="formula">${escapeHtml(step.formula)}</span></div>
          </div>`;
          break;

        case 'cnf':
          html += `<div class="msg-proof-step step-cnf">
            <div class="step-title">${escapeHtml(step.title)}</div>
            <div class="step-content"><span class="formula">${escapeHtml(step.formula)}</span></div>
          </div>`;
          break;

        case 'clauses': {
          const clauseHtml = (step.clauses || [])
            .map(c => `<span class="clause-tag">C${c.index}: ${escapeHtml(c.text)}</span>`)
            .join(' ');
          html += `<div class="msg-proof-step step-clauses">
            <div class="step-title">${escapeHtml(step.title)}</div>
            <div class="step-content">${clauseHtml}</div>
          </div>`;
          break;
        }

        case 'resolve': {
          let resolveHtml = '';
          const rs = step.resolutionSteps || [];
          if (rs.length > 0) {
            const maxShow = 15;
            resolveHtml = rs.slice(0, maxShow).map(r =>
              `C${r.index}: Res(C${r.from[0]}, C${r.from[1]}) on <span class="formula">${escapeHtml(r.literal)}</span> = <span class="clause-tag">${escapeHtml(r.result)}</span>`
            ).join('<br>');
            if (rs.length > maxShow) {
              resolveHtml += `<br><em style="color:var(--text-muted)">... 他 ${rs.length - maxShow} ステップ</em>`;
            }
          } else {
            resolveHtml = '<em style="color:var(--text-muted)">導出ステップなし</em>';
          }
          html += `<div class="msg-proof-step step-resolve">
            <div class="step-title">${escapeHtml(step.title)}</div>
            <div class="step-content">${resolveHtml}</div>
          </div>`;
          break;
        }

        case 'result':
          html += `<div class="msg-result ${step.valid ? 'valid' : 'invalid'}">
            ${step.valid ? '&#x2713;' : '&#x2717;'} ${escapeHtml(step.content)}
          </div>`;
          break;
      }
    });

    return html;
  }

  // Simple markdown -> HTML for AI explanation
  function markdownToHtml(md) {
    return md
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/```([\s\S]*?)```/g, '<pre><code>$1</code></pre>')
      .replace(/`([^`]+)`/g, '<code>$1</code>')
      .replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>')
      .replace(/^\s*###\s+(.+)$/gm, '<h4>$1</h4>')
      .replace(/^\s*##\s+(.+)$/gm, '<h3>$1</h3>')
      .replace(/^\s*[-*]\s+(.+)$/gm, '<li>$1</li>')
      .replace(/(<li>.*<\/li>)/gs, '<ul>$1</ul>')
      .replace(/<\/ul>\s*<ul>/g, '')
      .replace(/\n{2,}/g, '</p><p>')
      .replace(/^/, '<p>').replace(/$/, '</p>')
      .replace(/<p>\s*<(h[34]|ul|pre)/g, '<$1')
      .replace(/<\/(h[34]|ul|pre)>\s*<\/p>/g, '</$1>');
  }

  function scrollToBottom() {
    requestAnimationFrame(() => {
      chatMessages.scrollTop = chatMessages.scrollHeight;
    });
  }

  function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  }
});
