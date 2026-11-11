.PHONY: all build replay live-host live-sdr down clean

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

down:
	docker compose --profile sdr down

clean:
	rm -rf build
