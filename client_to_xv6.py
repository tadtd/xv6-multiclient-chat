import socket

# WE CONNECT TO LOCALHOST because QEMU is forwarding the port
HOST = '127.0.0.1'  
PORT = 56789        

def start_client():
  print(f"Attempting to connect to xv6 server at {HOST}:{PORT}...")
  
  try:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
      s.connect((HOST, PORT))
      print("Connected!")
      
      # 1. Send Data
      message = "Hello xv6!"
      print(f"Sending: {message}")
      s.sendall(message.encode())
      
      # 2. Receive Reply
      data = s.recv(1024)
      print(f"Received from xv6: {data.decode()}")
          
  except ConnectionRefusedError:
    print("Error: Connection Refused.")
    print("Check: Is xv6 running? Is hostfwd configured in Makefile?")
  except Exception as e:
    print(f"An error occurred: {e}")

if __name__ == "__main__":
  start_client()