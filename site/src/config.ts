// Site settings. Everything site-specific lives here or in src/data/profile.ts; never hardcode
// it in a component.
import defaultOgImage from './assets/the-cranes-at-kieldrecht.avif';

export const site = {
  name: 'Lucas Ruiz Gómez',
  description:
    'Lucas Ruiz Gómez: Computer Science and Business student at UC3M in Madrid. Software projects in C++ and JavaScript, plus energy market research.',
  url: 'https://lukeda.me',
  locale: 'en',
  author: 'Lucas Ruiz Gómez',
  defaultOgImage,
} as const;

export const links = {
  cv: '/Lucas_Ruiz_Gomez_CV.pdf',
  github: 'https://github.com/LucasRuizGomez',
  linkedin: 'https://www.linkedin.com/in/lucas-ruiz-gomez/',
  research: 'https://graupedreira.es',
  repo: 'https://github.com/LucasRuizGomez/LucasRuizGomez.github.io',
} as const;

export const nav = {
  header: [
    { label: 'Projects', href: '/#projects' },
    { label: 'About', href: '/#about' },
    { label: 'CV', href: '/resume' },
    { label: 'GitHub', href: links.github },
  ],
  social: [
    { label: 'GitHub', href: links.github, icon: 'lucide:github' },
    { label: 'LinkedIn', href: links.linkedin, icon: 'lucide:linkedin' },
    { label: 'CV (PDF)', href: links.cv, icon: 'lucide:file-text' },
  ],
} as const;

export const seo = {
  titleTemplate: '%s · Lucas Ruiz Gómez',
};

export const features = {
  darkMode: true,
};
