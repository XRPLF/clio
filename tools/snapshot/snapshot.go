package main

import (
	"flag"
	"log"

	"xrplf/clio/clio_snapshot/internal/export"
	"xrplf/clio/clio_snapshot/internal/server"
)

func validateExportFlags(seq, endSeq uint, path, grpcServer string) bool {
	return seq != 0 && endSeq != 0 && path != "" && grpcServer != ""
}

func main() {
	exportMode := flag.String("export", "", "Set export mode: 'full' (export full ledger data of start_seq) or 'delta' (only export ledger diff data)")
	seq := flag.Uint("start_seq", 0, "Starting sequence number")
	endSeq := flag.Uint("end_seq", 0, "Ending sequence number")
	path := flag.String("path", "", "Path to the data")
	grpcServer := flag.String("grpc_server", "localhost:50051", "rippled's gRPC server address")
	serverMode := flag.Bool("server", false, "Start server mode")
	grpcPort := flag.Int("grpc_port", 0, "Port for gRPC server to listen on")
	wsPort := flag.Int("ws_port", 0, "Port for WebSocket server to listen on")

	flag.Parse()

	// Ensure --export and --server are not used together
	if *serverMode && *exportMode != "" {
		log.Fatal("Error: --server and --export cannot be used at the same time.")
	}

	// Handle the --server mode
	if *serverMode {
		if *grpcPort == 0 || *wsPort == 0 || *path == "" {
			log.Fatal("Error: --grpc_port and --ws_port and --path are required for server mode.")
		}

		log.Printf("Starting server with gRPC on port %d and WebSocket on port %d...\n", *grpcPort, *wsPort)
		server.StartServer(uint32(*grpcPort))
		return
	}

	// Handle export mode
	if *exportMode != "" {
		if *exportMode == "full" {
			// Handle full export
			if !validateExportFlags(*seq, *endSeq, *path, *grpcServer) {
				log.Fatal("Error: --start_seq, --end_seq, --grpc_server and --path are required for full export")
			}
			log.Printf("Performing full export from seq %d to %d at path %s\n", *seq, *endSeq, *path)
			export.ExportFromFullLedger(*grpcServer, uint32(*seq), uint32(*endSeq), *path)

		} else if *exportMode == "delta" {
			// Handle delta export
			if !validateExportFlags(*seq, *endSeq, *path, *grpcServer) {
				log.Fatal("Error: --start_seq, --end_seq, --grpc_server and --path are required for delta export")
			}
			log.Printf("Performing delta export from seq %d to %d at path %s\n", *seq, *endSeq, *path)
			export.ExportFromDeltaLedger(*grpcServer, uint32(*seq), uint32(*endSeq), *path)
		} else {
			log.Fatal("Error: Invalid export mode. Use 'full' or 'delta'.")
		}
	} else {
		// Handle the case where neither export nor server mode is provided
		log.Fatal("Error: --export or --server flag is required.")
	}
}
