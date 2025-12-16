import socket
import struct

# This matches the port in your pp-client.c
HOST = ''  # Listen on all interfaces
PORT = 20480

def main():
    print(f"Server listening on port {PORT}...")
    
    # Create a TCP socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        # Allow reusing the address to avoid "Address already in use" errors
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        s.bind((HOST, PORT))
        s.listen()
        
        while True:
            # Accept a new connection
            conn, addr = s.accept()
            with conn:
                print(f"Connected by {addr}")
                while True:
                    # Receive data (up to 1024 bytes)
                    data = conn.recv(1024)
                    if not data:
                        break
                    
                    # Decode to print what we received
                    try:
                        print(f"Received: {data.decode('utf-8')}")
                    except:
                        print(f"Received (raw): {data}")

                    # ECHO: Send the exact same data back to xv6
                    conn.sendall(data)
            print("Connection closed. Waiting for next...")

if __name__ == "__main__":
    main()