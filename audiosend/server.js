/*
  SongSender — server.js
  Complete backend for Stripe subscription handling.

  Tech stack:
  - Node.js (v18+)
  - Express  (web server)
  - Stripe   (payments)
  - nodemailer (sending the download email after payment)

  Install dependencies:
    npm install express stripe nodemailer dotenv cors

  Run:
    node server.js

  Or in production with auto-restart:
    npm install -g pm2
    pm2 start server.js --name songsender
*/

require('dotenv').config();
const express   = require('express');
const stripe    = require('stripe')(process.env.STRIPE_SECRET_KEY);
const nodemailer = require('nodemailer');
const cors      = require('cors');
const path      = require('path');

const app = express();

/* ──────────────────────────────────────────────────────
   STATIC FILES
   Serve your HTML/CSS/JS from the same server.
   Put index.html, style.css, main.js, success.html,
   cancel.html in a folder called "public".
────────────────────────────────────────────────────── */
app.use(express.static(path.join(__dirname, 'public')));

/* ──────────────────────────────────────────────────────
   CORS
   In production, lock this down to your actual domain.
────────────────────────────────────────────────────── */
app.use(cors({
  origin: process.env.SITE_URL || 'http://localhost:3000',
}));

/* ──────────────────────────────────────────────────────
   IMPORTANT: Stripe webhooks need the RAW body,
   so we set up the raw body parser BEFORE json parser,
   but only on the webhook route.
────────────────────────────────────────────────────── */
app.use('/api/webhook', express.raw({ type: 'application/json' }));
app.use(express.json());

/* ──────────────────────────────────────────────────────
   EMAIL TRANSPORTER
   Configure with your email provider.
   For small volume: use Gmail with an App Password.
   For production: use Resend, Postmark, or SendGrid.
────────────────────────────────────────────────────── */
const mailer = nodemailer.createTransport({
  host:   process.env.SMTP_HOST,
  port:   parseInt(process.env.SMTP_PORT || '587'),
  secure: process.env.SMTP_SECURE === 'true',
  auth: {
    user: process.env.SMTP_USER,
    pass: process.env.SMTP_PASS,
  },
});

/* ──────────────────────────────────────────────────────
   ROUTE: POST /api/create-checkout-session

   Called by main.js when the user clicks "Get started".
   Creates a Stripe Checkout Session and returns the
   session ID to the browser. The browser then redirects
   to Stripe's hosted checkout page.

   HOW ACCOUNTS WORK:
   Stripe Checkout asks for an email address by default.
   We use that email as the customer's "account" —
   no separate sign-up needed. After payment, we email
   them a unique download link tied to their email.
   The plugin validates their license by checking their
   email + a license key against your server.
────────────────────────────────────────────────────── */
app.post('/api/create-checkout-session', async (req, res) => {
  try {
    const session = await stripe.checkout.sessions.create({
      mode: 'subscription',

      // Stripe asks the user for their email here — this IS their account
      // No separate sign-up needed. Stripe handles it.
      customer_creation: 'always',

      line_items: [
        {
          // This is your Price ID from the Stripe dashboard.
          // Create it at: dashboard.stripe.com → Products → Add Product
          // Set it as a recurring price ($4.99/month)
          price: process.env.STRIPE_PRICE_ID,
          quantity: 1,
        },
      ],

      // Where Stripe sends the user after successful payment
      success_url: `${process.env.SITE_URL}/success.html?session_id={CHECKOUT_SESSION_ID}`,

      // Where Stripe sends the user if they cancel
      cancel_url: `${process.env.SITE_URL}/cancel.html`,

      // Collect billing address (optional but useful for VAT)
      billing_address_collection: 'auto',

      // Let users apply promo codes if you create them in Stripe dashboard
      allow_promotion_codes: true,
    });

    res.json({ sessionId: session.id });

  } catch (err) {
    console.error('Stripe session error:', err.message);
    res.status(500).json({ error: 'Could not create checkout session.' });
  }
});

/* ──────────────────────────────────────────────────────
   ROUTE: POST /api/webhook

   Stripe calls this URL when things happen:
   - checkout.session.completed  → payment succeeded, send download
   - customer.subscription.deleted → subscription cancelled
   - invoice.payment_failed        → payment failed, warn user

   You MUST register this URL in the Stripe dashboard:
   dashboard.stripe.com → Developers → Webhooks → Add endpoint
   URL: https://yourdomain.com/api/webhook
   Events to listen for: checkout.session.completed,
     customer.subscription.deleted, invoice.payment_failed
────────────────────────────────────────────────────── */
app.post('/api/webhook', async (req, res) => {
  const sig = req.headers['stripe-signature'];
  let event;

  try {
    // This verifies the webhook actually came from Stripe
    event = stripe.webhooks.constructEvent(
      req.body,
      sig,
      process.env.STRIPE_WEBHOOK_SECRET
    );
  } catch (err) {
    console.error('Webhook signature verification failed:', err.message);
    return res.status(400).send(`Webhook Error: ${err.message}`);
  }

  // Handle the events
  switch (event.type) {

    case 'checkout.session.completed': {
      const session = event.data.object;

      // Get the customer's email and subscription ID
      const customerEmail  = session.customer_details?.email;
      const subscriptionId = session.subscription;
      const customerId     = session.customer;

      console.log(`✅ New subscriber: ${customerEmail} (sub: ${subscriptionId})`);

      // Generate a license key for this subscriber
      const licenseKey = generateLicenseKey(customerEmail, subscriptionId);

      // TODO: Save to your database here
      // await db.subscribers.create({ email: customerEmail, subscriptionId, licenseKey, customerId });

      // Send the welcome + download email
      await sendWelcomeEmail(customerEmail, licenseKey, subscriptionId);

      break;
    }

    case 'customer.subscription.deleted': {
      const subscription = event.data.object;
      const customerId   = subscription.customer;

      console.log(`❌ Subscription cancelled for customer: ${customerId}`);

      // TODO: Mark as inactive in your database
      // await db.subscribers.update({ customerId }, { active: false });

      // The plugin should check subscription status on each launch
      // using GET /api/verify-license, which you can make return false
      // once the subscription is cancelled.

      break;
    }

    case 'invoice.payment_failed': {
      const invoice      = event.data.object;
      const customerEmail = invoice.customer_email;

      console.log(`⚠️ Payment failed for: ${customerEmail}`);

      // Stripe automatically retries. You can also send a manual reminder.
      // await sendPaymentFailedEmail(customerEmail);

      break;
    }

    default:
      // Ignore other event types
      break;
  }

  // Tell Stripe we received the webhook successfully
  res.json({ received: true });
});

/* ──────────────────────────────────────────────────────
   ROUTE: GET /api/verify-license

   Your VST plugin calls this on startup to check if
   the user still has an active subscription.
   The plugin sends: { email, licenseKey }
   The server responds: { valid: true/false }

   This is how the plugin knows whether to unlock itself.
   You need a database for this (see Database section below).
────────────────────────────────────────────────────── */
app.get('/api/verify-license', async (req, res) => {
  const { email, licenseKey } = req.query;

  if (!email || !licenseKey) {
    return res.json({ valid: false, reason: 'Missing credentials' });
  }

  try {
    // TODO: Look up the subscriber in your database
    // const subscriber = await db.subscribers.findOne({ email, licenseKey });
    // if (!subscriber || !subscriber.active) {
    //   return res.json({ valid: false, reason: 'Subscription not active' });
    // }

    // Also verify with Stripe directly that subscription is still active
    // const subscription = await stripe.subscriptions.retrieve(subscriber.subscriptionId);
    // const active = subscription.status === 'active' || subscription.status === 'trialing';
    // return res.json({ valid: active });

    // PLACEHOLDER — replace with real DB check
    return res.json({ valid: true });

  } catch (err) {
    console.error('License verification error:', err);
    return res.json({ valid: false, reason: 'Server error' });
  }
});

/* ──────────────────────────────────────────────────────
   ROUTE: POST /api/cancel-subscription

   Lets subscribers cancel from the success page
   or a customer portal link.
   Better option: use Stripe's Customer Portal (see below).
────────────────────────────────────────────────────── */
app.post('/api/customer-portal', async (req, res) => {
  const { customerId } = req.body;

  try {
    const session = await stripe.billingPortal.sessions.create({
      customer: customerId,
      return_url: process.env.SITE_URL,
    });

    res.json({ url: session.url });

  } catch (err) {
    console.error('Portal error:', err);
    res.status(500).json({ error: 'Could not open customer portal.' });
  }
});

/* ──────────────────────────────────────────────────────
   HELPERS
────────────────────────────────────────────────────── */

function generateLicenseKey(email, subscriptionId) {
  // In production, use crypto.randomBytes or a UUID library
  // and store it in your database.
  // This is just a placeholder that creates a deterministic key —
  // replace with a proper random UUID in production.
  const crypto = require('crypto');
  return crypto
    .createHash('sha256')
    .update(email + subscriptionId + process.env.LICENSE_SALT)
    .digest('hex')
    .substring(0, 32)
    .toUpperCase()
    .replace(/(.{8})/g, '$1-')
    .slice(0, -1); // format: XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX
}

async function sendWelcomeEmail(toEmail, licenseKey, subscriptionId) {
  // The download page is PUBLIC — no token needed in the URL.
  // The license key is what gates access inside the plugin itself.
  const downloadPageUrl  = `${process.env.SITE_URL}/download`;
  const portalUrl        = `${process.env.SITE_URL}/portal`;
  const resendKeyUrl     = `${process.env.SITE_URL}/resend-key`;

  const htmlBody = `
    <div style="font-family:Tahoma,sans-serif;max-width:480px;margin:0 auto;color:#1a3a5c">

      <!-- Header banner -->
      <div style="background:linear-gradient(180deg,#1c6fbe 0%,#0a3878 100%);padding:24px 28px;border-radius:8px 8px 0 0;position:relative;overflow:hidden">
        <h1 style="color:#fff;font-size:22px;margin:0 0 4px;font-family:'Trebuchet MS',sans-serif;letter-spacing:0.02em">SongSender</h1>
        <p style="color:rgba(180,220,255,0.88);font-size:12px;margin:0;letter-spacing:0.04em">Plugin subscription · you're in</p>
      </div>

      <!-- Body -->
      <div style="background:#f4f8ff;padding:24px 28px;border:1px solid #c0d8f0;border-top:none;border-radius:0 0 8px 8px">

        <p style="font-size:13px;color:#0a3878;margin:0 0 14px;font-weight:bold">
          Your subscription is active. Here's your license key:
        </p>

        <!-- License key box -->
        <div style="background:#e0ecff;border:1px solid #a0c0e8;border-left:4px solid #2288dd;padding:12px 16px;border-radius:0 4px 4px 0;margin-bottom:16px">
          <div style="font-size:10px;color:#6888aa;margin-bottom:4px;letter-spacing:0.1em;text-transform:uppercase">License key</div>
          <div style="font-family:'Courier New',monospace;font-size:15px;color:#0a3878;letter-spacing:0.06em;font-weight:bold">${licenseKey}</div>
        </div>

        <!-- Instructions -->
        <p style="font-size:12px;color:#3a5878;line-height:1.7;margin:0 0 6px">
          <strong>Step 1.</strong> Download SongSender from the link below — it's a public download, no login needed.<br>
          <strong>Step 2.</strong> Install it. On first launch, enter your email address and the license key above.<br>
          <strong>Step 3.</strong> That's it. The plugin checks your subscription is active and unlocks.
        </p>

        <p style="font-size:12px;color:#3a5878;line-height:1.7;margin:0 0 20px">
          Keep this email. Every future plugin I release goes to this same address automatically — same key, same price, nothing extra to do.
        </p>

        <!-- Download button -->
        <a href="${downloadPageUrl}" style="display:inline-block;padding:11px 24px;background:linear-gradient(180deg,#44c8f8 0%,#0068a8 100%);color:#fff;text-decoration:none;border-radius:5px;font-weight:bold;font-size:13px;font-family:Tahoma,sans-serif;letter-spacing:0.02em">
          ↓ &nbsp;Download SongSender
        </a>

        <!-- Footer links -->
        <p style="font-size:11px;color:#6888aa;margin-top:22px;line-height:1.8;border-top:1px solid #c0d8f0;padding-top:14px">
          Lost this email? <a href="${resendKeyUrl}" style="color:#2288dd">Resend your license key</a><br>
          Manage or cancel your subscription: <a href="${portalUrl}" style="color:#2288dd">Subscription settings</a>
        </p>

      </div>
    </div>
  `;

  try {
    await mailer.sendMail({
      from:    `"SongSender" <${process.env.SMTP_FROM}>`,
      to:      toEmail,
      subject: 'SongSender — your license key',
      html:    htmlBody,
    });
    console.log(`📧 Welcome email sent to ${toEmail}`);
  } catch (err) {
    console.error('Email send error:', err.message);
  }
}

/* ──────────────────────────────────────────────────────
   ROUTE: POST /api/resend-key

   If a subscriber lost their email, they enter their
   email address on /resend-key.html and we look up
   their license key and resend it.
   This prevents someone who never paid from getting a key —
   we only send it if the email exists in our subscriber DB.
────────────────────────────────────────────────────── */
app.post('/api/resend-key', async (req, res) => {
  const { email } = req.body;

  if (!email) {
    return res.status(400).json({ error: 'Email is required.' });
  }

  try {
    // TODO: look up the subscriber in your database
    // const subscriber = await db.subscribers.findOne({ email });
    // if (!subscriber) {
    //   // Don't reveal whether the email exists — just say "sent if found"
    //   return res.json({ sent: true });
    // }
    // await sendWelcomeEmail(subscriber.email, subscriber.licenseKey, subscriber.subscriptionId);

    // PLACEHOLDER — always returns success to avoid email enumeration
    console.log(`🔑 Key resend requested for: ${email}`);
    return res.json({ sent: true });

  } catch (err) {
    console.error('Resend key error:', err);
    res.status(500).json({ error: 'Server error.' });
  }
});

/* ──────────────────────────────────────────────────────
   START SERVER
────────────────────────────────────────────────────── */
const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`SongSender server running on port ${PORT}`);
  console.log(`Site URL: ${process.env.SITE_URL}`);
});
