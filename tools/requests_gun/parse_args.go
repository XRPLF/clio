package main

import (
	"fmt"
	"os"

	flag "github.com/spf13/pflag"
)

func PrintUsage() {
	fmt.Fprintf(os.Stderr, "Usage of %s:\n", os.Args[0])
	fmt.Fprintf(os.Stderr, "%s [options] <ammo_file>\n", os.Args[0])
    fmt.Fprintf(os.Stderr, "  <ammo_file>        Path to file with requests to use\n")
	fmt.Fprintf(os.Stderr, "Options:\n")
	flag.PrintDefaults()
}

type CliArgs struct {
	url         string
	port        uint
	target_load uint
	protocol    string
	ammo        string
}

func parseArgs() CliArgs {
	flag.Usage = PrintUsage
	url := flag.StringP("url", "u", "localhost", "URL to send the request to")
	port := flag.UintP("port", "p", 51233, "Port to send the request to")
	target_load := flag.UintP("load", "l", 100, "Target requests per second load")
	use_ws := flag.BoolP("ws", "w", false, "Use websocket protocol for requests")
	help := flag.BoolP("help", "h", false, "Print help message")

	flag.Parse()

	if flag.NFlag() == 0 {
        flag.Usage()
		os.Exit(1)
	}

	if *help {
		flag.Usage()
		os.Exit(0)
	}

	protocol := "http"
	if *use_ws {
		protocol = "ws"
	}

	return CliArgs{*url, *port, *target_load, protocol, flag.Arg(0)}
}
