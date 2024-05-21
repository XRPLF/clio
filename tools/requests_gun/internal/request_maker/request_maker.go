package request_maker

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"
)

type RequestMaker interface {
	MakeRequest(request string) (*ResponseData, error)
}

type HttpRequestMaker struct {
	url string
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
	response, err := http.Post(h.url, "application/json", strings.NewReader(request))
	requestDuration := time.Since(startTime)

	if err != nil {
        return nil, errors.New("Error making request: " + err.Error())
	}

	body, err := io.ReadAll(response.Body)
    response.Body.Close()
	if err != nil {
        return nil, err
	}

	if response.StatusCode != 200 {
        return &ResponseData{StatusCode: StatusCode(response.StatusCode), StatusStr: response.Status + ": "+ string(body)}, nil
	}

	var bodyParsed JsonMap
	err = json.Unmarshal(body, &bodyParsed)
	if err != nil {
        return nil, errors.New("Error parsing response '" + string(body) + "': " + err.Error())
	}

	return &ResponseData{bodyParsed, StatusCode(response.StatusCode), response.Status, requestDuration}, nil
}

func NewHttp(host string, port uint) *HttpRequestMaker {
    if !strings.HasPrefix(host, "http://") {
        host = "http://" + host
    }
    return &HttpRequestMaker{host + ":" + fmt.Sprintf("%d", port)}
}
