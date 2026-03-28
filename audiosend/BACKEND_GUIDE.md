# SongSender — Backend Architecture Guide

Everything you need to know to go from zero to taking real payments.

---

## The Big Picture — How It All Works

```
User clicks "Get started"
        ↓
main.js calls your server: POST /api/create-checkout-session
        ↓
Your server calls Stripe's API with your SECRET key
Stripe returns a sessionId
        ↓
Browser redirects to Stripe's hosted checkout page
(Stripe asks for email + card — you never touch card data)
        ↓
Payment succeeds
        ↓
Stripe redirects user to your /success.html
        ↓
Stripe ALSO calls your webhook: POST /api/webhook
Your server receives the event, generates a license key,
sends a welcome email with the download link
        ↓
User enters license key in the plugin on first launch
Plugin calls GET /api/verify-license
Your server checks with Stripe that subscription is still active
Plugin unlocks (or stays locked if subscription cancelled)
```

---

## Do Subscribers Need to Create an Account?

**No. They don't.**

Here's the reasoning: Stripe Checkout asks for an email address as part of payment. That email becomes their identity. After payment you:

1. Generate a unique license key tied to their email
2. Email them the key + download link
3. They enter email + key into the plugin once

The plugin calls your `/api/verify-license` endpoint on each launch. You check against Stripe whether the subscription is still active. That's the whole "account" — email + license key. No passwords, no sign-up forms, no user database beyond a simple subscriber table.

**When would you want accounts?**
Only if you want a customer dashboard where they can see their license key, re-download, manage their subscription from your site rather than Stripe's portal. For a solo creator selling $4.99/month, Stripe's built-in Customer Portal handles all of that. Recommended: use Stripe's portal and skip building your own account system entirely.

---

## Technology Stack — Recommendations

### Payments
**Stripe** — the only serious choice at this scale.
- Handles PCI compliance so you never touch card data
- Built-in subscription management, invoices, receipts
- Customer Portal handles cancellations and billing updates
- Takes ~2.9% + 30¢ per transaction (standard rate)
- At $4.99/month you net approximately $3.84 per subscriber

### Backend Server
**Node.js + Express** — what server.js is already built with.

Alternatives if you prefer:
- **Python + FastAPI** — same concepts, cleaner syntax
- **Python + Flask** — even simpler, fine for low traffic

### Hosting — Where to Run Your Server

| Option | Cost | Effort | Recommendation |
|---|---|---|---|
| **Railway** | ~$5/mo | Very easy | ✅ Best for solo creators |
| **Render** | Free tier, then $7/mo | Easy | ✅ Good free start |
| **Fly.io** | ~$3/mo | Medium | Good performance |
| **Vercel** | Free (serverless) | Medium | Works but webhook handling is trickier |
| **DigitalOcean Droplet** | $6/mo | Manual setup | Fine if you know Linux |
| **VPS (Hetzner)** | €4/mo | Manual setup | Cheapest long-term |

**Recommendation for you right now: Railway.**
Push your code to GitHub, connect Railway to the repo, set your environment variables in the Railway dashboard, done. No server setup, no SSH. It auto-deploys on every git push. Their free tier is enough to start; $5/month plan once you have paying subscribers.

### Database — Storing Subscriber Records

You need to store: email, licenseKey, subscriptionId, customerId, active (bool), createdAt.

| Option | Cost | Effort | Recommendation |
|---|---|---|---|
| **SQLite (file-based)** | Free | Very easy | ✅ Fine for < 500 subscribers |
| **Postgres on Railway** | ~$5/mo | Easy | ✅ Best long-term |
| **Supabase** | Free tier | Easy | Good Postgres with dashboard |
| **PlanetScale** | Free tier | Easy | MySQL, good UI |
| **MongoDB Atlas** | Free tier | Easy | If you prefer NoSQL |

**Recommendation: SQLite to start, migrate to Postgres when you have 100+ subscribers.**

For SQLite in Node.js, use the `better-sqlite3` package:
```
npm install better-sqlite3
```

### Email — Sending License Keys

| Option | Cost | Limit | Recommendation |
|---|---|---|---|
| **Gmail + App Password** | Free | 500/day | ✅ Fine to start |
| **Resend** | Free (3k/mo), then $20/mo | Generous free tier | ✅ Best DX |
| **Postmark** | $15/mo | 10k/mo | Best deliverability |
| **SendGrid** | Free (100/day) | 100/day free | Okay |
| **Brevo (Sendinblue)** | Free (300/day) | 300/day free | Fine |

**Recommendation: Start with Gmail App Password. Switch to Resend when you want a custom domain email (e.g. hello@songsender.com) — Resend has the simplest setup.**

---

## Step-by-Step: Going Live

### Step 1 — Set up Stripe

1. Create a Stripe account at stripe.com
2. Go to **Products** → **Add Product**
   - Name: "SongSender Creator Pass"
   - Price: $4.99/month, recurring
   - Copy the **Price ID** (starts with `price_...`)
3. Go to **Developers** → **API Keys**
   - Copy your **Secret Key** (`sk_live_...`)
   - Copy your **Publishable Key** (`pk_live_...`) — goes in main.js
4. Go to **Developers** → **Webhooks** → **Add endpoint**
   - URL: `https://yourdomain.com/api/webhook`
   - Events: select `checkout.session.completed`, `customer.subscription.deleted`, `invoice.payment_failed`
   - Copy the **Signing Secret** (`whsec_...`)
5. Enable **Customer Portal** (Stripe Dashboard → Billing → Customer portal)
   - This gives subscribers a self-service page to cancel/update billing

### Step 2 — Set up your server

```bash
# Clone or create your project folder
mkdir songsender-server && cd songsender-server

# Copy server.js, package.json into this folder
npm install express stripe nodemailer dotenv cors

# Copy .env.example to .env and fill in all values
cp .env.example .env
nano .env
```

### Step 3 — Deploy to Railway

1. Push your code to a GitHub repo (keep .env out of git — it's in .gitignore)
2. Go to railway.app → New Project → Deploy from GitHub repo
3. Add your environment variables in Railway's dashboard (Variables tab)
4. Railway gives you a public URL like `https://songsender-abc123.railway.app`
5. Use that URL as your SITE_URL in .env and in your Stripe webhook endpoint

### Step 4 — Test the flow

1. In Stripe, switch to **Test mode**
2. Use the test publishable key in main.js
3. Use Stripe's test card: `4242 4242 4242 4242`, any future expiry, any CVV
4. Complete a test checkout
5. Check that the webhook fires (Stripe dashboard shows webhook attempts)
6. Check that you receive the email
7. Verify the license key works in the plugin

### Step 5 — Go live

1. Switch to your live Stripe keys
2. Update main.js with the live publishable key
3. Redeploy

---

## How the Plugin Validates the License

In your JUCE C++ plugin, on startup:

```cpp
// Pseudo-code — adapt to your HTTP library (juce::URL works)
String email = getUserEmail(); // stored in local settings after first entry
String licenseKey = getLicenseKey(); // stored in local settings

URL verifyUrl("https://yourdomain.com/api/verify-license");
verifyUrl = verifyUrl.withParameter("email", email)
                     .withParameter("licenseKey", licenseKey);

String response = verifyUrl.readEntireTextStream();
var json = JSON::parse(response);

bool valid = json["valid"]; // true or false

if (!valid) {
  // Show a "Your subscription is not active" dialog
  // Disable plugin functionality or show a paywall
}
```

Cache the result locally for 24 hours so the plugin doesn't need a network connection constantly. Re-check on each DAW launch.

---

## File Structure

```
songsender/
├── public/              ← static files served by Express
│   ├── index.html
│   ├── style.css
│   ├── main.js
│   ├── success.html
│   └── cancel.html
├── server.js            ← Express backend
├── .env                 ← your secrets (NOT in git)
├── .env.example         ← template (safe to commit)
├── .gitignore
└── package.json
```

### .gitignore
```
node_modules/
.env
*.sqlite
```

### package.json
```json
{
  "name": "songsender-server",
  "version": "1.0.0",
  "main": "server.js",
  "scripts": {
    "start": "node server.js",
    "dev": "node --watch server.js"
  },
  "dependencies": {
    "better-sqlite3": "^9.4.3",
    "cors": "^2.8.5",
    "dotenv": "^16.4.5",
    "express": "^4.18.3",
    "nodemailer": "^6.9.12",
    "stripe": "^14.20.0"
  }
}
```

---

## Cost Summary — Running SongSender at Scale

| Subscribers | Monthly Revenue | Stripe Fees | Server + Email | Your Take |
|---|---|---|---|---|
| 10 | $49.90 | ~$4.44 | ~$5 free tiers | ~$40 |
| 50 | $249.50 | ~$22 | ~$5 | ~$222 |
| 100 | $499 | ~$44 | ~$10 | ~$445 |
| 500 | $2,495 | ~$221 | ~$20 | ~$2,254 |

Server costs don't scale much. Stripe fees are the main variable cost.

---

## Summary of Decisions Made For You

| Decision | Choice | Why |
|---|---|---|
| Payment processor | Stripe | Industry standard, best docs, best reliability |
| Checkout flow | Stripe hosted Checkout | No PCI liability, mobile-optimised, handles SCA/3DS |
| Account system | Email + license key | No sign-up friction. Stripe handles identity |
| Subscription portal | Stripe Customer Portal | Free, handles cancellations/upgrades, zero code |
| License validation | Server-side API call | Can revoke access instantly on cancellation |
| Backend framework | Node.js + Express | Minimal, fast to set up, matches existing JS |
| Hosting | Railway | Easiest deploy for a solo creator |
| Database | SQLite → Postgres | Start simple, scale when needed |
| Email | Gmail → Resend | Start free, upgrade for custom domain |
