FROM node:22-slim

WORKDIR /app

COPY extensions/src/multiplayer/server/package.json ./
RUN npm install

COPY extensions/src/multiplayer/server/*.ts extensions/src/multiplayer/server/wrangler.toml ./

EXPOSE 8787

# Run wrangler directly as PID 1 so it receives SIGINT (Ctrl+C)
# and shuts down gracefully. Using npx as PID 1 swallows signals.
CMD ["node_modules/.bin/wrangler", "dev", "--ip", "0.0.0.0", "--port", "8787"]
