import socket
import threading


def receive_messages(client_socket):
    """
    持續接收來自伺服器的訊息
    """
    while True:
        try:
            message = client_socket.recv(1024).decode('utf-8')
            if message:
                print(message)
            else:
                print("[INFO]: Disconnected from server.")
                break
        except Exception as e:
            print(f"[ERROR]: {e}")
            break


def main():
    SERVER_IP = input("Enter server IP: ") or "127.0.0.1"
    SERVER_PORT = int(input("Enter server port: ") or 4040)

    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        client_socket.connect((SERVER_IP, SERVER_PORT))
        print(f"[CONNECTED]: Connected to server {SERVER_IP}:{SERVER_PORT}")

        # 啟動接收訊息的線程
        threading.Thread(target=receive_messages, args=(client_socket,), daemon=True).start()

        while True:
            message = input()
            if message.lower() == "exit":
                print("[INFO]: Exiting chat.")
                break
            client_socket.send(message.encode('utf-8'))

    except Exception as e:
        print(f"[ERROR]: Unable to connect to server. {e}")
    finally:
        client_socket.close()


if __name__ == "__main__":
    main()
