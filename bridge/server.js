const { spawn } = require('child_process');
const readline = require('readline');
const fs = require('fs');
const net = require('net');
const path = require('path');
const { WebSocketServer } = require('ws');
require('dotenv').config({ path: path.resolve(__dirname, '../.env') });

const WS_PORT = process.env.WS_PORT || 8080;
const MODE = process.env.STREAM_MODE || 'replay';
const READSB_HOST = process.env.READSB_HOST || '127.0.0.1';
const READSB_PORT = process.env.READSB_PORT || 30047;
const sampleRelPath = process.env.SAMPLE_PATH || 'sample_data/brussels_live.ndjson';
const SAMPLE_FILE = path.resolve(__dirname, '..', sampleRelPath);
const KALMAN_BIN = path.resolve(__dirname, '../build/kalman_server');

const wss = new WebSocketServer({ port: WS_PORT });
console.log(`[Bridge] WebSocket server listening on ws://localhost:${WS_PORT}`);

const kalmanProc = spawn(KALMAN_BIN, [], {
	env: { ...process.env }
});

// Forward server stderr to the terminal
kalmanProc.stderr.on('data', (data) => {
	process.stderr.write(data.toString());
});

// Read server stdout and broadcast to browsers
const rlOut = readline.createInterface({ input: kalmanProc.stdout });
rlOut.on('line', (jsonLine) => {
	for (const client of wss.clients) {
		if (client.readyState === 1) {
			client.send(jsonLine);
		}
	}
});

// Feed Data into kalman server std::cin
if (MODE === 'live') {
	console.log(`[Bridge] Connecting to live readsb at ${READSB_HOST}:${READSB_PORT}...`);
	const client = net.createConnection({ host: READSB_HOST, port: READSB_PORT }, () => {
		console.log('[Bridge] Connected to readsb! Piping live ADS-B to C++ Kalman engine...');
	});
	client.pipe(kalmanProc.stdin);
	client.on('error', (err) => console.error('[Bridge] readsb TCP error:', err.message));
} else {
	console.log(`[Bridge] Starting real-time timestamp-synced replay of ${SAMPLE_FILE}...`);
	const lines = fs.readFileSync(SAMPLE_FILE, 'utf-8').split('\n').filter(Boolean);
	let idx = 0;

	function sendNextPing() {
		if (idx >= lines.length) {
			console.log('[Bridge] Reached end of sample file.');
			return; // Stops cleanly at the end of the file!
		}

		const currentLine = lines[idx];
		kalmanProc.stdin.write(currentLine + '\n');
		idx++;

		if (idx < lines.length) {
			try {
				const currTime = JSON.parse(currentLine).now;
				const nextTime = JSON.parse(lines[idx]).now;
				// Calculate real-time delay in milliseconds, wrapped by max(0, to protect against out of order pings
				const delayMs = Math.max(0, (nextTime - currTime) * 1000);
				setTimeout(sendNextPing, delayMs);
			} catch (e) {
				setTimeout(sendNextPing, 50);
			}
		}
	}

	sendNextPing();
}