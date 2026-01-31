#!/bin/sh
set -e

echo "Setting up LEGO Island assets..."
npm run prepare:assets -- -f -p /assets

echo "Starting development server..."
exec npm run dev -- --host 0.0.0.0 --port 6931
