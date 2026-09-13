#!/usr/bin/env python3
"""Subprocess/discovery helpers shared by the differential harness oracles."""

import os
import signal
import shutil
import subprocess


class EngineMissing(Exception):
    """A configured reference engine is not installed on PATH."""


class OracleTimeout(Exception):
    """An oracle did not answer within its deadline."""


def which(name):
    """Resolve an executable on PATH, raising EngineMissing if absent."""
    path = shutil.which(name)
    if not path:
        raise EngineMissing("required engine '%s' not found on PATH" % name)
    return path


def run_argv(argv, timeout):
    """Run a subprocess, returning (returncode, stdout, stderr).

    Raises OracleTimeout when `timeout` (seconds) elapses, killing the whole
    process group so no orphan runtime survives."""
    try:
        proc = subprocess.run(argv, capture_output=True, text=True,
                              timeout=timeout, start_new_session=True)
    except subprocess.TimeoutExpired as exc:
        pid = getattr(exc, 'pid', None)
        if pid is not None:
            try:
                os.killpg(pid, signal.SIGKILL)
            except (OSError, ProcessLookupError):
                pass
        raise OracleTimeout("timed out after %ss" % timeout)
    return proc.returncode, proc.stdout, proc.stderr


def run_stdin(argv, data, timeout):
    """Run a subprocess fed `data` on stdin; returns (rc, stdout, stderr).

    Raises OracleTimeout on expiry (killing the process group)."""
    try:
        proc = subprocess.run(argv, input=data, capture_output=True, text=True,
                              timeout=timeout, start_new_session=True)
    except subprocess.TimeoutExpired as exc:
        pid = getattr(exc, 'pid', None)
        if pid is not None:
            try:
                os.killpg(pid, signal.SIGKILL)
            except (OSError, ProcessLookupError):
                pass
        raise OracleTimeout("timed out after %ss" % timeout)
    return proc.returncode, proc.stdout, proc.stderr
