package server

import (
	"context"
	"fmt"

	"xrplf/clio/clio_snapshot/internal/ledgers"
	pb "xrplf/clio/clio_snapshot/org/xrpl/rpc/v1"
)

// create a server implement the xrpl rpc v1 server interface
type Server struct {
	pb.XRPLedgerAPIServiceServer
	ledgersHouse *ledgers.LedgersHouse
}

func (s *Server) GetLedger(ctx context.Context, req *pb.GetLedgerRequest) (*pb.GetLedgerResponse, error) {
	return s.ledgersHouse.ReadLedgerDeltaData(req.GetLedger().GetSequence())
}

func (s *Server) GetLedgerData(ctx context.Context, req *pb.GetLedgerDataRequest) (*pb.GetLedgerDataResponse, error) {
	marker := req.GetMarker()
	if marker == nil {
		marker = make([]byte, 32)
	}
	return s.ledgersHouse.ReadLedgerData(req.GetLedger().GetSequence(), marker)
}

func (s *Server) GetLedgerDiff(ctx context.Context, req *pb.GetLedgerDiffRequest) (*pb.GetLedgerDiffResponse, error) {
	return nil, fmt.Errorf("GetLedgerDiff not supported")
}

func (s *Server) GetLedgerEntry(ctx context.Context, req *pb.GetLedgerEntryRequest) (*pb.GetLedgerEntryResponse, error) {
	return nil, fmt.Errorf("GetLedgerEntry not supported")
}

func newServer(path string) *Server {
	s := &Server{}
	s.ledgersHouse = ledgers.NewLedgersHouse(path)
	return s
}
