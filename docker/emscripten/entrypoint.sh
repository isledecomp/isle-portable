#!/bin/sh

set -e

echo "Starting nginx web server on port 6931..."
exec nginx -c /etc/nginx/nginx.conf
