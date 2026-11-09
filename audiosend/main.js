/* ════════════════════════════════════════════════════════
   SongSender — main.js
   
   Handles:
   1. Nav tab + sidebar active state on scroll
   2. Demo "Send to Phone" button pulse animation
════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────
   NAVIGATION — active tab highlighting on scroll
────────────────────────────────────────────────────── */
document.addEventListener('DOMContentLoaded', () => {

  const sections = ['home', 'how', 'features', 'pricing'];
  const navTabs     = document.querySelectorAll('.nav-tab');
  const sidebarItems = document.querySelectorAll('.sidebar-item[data-section]');

  function setActive(sectionId) {
    navTabs.forEach(tab => {
      tab.classList.toggle('active', tab.dataset.section === sectionId);
    });
    sidebarItems.forEach(item => {
      item.classList.toggle('active', item.dataset.section === sectionId);
    });
  }

  // IntersectionObserver watches each section
  const observer = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        setActive(entry.target.id);
      }
    });
  }, { threshold: 0.3 });

  sections.forEach(id => {
    const el = document.getElementById(id);
    if (el) observer.observe(el);
  });

});

/* ──────────────────────────────────────────────────────
   DEMO SEND BUTTON — visual feedback only (not real)
────────────────────────────────────────────────────── */
document.addEventListener('DOMContentLoaded', () => {
  const demoBtn = document.getElementById('demo-send-btn');
  if (!demoBtn) return;

  const lcd = demoBtn.closest('.mw-body')?.querySelector('.mw-lcd');

  const states = [
    '● READY · rough_v4.wav',
    '⟳ EXPORTING...',
    '↑ SENDING...',
    '✓ SENT · open on phone',
  ];
  let step = 0;
  let running = false;

  demoBtn.addEventListener('click', () => {
    if (running) return;
    running = true;
    step = 0;

    function tick() {
      if (!lcd) return;
      lcd.textContent = states[step];
      step++;
      if (step < states.length) {
        setTimeout(tick, 900);
      } else {
        setTimeout(() => {
          lcd.textContent = states[0];
          running = false;
        }, 1800);
      }
    }
    tick();
  });
});
