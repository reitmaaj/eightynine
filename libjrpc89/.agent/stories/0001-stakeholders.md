# Stakeholders

## 0000 library consumer

AS a C program that must call a remote JSON-RPC 2.0 service over a Unix socket
I WANT a library that builds a request, sends it on my already-open fd, and
parses the response into a result or a structured error
SO THAT I do not hand-assemble JSON-RPC objects or re-implement the protocol.

## 0001 CLI user

AS an operator debugging a JSON-RPC 2.0 Unix-socket service
I WANT to run a command that connects, sends a method call, and prints the
result or error
SO THAT I can probe a service end to end without writing a program.

## 0002 reviewer

AS a code reviewer
I WANT every message shape and validation decision to be explicit and unit
tested
SO THAT protocol correctness is provable rather than assumed.

## 0003 profile maintainer

AS a maintainer of the strict C89 ∩ C23 `green` profile
I WANT a client whose every translation unit passes the seven-check matrix
SO THAT the profile is demonstrably applicable to real network-facing code.
