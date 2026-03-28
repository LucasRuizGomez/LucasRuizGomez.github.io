/* ════════════════════════════════════════════════════════
   SongSender — main.js
   
   Handles:
   1. Nav tab + sidebar active state on scroll
   2. Stripe Checkout session creation via your backend API
   3. Demo "Send to Phone" button pulse animation
════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────
   CONFIG
   Replace these values with your own before going live.
────────────────────────────────────────────────────── */
const CONFIG = {
  // Your backend endpoint that creates a Stripe Checkout Session.
  // This must be YOUR server — never call Stripe's API directly from the browser
  // with your secret key.
  checkoutEndpoint: '/api/create-checkout-session',

  // Your Stripe publishable key (this one IS safe to expose in the browser)
  // Get it from: https://dashboard.stripe.com/apikeys
  stripePublishableKey: 'pk_live_REPLACE_WITH_YOUR_PUBLISHABLE_KEY',
};

/* ──────────────────────────────────────────────────────
   STRIPE CHECKOUT
   Flow:
   1. User clicks "Get started" button
   2. We call YOUR backend (checkoutEndpoint) to create a Checkout Session
   3. Your backend uses the Stripe secret key to create the session
   4. Backend returns { sessionId } to us
   5. We redirect to Stripe's hosted Checkout page
   6. After payment, Stripe redirects to /success.html (or /cancel.html)
   7. Stripe sends a webhook to your backend → you store the subscription
   8. On /success.html you show a download link or license key
────────────────────────────────────────────────────── */

const stripe = Stripe(CONFIG.stripePublishableKey);

async function startCheckout() {
  const btn = document.getElementById('checkout-btn');
  const msg = document.getElementById('checkout-msg');

  // Disable button to prevent double-clicks
  btn.disabled = true;
  btn.textContent = '⏳ Connecting to Stripe...';
  if (msg) msg.textContent = '';

  try {
    const response = await fetch(CONFIG.checkoutEndpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        // You can pass metadata here — e.g. a user ID if you have one
        // priceId is set server-side, not here, so it can't be tampered with
      }),
    });

    if (!response.ok) {
      throw new Error('Server error: ' + response.status);
    }

    const { sessionId } = await response.json();

    // Hand off to Stripe's hosted Checkout page
    const { error } = await stripe.redirectToCheckout({ sessionId });

    if (error) {
      throw new Error(error.message);
    }

  } catch (err) {
    btn.disabled = false;
    btn.textContent = '✦ Get started';
    if (msg) msg.textContent = 'Something went wrong. Please try again.';
    console.error('Checkout error:', err);
  }
}

// Wire up all CTA buttons to the checkout flow
document.addEventListener('DOMContentLoaded', () => {

  const checkoutBtn  = document.getElementById('checkout-btn');
  const heroCta      = document.getElementById('hero-cta');
  const sidebarCta   = document.getElementById('sidebar-cta');

  if (checkoutBtn) {
    checkoutBtn.addEventListener('click', (e) => {
      e.preventDefault();
      startCheckout();
    });
  }

  // Hero and sidebar CTAs scroll to pricing section first on mobile,
  // or trigger checkout directly on desktop
  function handleCta(e) {
    e.preventDefault();
    const pricingSection = document.getElementById('pricing');
    if (pricingSection) {
      pricingSection.scrollIntoView({ behavior: 'smooth' });
    }
  }

  if (heroCta)   heroCta.addEventListener('click', handleCta);
  if (sidebarCta) sidebarCta.addEventListener('click', handleCta);

});

/* ──────────────────────────────────────────────────────
   NAVIGATION — active tab highlighting on scroll
────────────────────────────────────────────────────── */
document.addEventListener('DOMContentLoaded', () => {

  const sections = ['home', 'how', 'features', 'pricing', 'reviews'];
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
