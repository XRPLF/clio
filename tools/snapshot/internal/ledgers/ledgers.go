package ledgers

import (
	"fmt"
	"os"
	"path/filepath"

	"google.golang.org/protobuf/proto"

	pb "xrplf/clio/clio_snapshot/org/xrpl/rpc/v1"
)

const deltaDataFolderDiv = 10000
const readWritePerm = 0644

func convertInnerMarkerToMarker(in []byte) []byte {
	if in == nil {
		return nil
	}
	out := make([]byte, len(in))
	out[0] = in[0] & 0xf0
	return out
}

func checkPath(path string) error {
	dir := filepath.Dir(path)
	if _, err := os.Stat(dir); os.IsNotExist(err) {
		// Create the directory if it doesn't exist
		err := os.MkdirAll(dir, os.ModePerm)
		if err != nil {
			return fmt.Errorf("Error creating directory: %s,%v", path, err)
		}
	}
	return nil
}

func roundDown(n uint32, roundTo uint32) uint32 {
	if roundTo == 0 {
		return n
	}
	return n - (n % roundTo)
}

type LedgersHouse struct {
	path     string
	manifest *Manifest
}

func NewLedgersHouse(path string) *LedgersHouse {
	return &LedgersHouse{path: path, manifest: NewManifest(path)}
}

func (lh *LedgersHouse) deltaDataPath(seq uint32) string {
	subPath := filepath.Join(lh.path, fmt.Sprintf("ledger_diff_%d", roundDown(seq, deltaDataFolderDiv)))
	return filepath.Join(subPath, fmt.Sprintf("%d.dat", seq))
}

func (lh *LedgersHouse) fullDataPath(seq uint32, marker string, innerMarker string) string {
	subPath := filepath.Join(lh.path, fmt.Sprintf("ledger_data_%d", seq), fmt.Sprintf("marker_%s", marker))
	return filepath.Join(subPath, fmt.Sprintf("%s.dat", innerMarker))
}

func (lh *LedgersHouse) ReadLedgerDeltaData(seq uint32) (*pb.GetLedgerResponse, error) {
	path := lh.deltaDataPath(seq)

	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var ledger pb.GetLedgerResponse
	if err := proto.Unmarshal(data, &ledger); err != nil {
		return nil, err
	}
	return &ledger, nil
}

func (lh *LedgersHouse) WriteLedgerDeltaData(seq uint32, data *pb.GetLedgerResponse) error {
	path := lh.deltaDataPath(seq)
	err := checkPath(path)
	if err != nil {
		return err
	}

	dataBytes, err := proto.Marshal(data)
	if err != nil {
		return err
	}
	return os.WriteFile(path, dataBytes, readWritePerm)
}

func (lh *LedgersHouse) ReadLedgerData(seq uint32, innerMarker []byte) (*pb.GetLedgerDataResponse, error) {
	marker := convertInnerMarkerToMarker(innerMarker)
	path := lh.fullDataPath(seq, fmt.Sprintf("%x", marker), fmt.Sprintf("%x", innerMarker))
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var ledger pb.GetLedgerDataResponse
	if err := proto.Unmarshal(data, &ledger); err != nil {
		return nil, err
	}
	return &ledger, nil
}

func (lh *LedgersHouse) WriteLedgerData(seq uint32, innerMarker []byte, data *pb.GetLedgerDataResponse) error {
	path := lh.fullDataPath(seq, fmt.Sprintf("%x", convertInnerMarkerToMarker(innerMarker)), fmt.Sprintf("%x", innerMarker))
	err := checkPath(path)
	if err != nil {
		return err
	}

	dataBytes, err := proto.Marshal(data)
	if err != nil {
		return err
	}
	return os.WriteFile(path, dataBytes, readWritePerm)
}

func (lh *LedgersHouse) SetRange(startSeq uint32, endSeq uint32) error {
	return lh.manifest.SetLedgerRange(startSeq, endSeq)
}

func (lh *LedgersHouse) AppendDeltaLedger(startSeq uint32, endSeq uint32) error {
	return lh.manifest.AppendDeltaLedger(startSeq, endSeq)
}

func (lh *LedgersHouse) IsExist() bool {
	return lh.manifest.IsExist()
}

func (lh *LedgersHouse) GetRange() (uint32, uint32, error) {
	return lh.manifest.Read()
}
