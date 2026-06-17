import { defineConfig, passthroughImageService } from 'astro/config';
import starlight from '@astrojs/starlight';

const REPO = 'https://github.com/Kinoshita-Electronics-Consortium/kn86-ui';

// Amber-monochrome syntax theme for fenced code blocks (Expressive Code).
// Lifted from kec-lisp / kn86-docs so the sites read as one brand.
const kecAmberCodeTheme = {
  name: 'kec-amber',
  type: 'dark',
  colors: {
    'editor.background': '#000000',
    'editor.foreground': '#e6a020',
    'editorLineNumber.foreground': '#5a554c',
    'editorLineNumber.activeForeground': '#e6a020',
  },
  tokenColors: [
    { scope: ['comment', 'punctuation.definition.comment'], settings: { foreground: '#8f8a7d', fontStyle: 'italic' } },
    { scope: ['string', 'string.quoted', 'constant.other.symbol', 'meta.attribute-selector'], settings: { foreground: '#ffd27f' } },
    { scope: ['constant.numeric', 'constant.language', 'constant.character', 'support.constant'], settings: { foreground: '#ffd27f' } },
    { scope: ['keyword', 'keyword.control', 'storage', 'storage.type', 'storage.modifier', 'keyword.operator.logical'], settings: { foreground: '#f4b94e' } },
    { scope: ['entity.name.function', 'support.function', 'meta.function-call', 'variable.function'], settings: { foreground: '#e6a020' } },
    { scope: ['entity.name.type', 'support.type', 'support.class', 'entity.name.class', 'entity.other.inherited-class'], settings: { foreground: '#f4b94e' } },
    { scope: ['variable', 'variable.other', 'variable.parameter', 'meta.definition.variable'], settings: { foreground: '#e8e4df' } },
    { scope: ['punctuation', 'meta.brace', 'keyword.operator'], settings: { foreground: '#9c9689' } },
    { scope: ['markup.heading', 'markup.bold'], settings: { foreground: '#e6a020', fontStyle: 'bold' } },
    { scope: ['markup.inline.raw', 'markup.raw'], settings: { foreground: '#ffd27f' } },
  ],
};

export default defineConfig({
  site: 'https://kinoshita-electronics-consortium.github.io',
  base: '/kn86-ui',
  image: { service: passthroughImageService() },
  vite: { server: { fs: { allow: ['..'] } } },
  integrations: [
    starlight({
      title: 'KN-86 UI',
      description: 'The KN-86 Deckline UI component kit + screen render engine — the default amber-on-black terminal UI library, authored in KEC Lisp.',
      logo: { src: './src/assets/kn86-ui-logo.svg', alt: 'KN-86 UI' },
      favicon: '/favicon.svg',
      customCss: ['./src/styles/kec.css'],
      social: [{ icon: 'github', label: 'GitHub', href: REPO }],
      editLink: { baseUrl: `${REPO}/edit/main/` },
      expressiveCode: {
        themes: [kecAmberCodeTheme],
        styleOverrides: {
          borderRadius: '0',
          borderColor: 'rgba(230, 160, 32, 0.25)',
          codeFontFamily: "'JetBrains Mono', 'SF Mono', Menlo, Consolas, monospace",
          uiFontFamily: "'JetBrains Mono', 'SF Mono', Menlo, Consolas, monospace",
          codeBackground: '#000000',
          frames: {
            editorBackground: '#000000',
            editorActiveTabBackground: '#000000',
            editorTabBarBackground: '#050505',
            terminalBackground: '#000000',
            terminalTitlebarBackground: '#050505',
            frameBoxShadowCssValue: 'none',
          },
        },
      },
      head: [
        { tag: 'link', attrs: { rel: 'preconnect', href: 'https://fonts.googleapis.com' } },
        { tag: 'link', attrs: { rel: 'preconnect', href: 'https://fonts.gstatic.com', crossorigin: true } },
        {
          tag: 'link',
          attrs: {
            rel: 'stylesheet',
            href: 'https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;500;600;700&family=Press+Start+2P&family=Space+Grotesk:wght@400;500;600;700&display=swap',
          },
        },
      ],
      sidebar: [
        { label: 'Start Here', items: [{ label: 'Overview', slug: 'index' }] },
        {
          label: 'The Kit',
          items: [
            { label: 'Component Kit', slug: 'component-kit' },
            { label: 'Screen DSL', slug: 'screen-dsl' },
          ],
        },
        {
          label: 'Design Language',
          items: [
            { label: 'UI Design Language', slug: 'ui-design-language' },
            { label: 'Screen Design Rules', slug: 'screen-design-rules' },
            { label: 'UI Patterns', slug: 'ui-patterns' },
            { label: 'Character Set', slug: 'character-set' },
          ],
        },
        {
          label: 'Project',
          items: [
            { label: 'Engineering docs (kn86-docs)', link: 'https://kn86-deckline.com', attrs: { target: '_blank' } },
            { label: 'Source (GitHub)', link: REPO, attrs: { target: '_blank' } },
          ],
        },
      ],
    }),
  ],
});
