package server

import (
	"context"
	"fmt"
	"log"
	"net"

	pb "xrplf/clio/clio_snapshot/org/xrpl/rpc/v1"

	"google.golang.org/grpc"
)

// create a server implement the xrpl rpc v1 server interface
type Server struct {
	// The server implementation
	pb.XRPLedgerAPIServiceServer
}

// GetLedger prints a message and returns a fake response.
func (s *Server) GetLedger(ctx context.Context, req *pb.GetLedgerRequest) (*pb.GetLedgerResponse, error) {
	log.Println("GetLedger called with request:", req)
	return &pb.GetLedgerResponse{}, nil
}

// GetLedgerEntry prints a message and returns a fake response.
func (s *Server) GetLedgerEntry(ctx context.Context, req *pb.GetLedgerEntryRequest) (*pb.GetLedgerEntryResponse, error) {
	log.Println("GetLedgerEntry called with request:", req)
	return &pb.GetLedgerEntryResponse{}, nil
}

// GetLedgerData prints a message and returns a fake response.
func (s *Server) GetLedgerData(ctx context.Context, req *pb.GetLedgerDataRequest) (*pb.GetLedgerDataResponse, error) {
	log.Println("GetLedgerData called with request:", req)
	return &pb.GetLedgerDataResponse{}, nil
}

// GetLedgerDiff prints a message and returns a fake response.
func (s *Server) GetLedgerDiff(ctx context.Context, req *pb.GetLedgerDiffRequest) (*pb.GetLedgerDiffResponse, error) {
	log.Println("GetLedgerDiff called with request:", req)
	return &pb.GetLedgerDiffResponse{}, nil
}

func newServer() *Server {
	s := &Server{}
	return s
}

func StartServer(port uint32) {
	lis, err := net.Listen("tcp", fmt.Sprintf("localhost:%d", port))

	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	pb.RegisterXRPLedgerAPIServiceServer(grpcServer, newServer())
	log.Print("Starting server...")
	grpcServer.Serve(lis)
}
