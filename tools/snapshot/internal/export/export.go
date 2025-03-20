package export

import (
	"context"
	"fmt"
	"log"
	"sync"
	pb "xrplf/clio/clio_snapshot/org/xrpl/rpc/v1"

	"xrplf/clio/clio_snapshot/internal/ledgers"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

const (
	grpcUser             = "clio-snapshot"
	markerNum            = 16
	maxConcurrency       = 256
	firstAvailableLedger = 32570
)

type gRPCClient struct {
	Client pb.XRPLedgerAPIServiceClient
	conn   *grpc.ClientConn
}

func (c *gRPCClient) Close() error {
	return c.conn.Close()
}

func createGRPCClient(serverAddr string) (*gRPCClient, error) {
	opts := []grpc.DialOption{grpc.WithTransportCredentials(insecure.NewCredentials())}

	conn, err := grpc.NewClient(serverAddr, opts...)
	if err != nil {
		return nil, fmt.Errorf("Failed to dial: %v", err)
	}

	client := pb.NewXRPLedgerAPIServiceClient(conn)
	return &gRPCClient{
		Client: client,
		conn:   conn,
	}, nil
}

func getLedgerDeltaDataInParallel(client pb.XRPLedgerAPIServiceClient, startSeq uint32, endSeq uint32, ledgersHouse *ledgers.LedgersHouse) {
	sem := make(chan struct{}, maxConcurrency)
	var wg sync.WaitGroup
	for i := startSeq; i <= endSeq; i++ {
		wg.Add(1)
		sem <- struct{}{}

		go func(seq uint32) {
			defer wg.Done()
			log.Printf("Process delta sequence: %d\n", seq)
			getLedgerDeltaData(client, seq, ledgersHouse)
			<-sem
		}(i)
	}

	wg.Wait()
}

func getLedgerDeltaData(client pb.XRPLedgerAPIServiceClient, seq uint32, ledgersHouse *ledgers.LedgersHouse) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	request := pb.GetLedgerRequest{}
	ledger := &pb.LedgerSpecifier{
		Ledger: &pb.LedgerSpecifier_Sequence{
			Sequence: seq,
		},
	}
	request.Ledger = ledger
	request.User = grpcUser
	request.Transactions = true
	request.Expand = true

	// The first available ledger doesn't have diff data
	request.GetObjects = firstAvailableLedger != seq
	request.GetObjectNeighbors = firstAvailableLedger != seq

	response, err := client.GetLedger(ctx, &request)

	if err != nil {
		log.Fatalf("Error getting ledger delta data: %v - seq: %d", err, seq)
	}

	err = ledgersHouse.WriteLedgerDeltaData(seq, response)
	if err != nil {
		log.Fatalf("Error writing ledger delta data: %v", err)
	}

	log.Printf("Processing delta sequence: %d\n", seq)
}

func generateMarkers(markerNum uint32) [][32]byte {
	var byteArray [32]byte

	incr := 256 / markerNum

	var byteArrayList [][32]byte

	for i := 0; i < int(markerNum); i++ {
		byteArray[0] = byte(i * int(incr)) // Increment the highest byte
		byteArrayList = append(byteArrayList, byteArray)
	}

	return byteArrayList
}

func getLedgerData(client pb.XRPLedgerAPIServiceClient, seq uint32, marker []byte, end []byte, ledgerHouse *ledgers.LedgersHouse) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	request := pb.GetLedgerDataRequest{}
	ledger := &pb.LedgerSpecifier{
		Ledger: &pb.LedgerSpecifier_Sequence{
			Sequence: seq,
		},
	}
	request.Ledger = ledger
	request.Marker = marker[:]
	if end != nil {
		request.EndMarker = end[:]
	}
	request.User = grpcUser

	for request.Marker != nil {
		res, err := client.GetLedgerData(ctx, &request)
		if err != nil {
			log.Fatalf("Error getting ledger data: %v", err)
		}

		err = ledgerHouse.WriteLedgerData(seq, request.Marker, res)
		if err != nil {
			log.Fatalf("Error writing ledger data: %v", err)
		}
		log.Printf("Saving ledger data %x", request.Marker)
		request.Marker = res.Marker
	}
}

func getLedgerFullData(client pb.XRPLedgerAPIServiceClient, seq uint32, ledgerHouse *ledgers.LedgersHouse) {
	log.Printf("Processing full sequence: %d\n", seq)

	markers := generateMarkers(markerNum)

	var wg sync.WaitGroup

	// Launch a goroutine for each marker
	for i, marker := range markers {
		wg.Add(1)
		var end []byte
		if i != len(markers)-1 {
			end = markers[i+1][:]
		}

		go func() {
			defer wg.Done()
			getLedgerData(client, seq, marker[:], end, ledgerHouse)
		}()

	}

	wg.Wait()
}

func ExportFromFullLedger(grpcServer string, startSeq uint32, endSeq uint32, path string) {
	client, err := createGRPCClient(grpcServer)
	if err != nil {
		log.Fatalf("Error creating gRPC client: %v", err)
	}

	defer client.Close()

	exportFromFullLedgerImpl(client.Client, startSeq, endSeq, path)
}

func exportFromFullLedgerImpl(client pb.XRPLedgerAPIServiceClient, startSeq uint32, endSeq uint32, path string) {
	ledgersHouse := ledgers.NewLedgersHouse(path)

	getLedgerFullData(client, startSeq, ledgersHouse)

	getLedgerDeltaDataInParallel(client, startSeq, endSeq, ledgersHouse)

	err := ledgersHouse.SetRange(startSeq, endSeq)
	if err != nil {
		log.Fatalf("Error writing range: %v", err)
	}

	log.Printf("Exporting from full ledger: %d to %d at path %s\n", startSeq, endSeq, path)
}

func ExportFromDeltaLedger(grpcServer string, startSeq uint32, endSeq uint32, path string) {
	client, err := createGRPCClient(grpcServer)
	if err != nil {
		log.Fatalf("Error creating gRPC client: %v", err)
	}

	defer client.Close()

	exportFromDeltaLedgerImpl(client.Client, startSeq, endSeq, path)
}

func exportFromDeltaLedgerImpl(client pb.XRPLedgerAPIServiceClient, startSeq uint32, endSeq uint32, path string) {
	ledgersHouse := ledgers.NewLedgersHouse(path)

	_, oldEnd, err := ledgersHouse.GetRange()
	if err != nil {
		log.Fatalf("Can't find existing snapshot to extend: %v", err)
	}

	if oldEnd < startSeq-1 {
		log.Fatalf("Missing delta ledger from %d to %d", oldEnd, startSeq)
	}

	if oldEnd >= endSeq {
		log.Fatalf("The snapshot already contains the requested delta ledger")
	}

	getLedgerDeltaDataInParallel(client, startSeq, endSeq, ledgersHouse)

	err = ledgersHouse.AppendDeltaLedger(startSeq, endSeq)

	if err != nil {
		log.Fatalf("Error writing new range: %v", err)
	}

	log.Printf("Exporting from ledger: %d to %d at path %s\n", startSeq, endSeq, path)
}
