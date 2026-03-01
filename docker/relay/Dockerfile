FROM node:22-slim

WORKDIR /app

COPY extensions/src/multiplayer/server/package.json ./
RUN npm install

COPY extensions/src/multiplayer/server/relay.ts extensions/src/multiplayer/server/wrangler.toml ./

EXPOSE 8787

CMD ["npx", "wrangler", "dev", "--ip", "0.0.0.0", "--port", "8787"]
