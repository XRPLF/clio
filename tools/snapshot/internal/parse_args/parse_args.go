package parse_args

import (
	"fmt"
	"os"

	flag "github.com/spf13/pflag"
)

type Args struct {
	ExportMode string
	StartSeq   uint32
	EndSeq     uint32
	Path       string
	GrpcServer string
	WsServer   string
	ServerMode bool
	ShowRange  bool
}

func Parse() (*Args, error) {

	fs := flag.NewFlagSet(os.Args[0], flag.ContinueOnError)
	fs.Usage = PrintUsage

	exportMode := fs.StringP("export", "e", "", "Set export mode: 'full' (export full ledger data of start_seq) or 'delta' (only export ledger diff data)")
	seq := fs.Uint32("start_seq", 0, "Starting sequence number")
	endSeq := fs.Uint32("end_seq", 0, "Ending sequence number")
	path := fs.StringP("path", "p", "", "Path to the data")
	grpcServer := fs.StringP("grpc_server", "g", "0.0.0.0:50051", "rippled's gRPC server address")
	wsServer := fs.StringP("ws_server", "w", "0.0.0.0:6006", "rippled's gRPC server address")
	serverMode := fs.BoolP("server", "s", false, "Start server mode")
	showRange := fs.BoolP("range", "r", false, "Show the range of the snapshot")
	fs.Parse(os.Args[1:])

	if *serverMode && *exportMode != "" {
		return nil, fmt.Errorf("Invalid usage: --server and --export cannot be used at the same time.")
	}

	if *serverMode {
		if *grpcServer == "" || *wsServer == "" || *path == "" {
			return nil, fmt.Errorf("Invalid usage: --grpc_server and --ws_server and --path are required for server mode.")
		}
	} else if *exportMode != "" {
		if *exportMode == "full" || *exportMode == "delta" {
			if *seq == 0 || *endSeq == 0 || *path == "" || *grpcServer == "" {
				return nil, fmt.Errorf("Invalid usage: --start_seq, --end_seq, --grpc_server and --path are required for export.")
			}
		} else {
			return nil, fmt.Errorf("Invalid usage: Invalid export mode. Use 'full' or 'delta'.")
		}
	} else if *showRange {
		if *path == "" {
			return nil, fmt.Errorf("Invalid usage: --path is required for show range.")
		}
	} else {
		return nil, fmt.Errorf("Invalid usage: --export or --server or --range flag is required.")
	}

	return &Args{*exportMode, *seq, *endSeq, *path, *grpcServer, *wsServer, *serverMode, *showRange}, nil
}

func PrintUsage() {
	fmt.Println("Usage: clio_snapshot [options]")
	fmt.Println("Options:")
	flag.PrintDefaults()
}
