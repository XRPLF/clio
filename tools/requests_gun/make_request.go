package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
)

type RequestMaker interface {
    MakeRequest(request string) (JsonMap, StatusCode, error)
}

type HttpRequestMaker struct {
    url string
}

type JsonMap map[string]interface{}
type StatusCode int


func (h *HttpRequestMaker) MakeRequest(request string) (JsonMap, StatusCode, error) {
    response, err := http.Post(h.url, "application/json", strings.NewReader(request))
    if err != nil {
        return JsonMap{}, StatusCode(0), err
    }

    if response.StatusCode != 200 {
        return JsonMap{}, StatusCode(response.StatusCode), nil
    }

    body, err := io.ReadAll(response.Body)
    if err != nil {
        return JsonMap{}, StatusCode(0), err
    }

    var result JsonMap
    err = json.Unmarshal(body, &result)
    if err != nil {
        return JsonMap{}, StatusCode(0), err
    }

    return result, StatusCode(response.StatusCode), nil
}

func NewHttpRequestMaker(host string, port uint) HttpRequestMaker {
    return HttpRequestMaker{host + ":" + fmt.Sprintf("%d",port)}
}
