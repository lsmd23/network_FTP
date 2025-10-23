import socket

size = 8192

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("", 9876))

print("UDP server up and listening")
message_count = 0
try:
    while True:
        data, address = sock.recvfrom(size)
        data_string = data.decode()
        print("received:", data, "from", address, "with content: ", data_string)
        message_count += 1
        # print("Total messages processed:", message_count)
        data_to_be_sent = str(message_count) + " " + data_string
        sock.sendto(data_to_be_sent.encode(), address)
        print("sent:", data_to_be_sent, "to", address)
finally:
    sock.close()
