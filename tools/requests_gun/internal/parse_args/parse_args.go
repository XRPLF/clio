package parse_args

import (
	"fmt"
	"os"

	flag "github.com/spf13/pflag"
)

type CliArgs struct {
	Url         string
	Port        uint
	Target_load uint
	Ammo        string
    Help        bool
}

func Parse() (*CliArgs, error) {
	flag.Usage = PrintUsage
	url := flag.StringP("url", "u", "localhost", "URL to send the request to")
	port := flag.UintP("port", "p", 51233, "Port to send the request to")
	target_load := flag.UintP("load", "l", 100, "Target requests per second load")
	help := flag.BoolP("help", "h", false, "Print help message")

	flag.Parse()

	if flag.NArg() == 0 {
        return nil, fmt.Errorf("No ammo file provided")
	}

	return &CliArgs{*url, *port, *target_load, flag.Arg(0), *help}, nil
}

func PrintUsage() {
	fmt.Printf("Usage of %s:\n", os.Args[0])
	fmt.Printf("%s [options] <ammo_file>\n", os.Args[0])
    fmt.Printf("  <ammo_file>        Path to file with requests to use\n")
	fmt.Printf("Options:\n")
	flag.PrintDefaults()
}

