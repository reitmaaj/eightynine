#!/usr/bin/env python3
"""Scriptable mock HTTP server for manually exercising llm89 (or any client).

Drive it with a JSON config describing routes. Each route may set a status, a
content type, a body (inline or from a file), an optional delay, and optional
chunked transfer. The request method/path/headers/body are printed to stderr so
you can see exactly what the client sent.

Config file format:
{
  "port": 18989,
  "routes": [
    {"path": "/ok",
     "status": 200,
     "content_type": "application/json",
     "body": "{\\"choices\\":[{\\"message\\":{\\"content\\":\\"hello\\"}}]}"},
    {"path": "/stream",
     "status": 200,
     "content_type": "text/event-stream",
     "chunked": true,
     "body": "data: {\\"choices\\":[{\\"delta\\":{\\"content\\":\\"hi\\"}}]}\\n\\ndata: [DONE]\\n\\n"},
    {"path": "/slow", "status": 200, "delay_ms": 5000, "body": "{}"}
  ]
}

Usage: mock_server.py <config.json>
"""
import json
import socket
import sys
import threading
import time


def load_config(path):
    with open(path, 'r') as f:
        return json.load(f)


def read_request(conn):
    data = b''
    while b'\r\n\r\n' not in data:
        chunk = conn.recv(8192)
        if not chunk:
            break
        data += chunk
    header_text, _, rest = data.partition(b'\r\n\r\n')
    lines = header_text.split(b'\r\n')
    request_line = lines[0].decode('latin-1')
    headers = {}
    for line in lines[1:]:
        if b':' in line:
            name, _, value = line.partition(b':')
            headers[name.strip().decode('latin-1').lower()] = \
                value.strip().decode('latin-1')
    length = int(headers.get('content-length', '0'))
    body = rest
    while len(body) < length:
        chunk = conn.recv(8192)
        if not chunk:
            break
        body += chunk
    return request_line, headers, body[:length]


def respond(conn, status, content_type, body, chunked):
    reason = 'OK' if status < 400 else 'Error'
    conn.sendall(('HTTP/1.1 %d %s\r\n' % (status, reason)).encode('latin-1'))
    conn.sendall(('Content-Type: ' + content_type + '\r\n').encode('latin-1'))
    if chunked:
        conn.sendall(b'Transfer-Encoding: chunked\r\n')
    else:
        conn.sendall(('Content-Length: ' + str(len(body)) + '\r\n').encode())
    conn.sendall(b'\r\n')
    if chunked:
        i = 0
        while i < len(body):
            piece = body[i:i + 3]
            conn.sendall(('%x\r\n' % len(piece)).encode())
            conn.sendall(piece)
            conn.sendall(b'\r\n')
            i += 3
        conn.sendall(b'0\r\n\r\n')
    else:
        conn.sendall(body)


def match_route(routes, path):
    for route in routes:
        if route.get('path') == path:
            return route
    return None


def handle(conn, route):
    status = int(route.get('status', 200))
    content_type = route.get('content_type', 'application/json')
    body = route.get('body', '')
    chunked = bool(route.get('chunked', False))
    body_file = route.get('body_file')
    if body_file is not None:
        with open(body_file, 'rb') as f:
            body = f.read()
    else:
        body = body.encode('utf-8')
    delay = int(route.get('delay_ms', 0))
    if delay > 0:
        time.sleep(delay / 1000.0)
    respond(conn, status, content_type, body, chunked)


def serve_conn(conn, routes):
    try:
        request_line, headers, body = read_request(conn)
        parts = request_line.split(' ')
        method = parts[0] if len(parts) > 0 else ''
        path = parts[1] if len(parts) > 1 else '/'
        sys.stderr.write('>>> %s %s\n' % (method, path))
        for name, value in headers.items():
            sys.stderr.write('    %s: %s\n' % (name, value))
        if body:
            sys.stderr.write('    body: %r\n' % body)
        route = match_route(routes, path)
        if route is None:
            respond(conn, 404, 'text/plain', b'no such route', False)
        else:
            handle(conn, route)
    except Exception as exc:
        sys.stderr.write('mock_server: %r\n' % exc)
    finally:
        try:
            conn.close()
        except Exception:
            pass


def main():
    if len(sys.argv) < 2:
        sys.stderr.write('usage: mock_server.py <config.json>\n')
        return 2
    config = load_config(sys.argv[1])
    port = int(config.get('port', 18989))
    routes = config.get('routes', [])
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('127.0.0.1', port))
    s.listen(16)
    sys.stderr.write('mock_server: listening on 127.0.0.1:%d\n' % port)
    while True:
        conn, _ = s.accept()
        t = threading.Thread(target=serve_conn, args=(conn, routes))
        t.daemon = True
        t.start()


if __name__ == '__main__':
    sys.exit(main())
