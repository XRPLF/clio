package server

import (
	"fmt"
	"log"
	"net"

	"xrplf/clio/clio_snapshot/internal/ledgers"
	pb "xrplf/clio/clio_snapshot/org/xrpl/rpc/v1"

	"google.golang.org/grpc"
)

func StartServer(grpcServerAddr string, wsServerAddr string, path string) {
	ledgersHouse := ledgers.NewLedgersHouse(path)

	if !ledgersHouse.IsExist() {
		log.Fatalf("Can't start server againist invalid snapshot folder: %s", path)
	}

	startSeq, endSeq, err := ledgersHouse.GetRange()
	if err != nil {
		log.Fatalf("Failed to get range: %v", err)
	}

	lis, err := net.Listen("tcp", grpcServerAddr)

	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	pb.RegisterXRPLedgerAPIServiceServer(grpcServer, newServer(path))
	log.Print("Starting server...")
	go grpcServer.Serve(lis)

	wsServer := NewWebSocketServer("Snapshot Server", func(message string) string {
		//mimic the response of the ledger stream
		ledgerStreamReply := fmt.Sprintf("{\"fee_base\":10,\"ledger_hash\":\"A320C67DA7D1250A577AC5AACDF06ADC25E0EEEF7AE5B8D63CE2E1CC7F76A438\",\"ledger_index\":%d,\"ledger_time\":792853443,\"reserve_base\":1000000,\"reserve_inc\":200000,\"txn_count\":0,\"type\":\"ledgerClosed\",\"validated_ledgers\":\"%d-%d\"}",
			endSeq, startSeq, endSeq)
		return ledgerStreamReply
	})
	wsServer.Start(wsServerAddr)

	select {}
}
