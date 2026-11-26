// @ts-check
import { fileURLToPath } from 'node:url';
import { defineConfig, fontProviders } from 'astro/config';
import tailwindcss from '@tailwindcss/vite';
import sitemap from '@astrojs/sitemap';
import icon from 'astro-icon';
import { site } from './src/config.ts';

// https://astro.build/config
export default defineConfig({
  site: process.env.SITE_URL ?? site.url,
  integrations: [sitemap(), icon()],
  // Webfonts, fetched at build time and served from this origin. Schibsted Grotesk carries
  // headlines, the name and UI; Spectral the body; IBM Plex Mono the kickers and folios.
  fonts: [
    {
      provider: fontProviders.fontsource(),
      name: 'Spectral',
      cssVariable: '--font-body',
      weights: [400, 600],
      styles: ['normal', 'italic'],
      subsets: ['latin'],
      fallbacks: ['Georgia', 'serif'],
    },
    {
      provider: fontProviders.fontsource(),
      name: 'Schibsted Grotesk',
      cssVariable: '--font-heading',
      weights: [400, 500, 700, 800, 900],
      styles: ['normal'],
      subsets: ['latin'],
      fallbacks: ['Helvetica Neue', 'Arial', 'sans-serif'],
    },
    {
      provider: fontProviders.fontsource(),
      name: 'IBM Plex Mono',
      cssVariable: '--font-mono',
      weights: [400, 600],
      styles: ['normal'],
      subsets: ['latin'],
      fallbacks: ['SF Mono', 'Menlo', 'monospace'],
    },
  ],
  vite: {
    plugins: [tailwindcss()],
    // The project screenshots live in /docs at the repo root, shared with the README.
    server: { fs: { allow: [fileURLToPath(new URL('..', import.meta.url))] } },
    build: {
      // Keep light-dark() native; lowering it to prefers-color-scheme blocks would break the
      // theme toggle. Older browsers fall back to the light values in tokens.css.
      cssTarget: ['chrome123', 'edge123', 'firefox120', 'safari17.5'],
    },
  },
});
