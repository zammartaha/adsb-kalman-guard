# ADS-B Kalman Guard

**Real-Time 6D Kalman Trajectory Engine, GNSS Spoofing Detector, and 3D WebGL Radar**

`adsb-kalman-guard` is a real-time civil aviation telemetry pipeline and GNSS/ADS-B spoofing detection system. It ingests live 1090 MHz Mode-S / ADS-B radio frames (via `readsb` and an RTL-SDR receiver) or timestamped recordings, projects aircraft coordinates into a local tangent East-North-Up (ENU) Cartesian frame, tracks 6-DOF kinematics with a Linear Kalman Filter, and rejects physically impossible GPS jumps using a Mahalanobis distance ($\chi^2$) innovation gate.

When an anomalous position jump is detected, the C++ engine drops the corrupted measurement and dead-reckons (coasts) the aircraft along its current velocity vector. Both the filtered trajectory and the rejected phantom spoofing vector are streamed over WebSockets to a 3D React + Deck.gl radar interface.

## Core Features

* **C++17 6D Kalman Engine:** Tracks 3D position and 3D velocity $\mathbf{x} = [x, y, z, v_x, v_y, v_z]^T$ per aircraft ICAO hex without external linear algebra libraries. Includes automatic pruning for stale tracks.
* **WGS84 to ENU Projection:** Converts spherical GPS coordinates $(\text{lat}, \text{lon}, \text{alt})$ into metric Cartesian coordinates $(E, N, U)$ relative to the receiver antenna origin so kinematic filtering runs in meters and meters per second.
* **Mahalanobis Innovation Gating:** Calculates the squared Mahalanobis distance $d^2 = \mathbf{y}^T \mathbf{S}^{-1} \mathbf{y}$ on every incoming ping. Pings that exceed the 3-DOF $\chi^2$ threshold ($d^2 > 200.0$) are flagged as `KINEMATIC_SPOOF_JUMP` and blocked from corrupting the filter state.
* **Live Spoof Injection Proxy:** The Node.js WebSocket bridge acts as a red-team proxy, allowing you to inject $\sim 5\text{ km}$ lateral GPS offsets into a specific aircraft or the entire airspace directly from the browser UI to test filter coasting and covariance growth.
* **3D WebGL Radar & Telemetry Inspector:**
  * 3D aircraft icons rotated along their live Kalman velocity vector $(v_x, v_y)$.
  * Real-time covariance uncertainty halos ($\pm\text{m}$) that expand while dead-reckoning and contract when valid measurements resume.
  * Dual 3D flight paths showing the filtered trajectory alongside diverging red spoofed paths.
  * Interactive telemetry drawer displaying ground speed, vertical rate ($v_z$), live Mahalanobis gate score, recent innovation history, camera follow lock, and dark/light map themes.
* **Containerized Deployment:** Supports historical file replay, host `readsb` networking, and direct RTL-SDR USB passthrough via Docker Compose profiles.

## System Architecture

```text
+-------------------------------------------------------------------+
| 1090 MHz RF Antenna -> RTL-SDR v4 -> readsb (TCP NDJSON :30047)   |
|                     OR sample_data/*.ndjson (1:1 Timestamp Replay)|
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
| Node.js WebSocket Bridge (bridge/server.js)                       |
|  - Ingests live TCP stream or schedules real-time file replay     |
|  - Red-Team Proxy: Injects dynamic ~5km GPS spoofing jumps on cmd |
|  - Pipes raw NDJSON to C++ Engine via stdin / reads via stdout    |
+-------------------------------------------------------------------+
                        | stdin             ^ stdout
                        v                   |
+-------------------------------------------------------------------+
| C++17 Kalman Trajectory Engine (backend/ -> kalman_server)        |
|  1. Parser: Extracts ICAO hex, callsign, WGS84 pos, timestamp     |
|  2. ENU Projector: WGS84 (deg) -> Local East-North-Up (meters)    |
|  3. Fleet Manager: Routes to per-aircraft 6D Kalman Tracker       |
|  4. Predict & Mahalanobis Gate: Rejects spoofing / updates state  |
+-------------------------------------------------------------------+
                                  |
                                  v WebSocket (ws://:8080)
+-------------------------------------------------------------------+
| React + Deck.gl 3D Radar Frontend (frontend/ -> :5173)            |
|  - Renders 3D trajectories, covariance halos & spoof vectors      |
|  - Telemetry Inspector Drawer & Spoof Controls                    |
+-------------------------------------------------------------------+
```

## Mathematical Overview

### 1. State Vector & Constant-Velocity Model
Each aircraft tracker maintains a 6-state vector $\mathbf{x}_k$ and a $6 \times 6$ covariance matrix $\mathbf{P}_k$ in the local ENU frame (meters and $\text{m/s}$):

$$\mathbf{x}_k = \begin{bmatrix} x & y & z & v_x & v_y & v_z \end{bmatrix}^T$$

For a time step $\Delta t = t_k - t_{k-1}$ between consecutive ADS-B pings, the prediction step projects the state and covariance forward using the state transition matrix $\mathbf{F}$ and process noise $\mathbf{Q}(\Delta t)$:

$$\hat{\mathbf{x}}_{k\vert{}k-1} = \mathbf{F} \mathbf{x}_{k-1}, \quad \mathbf{P}_{k\vert{}k-1} = \mathbf{F} \mathbf{P}_{k-1} \mathbf{F}^T + \mathbf{Q}(\Delta t)$$

$$\mathbf{F} = \begin{bmatrix} 1 & 0 & 0 & \Delta t & 0 & 0 \\ 0 & 1 & 0 & 0 & \Delta t & 0 \\ 0 & 0 & 1 & 0 & 0 & \Delta t \\ 0 & 0 & 0 & 1 & 0 & 0 \\ 0 & 0 & 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 0 & 0 & 1 \end{bmatrix}$$

### 2. Innovation & Mahalanobis Gate
Given an incoming ENU position measurement $\mathbf{z}_k = [x_m, y_m, z_m]^T$, observation matrix $\mathbf{H} = [\mathbf{I}_{3\times3} \mid \mathbf{0}_{3\times3}]$, and measurement noise covariance $\mathbf{R}$, the innovation residual $\mathbf{y}_k$ and innovation covariance $\mathbf{S}_k$ are:

$$\mathbf{y}_k = \mathbf{z}_k - \mathbf{H} \hat{\mathbf{x}}_{k\vert{}k-1}, \quad \mathbf{S}_k = \mathbf{H} \mathbf{P}_{k\vert{}k-1} \mathbf{H}^T + \mathbf{R}$$

Before applying the measurement update, the tracker computes the squared Mahalanobis distance:

$$d^2 = \mathbf{y}_k^T \mathbf{S}_k^{-1} \mathbf{y}_k$$

* **If $d^2 > 200.0$:** The measurement is rejected as an anomaly (`is_anomalous = true`). The tracker skips the Kalman gain update and coasts on the predicted state $\hat{\mathbf{x}}_{k\vert{}k-1}$ while $\mathbf{P}_{k\vert{}k-1}$ grows to reflect increasing position uncertainty.
* **If $d^2 \le 200.0$:** The Kalman gain $\mathbf{K}_k = \mathbf{P}_{k\vert{}k-1} \mathbf{H}^T \mathbf{S}_k^{-1}$ is applied to update both the state vector and covariance matrix.

## Configuration (`.env`)

Copy `template.env` to `.env` before building or running the stack:

```bash
cp template.env .env
```

Edit `.env` to set your reference antenna coordinates and stream mode:

```dotenv
# Reference Antenna Origin (WGS84 Decimal Degrees & Altitude in Meters)
# Default in template.env is set to Brussels Airport (EBBR)
REF_LAT=50.9014
REF_LON=4.4844
REF_ALT_M=56.0

# Telemetry Stream Mode ('replay' or 'live')
STREAM_MODE=replay
SAMPLE_PATH=sample_data/brussels_live.ndjson

# Live readsb TCP JSON Source
READSB_HOST=127.0.0.1
READSB_PORT=30047

# WebSocket Bridge Port & Initial Spoof Target ('NONE', 'ANY', or ICAO hex)
WS_PORT=8080
SPOOF_HEX=NONE
```

## Running with Docker Compose

### 1. Replay Mode (No Hardware Required)
Streams `sample_data/brussels_live.ndjson` synced to the original recording timestamps:

```bash
make replay
# Or directly: STREAM_MODE=replay docker compose up --build
```

Open `http://localhost:5173` in your browser.

### 2. Containerized Live RTL-SDR Mode (`--profile sdr`)
Passes a USB RTL-SDR dongle directly into the `readsb` container (`docker-adsb-ultrafeeder`), decodes live 1090 MHz frames, and streams them into the Kalman backend:

```bash
make live-sdr
# Or directly: STREAM_MODE=live READSB_HOST=readsb docker compose --profile sdr up --build
```

*Note for WSL2 users: Attach the USB device to WSL2 first via PowerShell (`usbipd attach --wsl --busid <BUSID>`).*

### 3. Live Mode via Host `readsb`
If `readsb` is already running natively on your host machine with `--net-json-port 30047`:

```bash
# Terminal 1 (Host):
readsb --device-type rtlsdr --net --net-json-port 30047 --quiet

# Terminal 2 (Docker stack):
make live-host
```

To stop all running containers:

```bash
make down
```

## Manual Local Build (Without Docker)

### 1. Compile the C++17 Engine
Requires `cmake` ($\ge 3.14$) and a C++17 compiler (`g++` or `clang++`):

```bash
make build
```

This outputs the binary to `./build/kalman_server`. You can test the engine standalone by piping an NDJSON capture directly into `stdin`:

```bash
./build/kalman_server < sample_data/brussels_live.ndjson
```

### 2. Run the Node.js WebSocket Bridge
```bash
cd bridge
npm install
node server.js
```

### 3. Run the Vite Frontend
```bash
cd frontend
npm install
npm run dev
```

Open `http://localhost:5173`.

## Using the Spoof Injector and Telemetry Inspector

1. **Inspect an Aircraft:** Click any aircraft on the map (or select one from the dropdown in the top-left panel) to open the telemetry inspector in the bottom-right corner.
2. **Camera Lock:** Click **Follow Aircraft** in the inspector panel to lock the 3D camera onto the selected plane. Dragging the map automatically releases the camera lock.
3. **Spoof a Single Aircraft:** Click **Spoof This Plane** in the inspector (or type an ICAO hex into the top-left panel and click **Spoof Hex**).
   * The Node.js bridge offsets incoming GPS coordinates for that aircraft by roughly 5 km.
   * The Mahalanobis $d^2$ meter spikes past the `200.0` threshold and rejects the spoofed pings.
   * On the map, a red displacement line and red trajectory show the rejected coordinates while the filtered aircraft icon coasts along its predicted path and its uncertainty halo grows.
4. **Spoof Entire Airspace:** Click **Spoof ALL** to inject spoofing offsets across all tracked aircraft simultaneously, or **Stop Spoof** to return to normal tracking.