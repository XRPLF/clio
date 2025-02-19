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
	ServerMode bool
	GrpcPort   uint32
	WsPort     uint32
}

func Parse() (*Args, error) {

	fs := flag.NewFlagSet(os.Args[0], flag.ContinueOnError)
	fs.Usage = PrintUsage

	exportMode := fs.StringP("export", "e", "", "Set export mode: 'full' (export full ledger data of start_seq) or 'delta' (only export ledger diff data)")
	seq := fs.Uint32("start_seq", 0, "Starting sequence number")
	endSeq := fs.Uint32("end_seq", 0, "Ending sequence number")
	path := fs.StringP("path", "p", "", "Path to the data")
	grpcServer := fs.StringP("grpc_server", "g", "localhost:50051", "rippled's gRPC server address")
	serverMode := fs.BoolP("server", "s", false, "Start server mode")
	grpcPort := fs.Uint32("grpc_port", 0, "Port for gRPC server to listen on")
	wsPort := fs.Uint32("ws_port", 0, "Port for WebSocket server to listen on")
	fs.Parse(os.Args[1:])

	if *serverMode && *exportMode != "" {
		return nil, fmt.Errorf("Invalid usage: --server and --export cannot be used at the same time.")
	}

	if *serverMode {
		if *grpcPort == 0 || *wsPort == 0 || *path == "" {
			return nil, fmt.Errorf("Invalid usage: --grpc_port and --ws_port and --path are required for server mode.")
		}
	} else if *exportMode != "" {
		if *exportMode == "full" || *exportMode == "delta" {
			if *seq == 0 || *endSeq == 0 || *path == "" || *grpcServer == "" {
				return nil, fmt.Errorf("Invalid usage: --start_seq, --end_seq, --grpc_server and --path are required for export")
			}
		} else {
			return nil, fmt.Errorf("Invalid usage: Invalid export mode. Use 'full' or 'delta'.")
		}
	} else {
		return nil, fmt.Errorf("Invalid usage: --export or --server flag is required.")
	}

	return &Args{*exportMode, *seq, *endSeq, *path, *grpcServer, *serverMode, *grpcPort, *wsPort}, nil
}

func PrintUsage() {
	fmt.Println("Usage: clio_snapshot [options]")
	fmt.Println("Options:")
	flag.PrintDefaults()
}
