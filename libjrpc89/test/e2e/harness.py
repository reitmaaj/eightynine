"""Shared e2e harness: spawn the libjrpc89 CLI over a Unix socketpair.

The CLI takes an already-open Unix socket fd and does not connect. The
harness therefore creates a socketpair, hands one end to the CLI as its fd
(via pass_fds), and plays the server on the other end.
"""

import socket
import subprocess


def spawn_cli(cli, method, params=None):
    """Start the CLI on one end of a socketpair.

    Returns (proc, server_sock). The caller plays server on server_sock and
    then calls finish() to reap the CLI.
    """
    client_end, server_end = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    args = [cli, str(client_end.fileno()), method]
    if params is not None:
        args.append(params)
    proc = subprocess.Popen(
        args,
        pass_fds=[client_end.fileno()],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    client_end.close()
    return proc, server_end


def read_frame(sock):
    """Read one newline-delimited frame; return bytes without the newline."""
    data = b""
    while b"\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            break
        data = data + chunk
    return data.split(b"\n", 1)[0]


def send_bytes(sock, raw):
    """Send raw bytes and leave the socket open (finish() closes it)."""
    sock.sendall(raw)


def send_json(sock, obj):
    """Send an object as one newline-terminated JSON frame."""
    send_bytes(sock, (json_dumps(obj) + "\n").encode("utf-8"))


def json_dumps(obj):
    import json

    return json.dumps(obj, separators=(",", ":"), ensure_ascii=True)


def finish(proc, sock):
    """Close the server socket (signals EOF if still open) and reap the CLI.

    Returns (stdout, stderr, returncode).
    """
    try:
        sock.close()
    except OSError:
        pass
    out, err = proc.communicate(timeout=10)
    return out, err, proc.returncode
