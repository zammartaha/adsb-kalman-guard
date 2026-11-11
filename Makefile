.PHONY: all build replay live-host live-sdr prod-sdr prod-replay logs down clean

all: build

build:
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j$$(nproc)

replay:
	STREAM_MODE=replay docker compose up --build

live-host:
	STREAM_MODE=live READSB_HOST=host.docker.internal docker compose up --build

live-sdr:
	STREAM_MODE=live READSB_HOST=readsb docker compose --profile sdr up --build

prod-sdr:
	STREAM_MODE=live READSB_HOST=readsb docker compose --profile sdr --profile prod up -d --build

logs:
	docker compose --profile sdr --profile prod logs -f --tail 50

down:
	docker compose --profile sdr --profile prod down

clean:
	rm -rf build
