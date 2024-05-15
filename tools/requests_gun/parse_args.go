package main

import (
	"os"
	flag "github.com/spf13/pflag"
)

type CliArgs struct {
	url         string
	port        uint
	target_load uint
	protocol    string
}

func parseArgs() CliArgs {
	url := flag.StringP("url", "u", "localhost", "URL to send the request to")
	port := flag.UintP("port", "p", 51233, "Port to send the request to")
	target_load := flag.UintP("load", "l", 100, "Target requests per second load")
	use_ws := flag.BoolP("ws", "w", false, "Use websocket protocol for requests")
    help := flag.BoolP("help", "h", false, "Print help message")

	flag.Parse()

    if *help {
        flag.Usage()
        os.Exit(0)
    }

	protocol := "http"
	if *use_ws {
		protocol = "ws"
	}

	return CliArgs{*url, *port, *target_load, protocol}
}
