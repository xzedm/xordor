import { defineConfig } from 'vite';
import { viteSingleFile } from 'vite-plugin-singlefile';

// The build is ONE html file with everything inside, so it can be opened by
// double-clicking, hosted anywhere, and used to publish games.
export default defineConfig({
  base: './',
  plugins: [viteSingleFile({ removeViteModuleLoader: true })],
  build: {
    target: 'es2022',
    chunkSizeWarningLimit: 4000,
    assetsInlineLimit: 100000000,
  },
  test: {
    environment: 'node',
    include: ['tests/**/*.test.js'],
  },
});
