package export

import (
	"context"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sync"
	pb "xrplf/clio/clio_snapshot/org/xrpl/rpc/v1"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/protobuf/proto"
)

const (
	deltaDataFolderDiv = 10000
	grpcUser           = "clio-snapshot"
	markerNum          = 16
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

func getLedgerDeltaData(client pb.XRPLedgerAPIServiceClient, seq uint32, path string) {
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
	request.GetObjectNeighbors = true
	request.Transactions = true
	request.Expand = true
	request.GetObjects = true

	response, err := client.GetLedger(ctx, &request)

	if err != nil {
		log.Fatalf("Error getting ledger data: %v", err)
	}

	saveLedgerDeltaData(seq, response, path)

	log.Printf("Processing delta sequence: %d\n", seq)
}

func roundDown(n uint32, roundTo uint32) uint32 {
	if roundTo == 0 {
		return n
	}
	return n - (n % roundTo)
}

func saveLedgerDeltaData(seq uint32, response *pb.GetLedgerResponse, path string) {
	subPath := filepath.Join(path, fmt.Sprintf("ledger_diff_%d", roundDown(seq, deltaDataFolderDiv)))
	err := os.MkdirAll(subPath, os.ModePerm)
	if err != nil {
		log.Fatalf("Error creating directory: %v", err)
	}

	protoData, err := proto.Marshal(response)
	if err != nil {
		log.Fatalf("Error marshalling data: %v", err)
	}

	filePath := filepath.Join(subPath, fmt.Sprintf("%d.dat", seq))

	err = os.WriteFile(filePath, protoData, 0644)
	if err != nil {
		log.Fatalf("failed to write file: %v", err)
	}
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

func saveLedgerData(path string, data *pb.GetLedgerDataResponse) {
	protoData, err := proto.Marshal(data)
	if err != nil {
		log.Fatalf("Error marshalling data: %v", err)
	}

	err = os.WriteFile(path, protoData, 0644)
	if err != nil {
		log.Fatalf("failed to write file: %v", err)
	}
}

func getLedgerData(client pb.XRPLedgerAPIServiceClient, seq uint32, marker []byte, end []byte, path string) {
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

	subPath := filepath.Join(path, fmt.Sprintf("ledger_data_%d", seq), fmt.Sprintf("marker_%x", marker))
	err := os.MkdirAll(subPath, os.ModePerm)
	if err != nil {
		log.Fatalf("Error creating directory: %v", err)
	}

	for request.Marker != nil {
		res, err := client.GetLedgerData(ctx, &request)
		if err != nil {
			log.Fatalf("Error getting ledger data: %v", err)
		}

		filePath := filepath.Join(subPath, fmt.Sprintf("%x.dat", request.Marker))
		saveLedgerData(filePath, res)
		request.Marker = res.Marker
	}
}

func getLedgerFullData(client pb.XRPLedgerAPIServiceClient, seq uint32, path string) {
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

		fmt.Printf("Got ledger data marker: %x-%x\n", marker, end)

		go func() {
			defer wg.Done()
			getLedgerData(client, seq, marker[:], end, path)
		}()

	}

	wg.Wait()
}

func checkPath(path string) {
	if _, err := os.Stat(path); os.IsNotExist(err) {
		// Create the directory if it doesn't exist
		err := os.MkdirAll(path, os.ModePerm)
		if err != nil {
			log.Fatalf("Error creating directory: %v", err)
		}
	}
}

func ExportFromFullLedger(grpcServer string, startSeq uint32, endSeq uint32, path string) {
	checkPath(path)

	client, err := createGRPCClient(grpcServer)
	if err != nil {
		log.Fatalf("Error creating gRPC client: %v", err)
	}

	defer client.Close()

	exportFromFullLedgerImpl(client.Client, startSeq, endSeq, path)
}

func exportFromFullLedgerImpl(client pb.XRPLedgerAPIServiceClient, startSeq uint32, endSeq uint32, path string) {

	getLedgerFullData(client, startSeq, path)

	//We need to fetch the ledger header and txs for startSeq as well
	for i := startSeq; i <= endSeq; i++ {
		getLedgerDeltaData(client, i, path)
	}

	log.Printf("Exporting from full ledger: %d to %d at path %s\n", startSeq, endSeq, path)
}

func ExportFromDeltaLedger(grpcServer string, startSeq uint32, endSeq uint32, path string) {
	checkPath(path)

	client, err := createGRPCClient(grpcServer)
	if err != nil {
		log.Fatalf("Error creating gRPC client: %v", err)
	}

	defer client.Close()

	exportFromDeltaLedgerImpl(client.Client, startSeq, endSeq, path)
}

func exportFromDeltaLedgerImpl(client pb.XRPLedgerAPIServiceClient, startSeq uint32, endSeq uint32, path string) {
	for i := startSeq; i <= endSeq; i++ {
		getLedgerDeltaData(client, i, path)
	}

	log.Printf("Exporting from ledger: %d to %d at path %s\n", startSeq, endSeq, path)
}
