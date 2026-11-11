# ==========================================
# Stage 1: Build React + Deck.gl Static Bundle
# ==========================================
FROM node:22-bookworm-slim AS web-builder

WORKDIR /app/frontend

COPY frontend/package*.json ./
RUN npm ci

COPY frontend/ ./

# Pass reference antenna coordinates into Vite at build time
ARG REF_LAT=0.0
ARG REF_LON=0.0
ARG REF_ALT_M=0.0
ENV REF_LAT=$REF_LAT \
    REF_LON=$REF_LON \
    REF_ALT_M=$REF_ALT_M

RUN npm run build

# ==========================================
# Stage 2: Serve with Lightweight Nginx
# ==========================================
FROM nginx:alpine
COPY --from=web-builder /app/frontend/dist /usr/share/nginx/html
COPY frontend/nginx.conf /etc/nginx/conf.d/default.conf
EXPOSE 80
CMD ["nginx", "-g", "daemon off;"]