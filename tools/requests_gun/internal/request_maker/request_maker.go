package request_maker

import (
	"encoding/json"
	"errors"
	"fmt"
	"github.com/gorilla/websocket"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"
)

type RequestMaker interface {
	MakeRequest(request string) (*ResponseData, error)
}

type WebSocketClient struct {
	conn *websocket.Conn
}

type HttpRequestMaker struct {
	host      string
	transport *http.Transport
	client    *http.Client
}

type JsonMap map[string]interface{}
type StatusCode int

type ResponseData struct {
	Body       JsonMap
	StatusCode StatusCode
	StatusStr  string
	Duration   time.Duration
}

func (h *HttpRequestMaker) MakeRequest(request string) (*ResponseData, error) {
	startTime := time.Now()
	req, err := http.NewRequest("POST", h.host, strings.NewReader(request))
	if err != nil {
		return nil, errors.New("Error creating request: " + err.Error())
	}

	response, err := h.client.Do(req)
	requestDuration := time.Since(startTime)

	if err != nil {
		return nil, errors.New("Error making request: " + err.Error())
	}

	body, err := io.ReadAll(response.Body)
	response.Body.Close()
	if err != nil {
		return nil, errors.New("Error reading response body: " + err.Error())
	}

	if response.StatusCode != 200 {
		return &ResponseData{StatusCode: StatusCode(response.StatusCode), StatusStr: response.Status + ": " + string(body)}, nil
	}

	var bodyParsed JsonMap
	err = json.Unmarshal(body, &bodyParsed)
	if err != nil {
		return nil, errors.New("Error parsing response '" + string(body) + "': " + err.Error())
	}

	return &ResponseData{bodyParsed, StatusCode(response.StatusCode), response.Status, requestDuration}, nil
}

func NewHttp(host string, port uint) *HttpRequestMaker {
	if !strings.HasPrefix(host, "http://") && !strings.HasPrefix(host, "https://") {
		host = "http://" + host
	}
	transport := http.DefaultTransport.(*http.Transport).Clone()
	client := &http.Client{Transport: transport}

	return &HttpRequestMaker{host + ":" + fmt.Sprintf("%d", port), transport, client}
}

func NewWebSocketClient(host string, port uint) (*WebSocketClient, error) {
	var u url.URL
	if !strings.HasPrefix(host, "ws://") && !strings.HasPrefix(host, "wss://") {
		u = url.URL{Scheme: "ws", Host: host + ":" + fmt.Sprintf("%d", port), Path: "/"}
	} else {
		u = url.URL{Host: host + ":" + fmt.Sprintf("%d", port), Path: "/"}
	}
	conn, _, err := websocket.DefaultDialer.Dial(u.String(), nil)
	if err != nil {
		return nil, errors.New("Error connecting to WebSocket: " + err.Error())
	}
	return &WebSocketClient{conn: conn}, nil
}

// SendMessage sends a message to the WebSocket server
func (ws *WebSocketClient) SendMessage(message string) (*ResponseData, error) {
	defer ws.conn.Close()
	start := time.Now()
	err := ws.conn.WriteMessage(websocket.TextMessage, []byte(message))
	if err != nil {
		return nil, errors.New("Error sending ws message: " + err.Error())
	}

	var msg []byte
	err = ws.conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	if err != nil {
		return nil, errors.New("Error setting timeout: " + err.Error())
	}
	_, msg, err = ws.conn.ReadMessage()
	if err != nil {
		return nil, errors.New("Error reading message: " + err.Error())
	}
	requestDuration := time.Since(start)
	ws.conn.Close()

	var response JsonMap
	err = json.Unmarshal(msg, &response)
	if err != nil {
		return nil, errors.New("Error unmarshalling message: " + err.Error())
	}
	return &ResponseData{response, StatusCode(200), "WS Ok", requestDuration}, nil
}

func (ws *WebSocketClient) Close() error {
	return ws.conn.Close()
}
