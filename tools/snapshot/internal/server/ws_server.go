package server

import (
	"log"
	"net/http"

	"github.com/gorilla/websocket"
)

type WebSocketServer struct {
	serverName string
	callback   func(message string) string
	upgrader   websocket.Upgrader
}

func NewWebSocketServer(serverName string, callback func(message string) string) *WebSocketServer {
	return &WebSocketServer{
		serverName: serverName,
		callback:   callback,
		upgrader: websocket.Upgrader{
			CheckOrigin: func(r *http.Request) bool { return true }, // Allow all connections
		},
	}
}

func (ws *WebSocketServer) handleConnections() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		conn, err := ws.upgrader.Upgrade(w, r, nil)
		if err != nil {
			log.Printf("[%s] Error upgrading to WebSocket: %v", ws.serverName, err)
			return
		}
		defer conn.Close()

		log.Printf("[%s] New WebSocket connection established", ws.serverName)

		for {
			_, msg, err := conn.ReadMessage()
			if err != nil {
				log.Printf("[%s] Error reading message: %v", ws.serverName, err)
				break
			}
			log.Printf("[%s] Received: %s", ws.serverName, msg)

			response := ws.callback(string(msg))

			err = conn.WriteMessage(websocket.TextMessage, []byte(response))
			log.Printf("[%s] Sending: %s", ws.serverName, response)
			if err != nil {
				log.Printf("[%s] Error writing message: %v", ws.serverName, err)
				break
			}
		}
	}
}

func (ws *WebSocketServer) Start(address string) {
	http.HandleFunc("/", ws.handleConnections())
	log.Printf("[%s] Starting ws server on address: %s", ws.serverName, address)
	err := http.ListenAndServe(address, nil)
	if err != nil {
		log.Fatalf("[%s] Server failed: %v", ws.serverName, err)
	}
}
