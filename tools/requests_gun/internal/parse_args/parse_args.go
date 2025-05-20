package parse_args

import (
	"fmt"

	flag "github.com/spf13/pflag"
)

type CliArgs struct {
	Host        string
	Port        uint
	TargetLoad  uint
	Ammo        string
	PrintErrors bool
	Help        bool
	Ws          bool
}

func Parse() (*CliArgs, error) {
	flag.Usage = PrintUsage
	host := flag.StringP("url", "u", "localhost", "URL to send the request to")
	port := flag.UintP("port", "p", 51233, "Port to send the request to")
	target_load := flag.UintP("load", "l", 100, "Target requests per second load")
	print_errors := flag.BoolP("print-errors", "e", false, "Print errors")
	help := flag.BoolP("help", "h", false, "Print help message")
	ws := flag.BoolP("ws", "w", false, "Use websocket")

	flag.Parse()

	if flag.NArg() == 0 {
		return nil, fmt.Errorf("No ammo file provided")
	}

	return &CliArgs{*host, *port, *target_load, flag.Arg(0), *print_errors, *help, *ws}, nil
}

func PrintUsage() {
	fmt.Println("Usage: requests_gun [options] <ammo_file>")
	fmt.Println("  <ammo_file>        Path to file with requests to use")
	fmt.Println("Options:")
	flag.PrintDefaults()
}
