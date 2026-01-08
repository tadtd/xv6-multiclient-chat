#!/usr/bin/env python3
"""
Event-Driven Multiclient Chat Client for xv6

This client connects to the xv6 chat server and provides a multi-threaded
interface:
- Main thread: handles user input and sends messages
- Listener thread: continuously receives messages from server

Features:
- Automatic reconnection on disconnect
- Graceful handling of server shutdown
- Non-blocking message reception
- Thread-safe operations
"""

import socket
import threading
import sys
import time
import select
from datetime import datetime

# Connection settings - QEMU forwards this port to xv6
HOST = '127.0.0.1'
PORT = 56789  # Must match QEMU's hostfwd configuration

# Global state
running = True
connected = False
sock = None
sock_lock = threading.Lock()


class ChatClient:
    def __init__(self, host=HOST, port=PORT):
        self.host = host
        self.port = port
        self.sock = None
        self.connected = False
        self.running = True
        self.listener_thread = None
        self.sock_lock = threading.Lock()
        
    def connect(self):
        """Establish connection to the xv6 chat server."""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(10)  # 10 second timeout for connection
            self.sock.connect((self.host, self.port))
            self.sock.settimeout(None)  # Remove timeout after connection
            self.connected = True
            print(f"\n[{self._timestamp()}] Connected to xv6 chat server at {self.host}:{self.port}")
            return True
        except ConnectionRefusedError:
            print(f"\n[{self._timestamp()}] Connection refused. Is the xv6 server running?")
            return False
        except socket.timeout:
            print(f"\n[{self._timestamp()}] Connection timed out.")
            return False
        except Exception as e:
            print(f"\n[{self._timestamp()}] Connection error: {e}")
            return False
    
    def disconnect(self):
        """Safely disconnect from the server."""
        with self.sock_lock:
            self.connected = False
            if self.sock:
                try:
                    self.sock.shutdown(socket.SHUT_RDWR)
                except:
                    pass
                try:
                    self.sock.close()
                except:
                    pass
                self.sock = None
    
    def _timestamp(self):
        """Get current timestamp for logging."""
        return datetime.now().strftime("%H:%M:%S")
    
    def listener_func(self):
        """
        Listener Thread Function
        
        Continuously listens for incoming data from the server socket.
        Handles:
        - Normal message reception
        - Server disconnection detection
        - Error handling
        
        This runs in a separate thread to allow non-blocking message reception
        while the main thread handles user input.
        """
        buffer = ""
        
        while self.running and self.connected:
            try:
                # Use select for non-blocking check with timeout
                with self.sock_lock:
                    if not self.sock or not self.connected:
                        break
                    sock_fd = self.sock
                
                # Wait for data with 1 second timeout
                readable, _, exceptional = select.select([sock_fd], [], [sock_fd], 1.0)
                
                if exceptional:
                    print(f"\n[{self._timestamp()}] Socket error detected")
                    self._handle_disconnect("Socket error")
                    break
                
                if readable:
                    with self.sock_lock:
                        if not self.sock or not self.connected:
                            break
                        try:
                            data = self.sock.recv(4096)
                        except:
                            data = None
                    
                    if not data:
                        # Empty data = server closed connection
                        self._handle_disconnect("Server closed connection")
                        break
                    
                    # Decode and process received data
                    try:
                        message = data.decode('utf-8')
                        buffer += message
                        
                        # Process complete lines
                        while '\n' in buffer:
                            line, buffer = buffer.split('\n', 1)
                            if line.strip():
                                print(f"\r{line}")
                                # Re-display prompt
                                print("> ", end="", flush=True)
                    except UnicodeDecodeError:
                        print(f"\r[{self._timestamp()}] Received non-UTF8 data: {data}")
                        print("> ", end="", flush=True)
                        
            except socket.timeout:
                # Timeout is normal, continue loop
                continue
            except OSError as e:
                if self.running and self.connected:
                    self._handle_disconnect(f"Socket error: {e}")
                break
            except Exception as e:
                if self.running and self.connected:
                    print(f"\n[{self._timestamp()}] Listener error: {e}")
                    self._handle_disconnect(str(e))
                break
        
        print(f"\n[{self._timestamp()}] Listener thread stopped")
    
    def _handle_disconnect(self, reason):
        """Handle unexpected disconnection."""
        print(f"\n[{self._timestamp()}] Disconnected: {reason}")
        self.disconnect()
    
    def send_message(self, message):
        """Send a message to the server."""
        if not self.connected:
            print(f"[{self._timestamp()}] Not connected to server")
            return False
        
        try:
            with self.sock_lock:
                if self.sock:
                    # Ensure message ends with newline
                    if not message.endswith('\n'):
                        message += '\n'
                    self.sock.sendall(message.encode('utf-8'))
                    return True
        except BrokenPipeError:
            self._handle_disconnect("Broken pipe")
        except Exception as e:
            print(f"[{self._timestamp()}] Send error: {e}")
        
        return False
    
    def start_listener(self):
        """Start the listener thread."""
        self.listener_thread = threading.Thread(target=self.listener_func, daemon=True)
        self.listener_thread.start()
    
    def stop(self):
        """Stop the client."""
        self.running = False
        self.disconnect()
        if self.listener_thread and self.listener_thread.is_alive():
            self.listener_thread.join(timeout=2)
    
    def run(self):
        """Main client loop - handles user input."""
        print("=" * 50)
        print("   xv6 Multiclient Chat Client")
        print("=" * 50)
        print(f"Connecting to {self.host}:{self.port}...")
        print("Commands: /name <newname>, /list, /quit, /reconnect")
        print("-" * 50)
        
        if not self.connect():
            print("Failed to connect. Use /reconnect to try again.")
        else:
            self.start_listener()
        
        try:
            while self.running:
                try:
                    # Read user input
                    print("> ", end="", flush=True)
                    user_input = input()
                    
                    if not user_input:
                        continue
                    
                    # Handle local commands
                    if user_input.lower() == '/quit':
                        print("Goodbye!")
                        break
                    
                    if user_input.lower() == '/reconnect':
                        print("Attempting to reconnect...")
                        self.disconnect()
                        time.sleep(1)
                        if self.connect():
                            self.start_listener()
                        continue
                    
                    if user_input.lower() == '/help':
                        print("Commands:")
                        print("  /name <newname> - Change your nickname")
                        print("  /list           - List connected users")
                        print("  /quit           - Exit the chat")
                        print("  /reconnect      - Reconnect to server")
                        print("  /help           - Show this help")
                        continue
                    
                    # Send message to server
                    if not self.send_message(user_input):
                        print("Message not sent. Try /reconnect")
                        
                except EOFError:
                    # Ctrl+D pressed
                    print("\nGoodbye!")
                    break
                except KeyboardInterrupt:
                    # Ctrl+C pressed
                    print("\nInterrupted. Goodbye!")
                    break
                    
        finally:
            self.stop()


def main():
    """Entry point for the chat client."""
    # Parse command line arguments
    host = HOST
    port = PORT
    
    if len(sys.argv) >= 2:
        host = sys.argv[1]
    if len(sys.argv) >= 3:
        try:
            port = int(sys.argv[2])
        except ValueError:
            print(f"Invalid port: {sys.argv[2]}")
            sys.exit(1)
    
    # Create and run client
    client = ChatClient(host, port)
    client.run()


if __name__ == "__main__":
    main()
