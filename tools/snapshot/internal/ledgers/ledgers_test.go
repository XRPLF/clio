package ledgers

import (
	"os"
	"path/filepath"
	"testing"
	pb "xrplf/clio/clio_snapshot/org/xrpl/rpc/v1"

	"github.com/stretchr/testify/assert"
)

func TestCheckPath(t *testing.T) {
	tests := []struct {
		name string
		path string
	}{
		{"Path", "test/d.dat"},
		{"NestedPath", "test/test/d.dat"}}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := checkPath(tt.path)
			assert.NoError(t, err)
			dir := filepath.Dir(tt.path)
			defer os.RemoveAll("test")
			_, err = os.Stat(dir)
			assert.False(t, os.IsNotExist(err))
		})
	}

}

func TestRoundDown(t *testing.T) {
	tests := []struct {
		name string
		in1  uint32
		in2  uint32
		out  uint32
	}{
		{"RoundDownToZero", 10, 0, 10},
		{"RoundDown12To10", 12, 10, 10},
		{"RoundDownToOne", 13, 1, 13},
		{"RoundDown100", 103, 100, 100},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			assert.Equal(t, roundDown(tt.in1, tt.in2), tt.out)
		})
	}
}

func TestConvertInnerMarkerToMarker(t *testing.T) {

	tests := []struct {
		name string
		in   []byte
		out  []byte
	}{
		{"SingleByte", []byte{0x01}, []byte{0x00}},
		{"MultipleBytes", []byte{0x01, 0x02, 0x03}, []byte{0x00, 0x00, 0x00}},
		{"MultipleBytes2", []byte{0xf1, 0x02, 0x03}, []byte{0xf0, 0x00, 0x00}},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			assert.Equal(t, convertInnerMarkerToMarker(tt.in), tt.out)
		})
	}
}

func TestLedgersHouseGetDeltaPath(t *testing.T) {
	lh := NewLedgersHouse("testdata")
	assert.Equal(t, lh.deltaDataPath(12345), "testdata/ledger_diff_10000/12345.dat")

	assert.Equal(t, lh.deltaDataPath(3), "testdata/ledger_diff_0/3.dat")

	assert.Equal(t, lh.deltaDataPath(0), "testdata/ledger_diff_0/0.dat")
}

func TestLedgersHouseGetFullDataPath(t *testing.T) {
	lh := NewLedgersHouse("testdata")
	assert.Equal(t, lh.fullDataPath(12345, "fffff", "ababab"), "testdata/ledger_data_12345/marker_fffff/ababab.dat")
}

func TestLedgerHouseLedgerDeltaData(t *testing.T) {
	defer os.RemoveAll("testdata")
	lh := NewLedgersHouse("testdata")
	data, err := lh.ReadLedgerDeltaData(12345)
	assert.True(t, data == nil)
	assert.True(t, err != nil)

	lh.WriteLedgerDeltaData(12345, &pb.GetLedgerResponse{})
	data, err = lh.ReadLedgerDeltaData(12345)
	assert.True(t, data != nil)
	assert.True(t, err == nil)
}

func TestLedgerHouseInvalidLedgerDeltaPath(t *testing.T) {
	lh := NewLedgersHouse("/etc")
	data, err := lh.ReadLedgerDeltaData(12345)
	assert.True(t, data == nil)
	assert.True(t, err != nil)

	err = lh.WriteLedgerDeltaData(12345, &pb.GetLedgerResponse{})
	assert.True(t, err != nil)
}

func TestLedgerHouseLedgerData(t *testing.T) {
	defer os.RemoveAll("testdata")
	lh := NewLedgersHouse("testdata")
	data, err := lh.ReadLedgerData(12345, []byte{0x01})
	assert.True(t, data == nil)
	assert.True(t, err != nil)

	lh.WriteLedgerData(12345, []byte{0x01}, &pb.GetLedgerDataResponse{})
	data, err = lh.ReadLedgerData(12345, []byte{0x01})
	assert.True(t, data != nil)
	assert.True(t, err == nil)
}

func TestLedgerHouseInvalidLedgerDataPath(t *testing.T) {
	lh := NewLedgersHouse("/etc")
	data, err := lh.ReadLedgerData(12345, []byte{0x01})
	assert.True(t, data == nil)
	assert.True(t, err != nil)

	err = lh.WriteLedgerData(12345, []byte{0x01}, &pb.GetLedgerDataResponse{})
	assert.True(t, err != nil)
}

func TestLedgersHouseManifest(t *testing.T) {

	defer os.RemoveAll("testdata")

	lh := NewLedgersHouse("testdata")
	startSeq, endSeq, err := lh.GetRange()
	assert.True(t, err != nil)
	assert.Equal(t, startSeq, uint32(0))
	assert.Equal(t, endSeq, uint32(0))
	assert.False(t, lh.IsExist())

	lh.SetRange(1, 100)
	assert.True(t, lh.IsExist())
	startSeq, endSeq, err = lh.GetRange()
	assert.True(t, err == nil)
	assert.Equal(t, startSeq, uint32(1))
	assert.Equal(t, endSeq, uint32(100))

	lh.AppendDeltaLedger(100, 200)
	assert.True(t, lh.IsExist())
	startSeq, endSeq, err = lh.GetRange()
	assert.True(t, err == nil)
	assert.Equal(t, startSeq, uint32(1))
	assert.Equal(t, endSeq, uint32(200))

	lh.AppendDeltaLedger(201, 300)
	assert.True(t, lh.IsExist())
	startSeq, endSeq, err = lh.GetRange()
	assert.True(t, err == nil)
	assert.Equal(t, startSeq, uint32(1))
	assert.Equal(t, endSeq, uint32(300))

	err = lh.AppendDeltaLedger(201, 100)
	assert.True(t, err != nil)
	assert.True(t, lh.IsExist())
	startSeq, endSeq, err = lh.GetRange()
	assert.True(t, err == nil)
	assert.Equal(t, startSeq, uint32(1))
	assert.Equal(t, endSeq, uint32(300))

	err = lh.AppendDeltaLedger(302, 350)
	assert.True(t, err != nil)
	assert.True(t, lh.IsExist())
	startSeq, endSeq, err = lh.GetRange()
	assert.True(t, err == nil)
	assert.Equal(t, startSeq, uint32(1))
	assert.Equal(t, endSeq, uint32(300))

	err = lh.AppendDeltaLedger(0, 350)
	assert.True(t, err != nil)
	assert.True(t, lh.IsExist())
	startSeq, endSeq, err = lh.GetRange()
	assert.True(t, err == nil)
	assert.Equal(t, startSeq, uint32(1))
	assert.Equal(t, endSeq, uint32(300))

}
