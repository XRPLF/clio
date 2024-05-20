package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"
)

type RequestMaker interface {
	MakeRequest(request string) ResponseData
}

type HttpRequestMaker struct {
	url string
}

type JsonMap map[string]interface{}
type StatusCode int

type ResponseData struct {
	body       JsonMap
	statusCode StatusCode
	duration   time.Duration
	error      error
}

func newResponseData_fromError(err error) ResponseData {
	return ResponseData{JsonMap{}, StatusCode(0), 0, err}
}

func newResponseData_fromStatusCode(statusCode StatusCode) ResponseData {
	return ResponseData{JsonMap{}, statusCode, 0, nil}
}

func (h *HttpRequestMaker) MakeRequest(request string) ResponseData {
	startTime := time.Now()
	response, err := http.Post(h.url, "application/json", strings.NewReader(request))
	requestDuration := time.Since(startTime)

	if err != nil {
		return newResponseData_fromError(err)
	}

	if response.StatusCode != 200 {
		return newResponseData_fromStatusCode(StatusCode(response.StatusCode))
	}

	body, err := io.ReadAll(response.Body)
	if err != nil {
		return newResponseData_fromError(err)
	}

	var bodyParsed JsonMap
	err = json.Unmarshal(body, &bodyParsed)
	if err != nil {
		return newResponseData_fromError(err)
	}

	return ResponseData{bodyParsed, StatusCode(response.StatusCode), requestDuration, nil}
}

func NewHttpRequestMaker(host string, port uint) HttpRequestMaker {
	return HttpRequestMaker{host + ":" + fmt.Sprintf("%d", port)}
}
