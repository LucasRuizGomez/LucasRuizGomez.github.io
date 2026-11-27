// Everything the pages say about Lucas. Source of truth: the CV in public/ (keep them in step).
// Screenshots come from /docs at the repo root (1280x800), the same files the README uses.
import type { ImageMetadata } from 'astro';
import audiosendShot from '../../../docs/audiosend.png';
import rpgShot from '../../../docs/rpg.png';
import wordlistShot from '../../../docs/wordlist.png';
import portfolioShot from '../../../docs/portfolio.png';
import filekitShot from '../../../docs/filekit.png';
import noteReadShot from '../../../docs/noteread.png';
import boardShot from '../../../docs/board.png';
import siteShot from '../../../docs/lukeda.png';
import rumboShot from '../../../docs/rumbo.png';
import { links } from '../config';

export interface Link {
  label: string;
  href: string;
}

export interface Project {
  name: string;
  summary: string; // painted on the container door: one line
  detail: string; // the manifest line: what is actually inside
  stack: string[];
  image: ImageMetadata;
  imageAlt: string;
  links: Link[];
}

const repoTree = (path: string) => `${links.repo}/tree/main/${path}`;

// The cargo. Order is the stowage order: left to right, top tier first.
export const projects: Project[] = [
  {
    name: 'Hollow Crypt',
    summary: 'Open-world action RPG in C++20, compiled to WebAssembly.',
    detail:
      'Six regions, twelve dungeons, 57 quests, crafting and a 33-skill tree. The game logic has no SDL dependency, so its unit tests run headless; GitHub Actions builds the WebAssembly bundle and runs them on every push before the site deploys.',
    stack: ['C++20', 'SDL2', 'Emscripten', 'CMake', 'doctest'],
    image: rpgShot,
    imageAlt: 'Hollow Crypt: the hooded hero in the village of Greenhollow, with minimap and hotbar',
    links: [
      { label: 'Play', href: '/rpg/' },
      { label: 'Source', href: repoTree('rpg') },
    ],
  },
  {
    name: 'AudioSend',
    summary: 'VST3 plugin that sends a mix from the DAW to your phone.',
    detail:
      'A built-in HTTP server streams exports to the phone over local Wi-Fi (scan a QR code, play in the browser). Every file gets a mix analysis: integrated LUFS (ITU-R BS.1770-4), 4x oversampled true peak, dynamic range and stereo width. Open source, AGPLv3.',
    stack: ['C++', 'JUCE', 'VST3'],
    image: audiosendShot,
    imageAlt: 'AudioSend in FL Studio: file list with loudness badges and the mix analysis panel',
    links: [{ label: 'Source', href: 'https://github.com/LucasRuizGomez/Audiosend' }],
  },
  {
    name: 'Wortschatz',
    summary: 'Spaced-repetition flashcards for the C1 German exam.',
    detail:
      'FSRS-6 scheduler written from scratch, matching the reference ts-fsrs to within 1e-6. Claude writes the cards from a word list through a Supabase Edge Function. Offline PWA that syncs between devices without a late answer overwriting a newer one.',
    stack: ['JavaScript', 'Supabase', 'Postgres', 'Claude API'],
    image: wordlistShot,
    imageAlt: 'Wortschatz flashcard showing der Vorreiter with its meaning, forms and an example',
    links: [
      { label: 'Open', href: '/wordlist/' },
      { label: 'Source', href: repoTree('wordlist') },
    ],
  },
  {
    name: 'Rumbo',
    summary: 'Voice tax copilot for digital nomads. HackFinance 2026.',
    detail:
      'Built by a team of six at HackFinance 2026 (Madrid Engineering Week). Rumbo counts the days spent in each country, estimates where you are tax resident this year and why, warns before a stay crosses a threshold, and sets tax aside from every invoice; a voice advisor answers with your own data. I wrote the product spec the team built from, the first working prototype (residency engine for five countries, ElevenLabs voice agent, web app), and the React front end that shipped in the final app, then gave the pitch.',
    stack: ['JavaScript', 'React', 'Python', 'FastAPI', 'ElevenLabs', 'Claude API', 'MyInvestor (mock)'],
    image: rumboShot,
    imageAlt: 'Rumbo dashboard: 140 days in Portugal with 43 left before residency, estimated tax residence Spain',
    links: [
      { label: 'Source', href: 'https://github.com/LucasRuizGomez/rumbo-hackfinance2026' },
      { label: 'My branch', href: 'https://github.com/LucasRuizGomez/rumbo-hackfinance2026/tree/lucas/motor-fiscal' },
    ],
  },
  {
    name: 'Portfolio',
    summary: 'Stock tracker with live prices, P&L and day change.',
    detail:
      'Positions stay in the browser; quotes come from the Finnhub API with the user’s own free key. Totals, P&L and day change update on refresh.',
    stack: ['JavaScript', 'Chart.js', 'Finnhub API'],
    image: portfolioShot,
    imageAlt: 'Portfolio tracker with five positions, total value and P&L (sample data)',
    links: [{ label: 'Open', href: '/stockview/' }],
  },
  {
    name: 'Filekit',
    summary: 'Merge, convert and edit PDFs without uploading them.',
    detail:
      'Merge PDFs, turn images and Word files into PDFs and edit pages. Everything runs client-side in the tab.',
    stack: ['JavaScript', 'pdf-lib', 'PDF.js'],
    image: filekitShot,
    imageAlt: 'Filekit merge tool with two drop zones',
    links: [{ label: 'Open', href: '/filekit/' }],
  },
  {
    name: 'NoteRead',
    summary: 'Sight-reading trainer that times every answer.',
    detail:
      'A note appears on the staff and you name it. It tracks streak, accuracy and reaction time, and repeats the notes you miss more often.',
    stack: ['JavaScript', 'SVG', 'Web Audio API'],
    image: noteReadShot,
    imageAlt: 'NoteRead with a note on a treble staff and the answer buttons',
    links: [{ label: 'Open', href: '/noteread/' }],
  },
  {
    name: 'My Little Board',
    summary: 'Sticky-note board with a checklist on each note.',
    detail: 'Click to place a note, drag to move it, scroll to rotate it, tick off tasks.',
    stack: ['JavaScript', 'HTML', 'CSS'],
    image: boardShot,
    imageAlt: 'My Little Board with five sticky notes holding task lists',
    links: [{ label: 'Open', href: '/positwebsite/' }],
  },
  {
    name: 'lukeda.me',
    summary: 'This site: the portfolio and the tools it ships.',
    detail:
      'Astro on the Halftone theme for these pages; each tool is a static page with its own test page. One GitHub Actions workflow builds the game and the site and deploys both to GitHub Pages.',
    stack: ['Astro', 'Tailwind CSS', 'GitHub Actions'],
    image: siteShot,
    imageAlt: 'The lukeda.me home page',
    links: [
      { label: 'Source', href: links.repo },
    ],
  },
];

export interface Award {
  when: string;
  title: string;
  note?: string;
}

export const competitions: Award[] = [
  {
    when: '2026',
    title: '1st place, MOEVE Innovation Challenge',
    note: 'Project LINCE: a model that quantifies worker focus before high-risk tasks, to improve safety at energy parks. Won the preliminary round and the final.',
  },
  {
    when: 'Nov 2026',
    title: 'Rumbo, HackFinance 2026',
    note: 'Voice tax copilot for digital nomads, built by a team of six at the Madrid Engineering Week hackathon on the sponsors’ stack: an ElevenLabs voice agent, the Claude API and a mock of MyInvestor’s banking API. I wrote the spec and the front end, and gave the pitch.',
  },
  {
    when: '2024',
    title: '1st place, Edumundo Business Simulation',
    note: 'During the exchange year at FH Kufstein Tirol, Austria.',
  },
  { when: '2022', title: 'Category winner, T3chFest Hackathon' },
];

export const interests =
  'Music production, piano, skiing and long-term equity investing.';

// ── The CV, as the /resume page sets it. Keep in step with public/Lucas_Ruiz_Gomez_CV.pdf ──

export interface Entry {
  when: string;
  title: string;
  org: string;
  orgHref?: string;
  place?: string;
  note?: string;
  bullets?: string[];
}

export const education: Entry[] = [
  {
    when: 'Expected Jan 2028',
    title: 'Dual Degree, Computer Science & Business Administration',
    org: 'Universidad Carlos III de Madrid',
    place: 'Madrid',
    bullets: [
      'GPA 7.3/10 (UK 2:1 equivalent). Top grades in Macroeconomics (10/10), Financial Management, Game Theory and Strategic Management (9.5/10).',
    ],
  },
  {
    when: 'Oct 2024 — Jul 2025',
    title: 'Exchange, Business Studies',
    org: 'FH Kufstein Tirol – University of Applied Sciences',
    place: 'Austria',
    bullets: [
      'Exchange-year average 9.4/10. Courses included Project Management, International Economics and Energy Business.',
    ],
  },
  {
    when: 'Sep 2016 — Jun 2022',
    title: 'High School',
    org: 'IES Francisco Javier de Uriarte',
    place: 'Spain',
    bullets: [
      'Grade 9.3/10. Class President for three years.',
      'Amancio Ortega Scholarship (6% acceptance): full scholarship for Grade 11 in Canada.',
    ],
  },
];

export const experience: Entry[] = [
  {
    when: 'Oct 2026 — Present',
    title: 'Founder & Analyst',
    org: 'Grau Pedreira',
    orgHref: links.research,
    place: 'Madrid',
    bullets: [
      'Launched a research firm covering oil, gas and LNG markets in North Africa and the Gulf of Guinea (Algeria, Nigeria, Senegal, Mauritania) for Spanish investors.',
      'Publish Radar diario, a daily oil and gas newsletter (13 editions), and a 10-page report on the African Atlantic Gas Pipeline (Nigeria–Morocco) and its impact on Spain’s gas security.',
      'Interviewed potential clients to test pricing and demand; building a network of African companies to supply proprietary data in exchange for visibility with Spanish investors.',
    ],
  },
  {
    when: 'Jul — Oct 2026',
    title: 'IT Strategy Intern',
    org: 'Capgemini Invent',
    place: 'Madrid',
    note: 'Client: one of Europe’s largest integrated energy companies.',
    bullets: [
      'Defined IT architecture solutions and co-authored Solution Documents for 10+ product initiatives across the client’s Customer unit (power and gas, retail fuel, lubricants, aviation).',
      'Built a validated inventory of 300+ applications and dependencies in SAP LeanIX with application owners, creating architecture views that flagged redundant and end-of-life systems for rationalisation.',
      'Supported senior client stakeholders in steering meetings, tracking decisions and actions across initiatives against the target architecture.',
    ],
  },
  {
    when: 'Oct 2023 — Jun 2024',
    title: 'Associate, Communications',
    org: 'Start UC3M – Entrepreneurship Association',
    place: 'Madrid',
    bullets: [
      'Ran data-driven digital campaigns for entrepreneurship events, growing attendance to 100+ participants and reaching 100K+ views.',
    ],
  },
];

export const achievements: Entry[] = [
  {
    when: '2026',
    title: 'First Place, MOEVE Innovation Challenge',
    org: 'Project LINCE',
    bullets: [
      'Built a model that quantifies worker focus before high-risk tasks to improve safety at energy parks. Won the preliminary round and the overall competition.',
    ],
  },
  { when: '2024', title: 'First Place, Edumundo Business Simulation', org: 'FH Kufstein Tirol' },
  { when: '2022', title: 'Category Winner, T3chFest Hackathon', org: 'T3chFest' },
  {
    when: '2014 — 2022',
    title: 'Eagle Scout',
    org: 'Boy Scouts of America (top ~4%)',
    bullets: ['Five years in leadership roles. Led 12 volunteers in a 290-hour service project.'],
  },
  {
    when: '2021',
    title: 'Aditus Programme',
    org: 'Selected for the national high-potential student programme (5th cohort)',
  },
  {
    when: 'Sep 2026 — Present',
    title: 'Member',
    org: 'Madrid Banking & Finance Society, Madrid’s cross-university finance platform',
  },
];

export const cvSkills: { label: string; value: string }[] = [
  {
    label: 'Languages',
    value: 'Spanish (native) · English (C2, IELTS 8.5, TOEFL 115/120) · German (Goethe-Zertifikat B2)',
  },
  { label: 'Technical', value: 'Excel, Python, SQL, SAP LeanIX' },
  {
    label: 'Projects',
    value: 'lukeda.me: built and run a site of free web tools, including a stock tracker with live market data',
  },
  { label: 'Interests', value: 'Long-term equity investing, energy transition in EMEA, music production, piano, skiing' },
];
