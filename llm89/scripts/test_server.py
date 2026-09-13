#!/usr/bin/env python3
"""Deterministic local HTTP test server for llm89 protocol tests.

Path-driven responses over POST to 127.0.0.1:<port>:
  /ok            200 JSON completion
  /stream        200 SSE completion with [DONE]
  /stream_chunked same, written 1-2 bytes at a time
  /stream_nodone 200 SSE without [DONE]
  /stream_badjson 200 SSE whose first event is not JSON, then [DONE]
  /stream_slow   SSE with sleeps between events (for cancellation)
  /slow          sleeps, then 200 (for total timeout)
  /err/400 /401 /429 /500  matching status with JSON error.message
  /err_nonjson  500 with a non-JSON body (message fallback)
  /proto_err     200 JSON with empty choices
  /badjson       200 with a syntactically invalid JSON body
  /echo_hdr      verifies X-Test header == "hello"
  /noauth        verifies no Authorization header
  /big           200 body larger than the response limit
  /sse_big       SSE event larger than the event limit, then [DONE]
"""
import socket
import sys
import threading
import time

PORT = int(sys.argv[1])

OK_JSON = b'{"choices":[{"message":{"content":"hello"}}]}'
STREAM_EV1 = b'{"choices":[{"delta":{"content":"Hello "}}]}'
STREAM_EV2 = b'{"choices":[{"delta":{"content":"world"}}]}'
BIG_EVENT_PAYLOAD = b'{"choices":[{"delta":{"content":"' + (b'x' * 400) + b'"}}]}'


def read_headers(conn):
    data = b''
    while b'\r\n\r\n' not in data:
        chunk = conn.recv(4096)
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
    content_length = int(headers.get('content-length', '0'))
    body = rest
    while len(body) < content_length:
        chunk = conn.recv(4096)
        if not chunk:
            break
        body += chunk
    return request_line, headers, body[:content_length]


def respond(conn, status_line, content_type, body, chunked=False):
    conn.sendall(('HTTP/1.1 ' + status_line + '\r\n').encode('latin-1'))
    conn.sendall(('Content-Type: ' + content_type + '\r\n').encode('latin-1'))
    if chunked:
        conn.sendall(b'Transfer-Encoding: chunked\r\n')
    else:
        conn.sendall(('Content-Length: ' + str(len(body)) + '\r\n').encode())
    conn.sendall(b'\r\n')
    if chunked:
        i = 0
        while i < len(body):
            n = (i % 2) + 1
            piece = body[i:i + n]
            conn.sendall(('%x\r\n' % len(piece)).encode())
            conn.sendall(piece)
            conn.sendall(b'\r\n')
            i += n
        conn.sendall(b'0\r\n\r\n')
    else:
        conn.sendall(body)


def handle(conn, path, headers):
    if path == '/ok':
        respond(conn, '200 OK', 'application/json', OK_JSON)
    elif path == '/stream':
        body = (b'data: ' + STREAM_EV1 + b'\n\ndata: ' + STREAM_EV2 +
                b'\n\ndata: [DONE]\n\n')
        respond(conn, '200 OK', 'text/event-stream', body)
    elif path == '/stream_chunked':
        body = (b'data: ' + STREAM_EV1 + b'\n\ndata: ' + STREAM_EV2 +
                b'\n\ndata: [DONE]\n\n')
        respond(conn, '200 OK', 'text/event-stream', body, chunked=True)
    elif path == '/stream_nodone':
        body = (b'data: ' + STREAM_EV1 + b'\n\ndata: ' + STREAM_EV2 + b'\n\n')
        respond(conn, '200 OK', 'text/event-stream', body)
    elif path == '/stream_badjson':
        body = b'data: not-json\n\ndata: [DONE]\n\n'
        respond(conn, '200 OK', 'text/event-stream', body)
    elif path == '/stream_slow':
        body = (b'data: ' + STREAM_EV1 + b'\n\n')
        respond(conn, '200 OK', 'text/event-stream', body)
        time.sleep(5)
    elif path == '/slow':
        time.sleep(5)
        respond(conn, '200 OK', 'application/json', OK_JSON)
    elif path.startswith('/err/'):
        code = path.split('/')[-1]
        body = ('{"error":{"message":"%s"}}' % code).encode('latin-1')
        status = {'400': '400 Bad Request', '401': '401 Unauthorized',
                  '429': '429 Too Many Requests',
                  '500': '500 Internal Server Error'}.get(code, '500')
        respond(conn, status, 'application/json', body)
    elif path == '/err_nonjson':
        respond(conn, '500 Internal Server Error', 'text/plain', b'oops')
    elif path == '/err_msg':
        respond(conn, '400 Bad Request', 'application/json',
                b'{"error":{"message":"the server said no"}}')
    elif path == '/proto_err':
        respond(conn, '200 OK', 'application/json', b'{"choices":[]}')
    elif path == '/badjson':
        respond(conn, '200 OK', 'application/json', b'{not valid json')
    elif path == '/echo_hdr':
        if headers.get('x-test') == 'hello':
            respond(conn, '200 OK', 'application/json', OK_JSON)
        else:
            respond(conn, '400 Bad Request', 'application/json',
                    b'{"error":{"message":"missing header"}}')
    elif path == '/noauth':
        if 'authorization' in headers:
            respond(conn, '400 Bad Request', 'application/json',
                    b'{"error":{"message":"auth present"}}')
        else:
            respond(conn, '200 OK', 'application/json', OK_JSON)
    elif path == '/big':
        respond(conn, '200 OK', 'application/json', b'x' * 5000)
    elif path == '/sse_big':
        body = b'data: ' + BIG_EVENT_PAYLOAD + b'\n\ndata: [DONE]\n\n'
        respond(conn, '200 OK', 'text/event-stream', body)
    else:
        respond(conn, '404 Not Found', 'text/plain', b'no such path')


def serve_conn(conn):
    try:
        request_line, headers, body = read_headers(conn)
        parts = request_line.split(' ')
        path = parts[1] if len(parts) > 1 else '/'
        handle(conn, path, headers)
    except Exception:
        pass
    finally:
        try:
            conn.close()
        except Exception:
            pass


def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('127.0.0.1', PORT))
    s.listen(16)
    while True:
        conn, _ = s.accept()
        t = threading.Thread(target=serve_conn, args=(conn,))
        t.daemon = True
        t.start()


if __name__ == '__main__':
    main()
