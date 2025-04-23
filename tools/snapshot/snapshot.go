package main

import (
	"log"

	"xrplf/clio/clio_snapshot/internal/export"
	"xrplf/clio/clio_snapshot/internal/ledgers"
	"xrplf/clio/clio_snapshot/internal/parse_args"
	"xrplf/clio/clio_snapshot/internal/server"
)

func main() {
	args, err := parse_args.Parse()
	if err != nil {
		log.Fatal(err)
	}

	if args.ExportMode == "full" {
		export.ExportFromFullLedger(args.GrpcServer, args.StartSeq, args.EndSeq, args.Path)

	} else if args.ExportMode == "delta" {
		export.ExportFromDeltaLedger(args.GrpcServer, args.StartSeq, args.EndSeq, args.Path)
	} else if args.ServerMode {
		server.StartServer(args.GrpcServer, args.WsServer, args.Path)
	} else if args.ShowRange {
		ledgers := ledgers.NewLedgersHouse(args.Path)
		if !ledgers.IsExist() {
			log.Fatalf("Invalid snapshot folder: %s", args.Path)
		}
		startSeq, endSeq, err := ledgers.GetRange()
		if err == nil {
			log.Printf("Snapshot range: %d-%d", startSeq, endSeq)
		} else {
			log.Fatalf("Failed to get snapshot range: %v", err)
		}
	}
}
