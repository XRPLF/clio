package main

import (
	"log"

	"xrplf/clio/clio_snapshot/internal/export"
	"xrplf/clio/clio_snapshot/internal/parse_args"
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
	}
	//TODO: Implement server mode
}
