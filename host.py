import socket
import threading
import json

SERVER_IP = "0.0.0.0" #IP address​

SERVER_PORT = 4040     #Port for message​

connected_clients = {}    #Recording connections​

lock = threading.Lock()   #Seperate each client


def broadcast_message(message, sender_socket=None):
    """
    廣播訊息給所有已連線的客戶端
    """
    with lock:
        for client_socket in connected_clients.values():
            if client_socket != sender_socket:  # 不回傳訊息給發送者
                try:
                    client_socket.send(message.encode('utf-8'))
                except Exception as e:
                    print(f"[ERROR] Unable to send message: {e}")


def handle_client(client_socket, client_address):
    """
    處理每個客戶端的通信
    """
    print(f"[NEW CONNECTION] {client_address} connected.")
    with lock:
        connected_clients[client_address] = client_socket

    try:
        while True:
            message = client_socket.recv(1024).decode('utf-8')
            if not message:
                break

            # 廣播接收到的訊息
            print(f"[MESSAGE FROM {client_address}]: {message}")
            broadcast_message(f"[{client_address}] {message}", sender_socket=client_socket)

    except Exception as e:
        print(f"[ERROR] {client_address}: {e}")
    finally:
        with lock:
            del connected_clients[client_address]
        client_socket.close()
        print(f"[DISCONNECT] {client_address} disconnected.")


def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((SERVER_IP, SERVER_PORT))
    server.listen(5)
    print(f"[STARTING] Server is listening on {SERVER_IP}:{SERVER_PORT}")

    while True:
        client_socket, client_address = server.accept()
        thread = threading.Thread(target=handle_client, args=(client_socket, client_address))
        thread.start()


if __name__ == "__main__":
    main()
