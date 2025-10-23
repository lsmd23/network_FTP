import socket

size = 8192

try:
    for num in range(51):
        msg = str(num).encode()
        print("Sending: ", msg)
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(msg, ("localhost", 9876))
        print("received:", sock.recvfrom(size)[0].decode())
    sock.close()

except Exception as e:
    print("cannot reach the server:", e)
