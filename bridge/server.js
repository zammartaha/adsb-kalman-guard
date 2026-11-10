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

let spoofTarget = process.env.SPOOF_HEX || 'NONE';
const pingCounts = new Map();

const wss = new WebSocketServer({ port: WS_PORT });
console.log(`[Bridge] WebSocket server listening on ws://localhost:${WS_PORT}`);

function broadcastSpoofConfig() {
	const msg = JSON.stringify({ type: 'SPOOF_CONFIG', target: spoofTarget });
	for (const client of wss.clients) {
		if (client.readyState === 1) client.send(msg);
	}
}

wss.on('connection', (ws) => {
	// Send current spoof target to newly connected browser
	ws.send(JSON.stringify({ type: 'SPOOF_CONFIG', target: spoofTarget }));

	ws.on('message', (rawMsg) => {
		try {
			const cmd = JSON.parse(rawMsg.toString());
			if (cmd.type === 'SET_SPOOF') {
				spoofTarget = (cmd.target || 'NONE').trim().toLowerCase();
				if (spoofTarget === 'none' || spoofTarget === '') spoofTarget = 'NONE';
				if (spoofTarget === 'any' || spoofTarget === 'all') spoofTarget = 'ANY';
				console.log(`\n[Bridge 🎯] Spoof target updated via UI -> ${spoofTarget}\n`);
				broadcastSpoofConfig();
			}
		} catch (err) {
			console.error('[Bridge] Invalid WS command:', err.message);
		}
	});
});

const kalmanProc = spawn(KALMAN_BIN, [], {
	env: { ...process.env, SPOOF_HEX: 'NONE' }
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

// Intercepts a raw readsb JSON line and injects a ~5km GPS jump if targeted
function feedLineToKalman(rawLine) {
	if (!rawLine || !rawLine.trim()) return;
	try {
		const obj = JSON.parse(rawLine);
		if (obj.hex && typeof obj.lat === 'number' && typeof obj.lon === 'number') {
			const hex = obj.hex.toLowerCase();
			const count = (pingCounts.get(hex) || 0) + 1;
			pingCounts.set(hex, count);

			const isTargeted =
				spoofTarget === 'ANY' ||
				(spoofTarget !== 'NONE' && hex === spoofTarget.toLowerCase());

			// Only spoof after the tracker has initialized (>= 3 pings)
			if (isTargeted && count >= 3) {
				obj.lat -= 0.045; // ~5000m South
				obj.lon += 0.070; // ~5000m East
				kalmanProc.stdin.write(JSON.stringify(obj) + '\n');
				return;
			}
		}
	} catch (e) {
		// If line isn't valid JSON, pass through as-is
	}
	kalmanProc.stdin.write(rawLine + '\n');
}

// Feed Data into kalman server std::cin
if (MODE === 'live') {
	function connectToReadsb() {
		console.log(`[Bridge] Connecting to live readsb at ${READSB_HOST}:${READSB_PORT}...`);
		const client = new net.Socket();
		let rlLive = null;

		client.connect(READSB_PORT, READSB_HOST, () => {
			console.log('[Bridge] Connected to readsb! Piping live ADS-B to C++ Kalman engine...');
			rlLive = readline.createInterface({ input: client });
			rlLive.on('line', (line) => feedLineToKalman(line));
			rlLive.on('error', () => {}); // Prevent unhandled readline error crashes
		});

		client.on('error', (err) => {
			console.error(`[Bridge] readsb TCP wait (${err.code || err.message}) - retrying in 2s...`);
		});

		client.on('close', () => {
			if (rlLive) rlLive.close();
			setTimeout(connectToReadsb, 2000);
		});
	}

	connectToReadsb();
} else {
	console.log(`[Bridge] Starting real-time timestamp-synced replay of ${SAMPLE_FILE}...`);
	const lines = fs.readFileSync(SAMPLE_FILE, 'utf-8').split('\n').filter(Boolean);
	let idx = 0;

	function sendNextPing() {
		if (idx >= lines.length) {
			console.log('[Bridge] Reached end of sample file.');
			return;
		}

		const currentLine = lines[idx];
		feedLineToKalman(currentLine);
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