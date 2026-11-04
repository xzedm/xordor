// Development mode: starts the Vite dev server (instant reloads) and opens the
// desktop app pointed at it. Close the app window to stop.
import { spawn } from 'node:child_process';
import { createServer } from 'vite';
import electronPath from 'electron';

const server = await createServer({ server: { port: 5173, strictPort: false } });
await server.listen();
const url = server.resolvedUrls.local[0];
console.log(`[xordor] dev server at ${url}`);

const child = spawn(electronPath, ['.'], { stdio: 'inherit', env: { ...process.env, XORDOR_DEV_URL: url } });
child.on('exit', async (code) => {
  await server.close();
  process.exit(code ?? 0);
});
