package export

import (
	"fmt"
	"os"
	"testing"

	"github.com/stretchr/testify/assert"

	"xrplf/clio/clio_snapshot/internal/ledgers"
	"xrplf/clio/clio_snapshot/mocks"
	pb "xrplf/clio/clio_snapshot/org/xrpl/rpc/v1"

	"github.com/golang/mock/gomock"
)

// Matcher used to verify the GetLedgerRequest parameters
type LedgerRequestMatcher struct {
	expectedObjects   bool
	expectedNeighbors bool
}

func (m LedgerRequestMatcher) Matches(x interface{}) bool {
	req, ok := x.(*pb.GetLedgerRequest)
	return ok && req.GetObjects == m.expectedObjects && req.GetObjectNeighbors == m.expectedNeighbors
}

func (m LedgerRequestMatcher) String() string {
	return fmt.Sprintf("LedgerRequest with objects=%v neighbors=%v", m.expectedObjects, m.expectedNeighbors)
}

func matchObjectsEquals(objects bool, neighbors bool) gomock.Matcher {
	return LedgerRequestMatcher{objects, neighbors}
}

func TestExportDeltaLedgerData(t *testing.T) {
	tests := []struct {
		name     string
		startSeq uint32
		endSeq   uint32
	}{
		{"OneSeq", 700000, 700000},
		{"MultipleSeq", 700000, 700019},
		{"FirstAvailableLedger", firstAvailableLedger, firstAvailableLedger},
		{"FirstAvailableLedgerMultipleSeq", firstAvailableLedger, firstAvailableLedger + 2},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			mockClient := mocks.NewMockXRPLedgerAPIServiceClient(ctrl)

			mockResponse := &pb.GetLedgerResponse{}

			times := tt.endSeq - tt.startSeq + 1
			if tt.endSeq < tt.startSeq {
				times = 0
			}

			if tt.startSeq == firstAvailableLedger {
				mockClient.EXPECT().GetLedger(gomock.Any(), matchObjectsEquals(false, false)).Return(mockResponse, nil).Times(1)
				mockClient.EXPECT().GetLedger(gomock.Any(), matchObjectsEquals(true, true)).Return(mockResponse, nil).Times(int(times) - 1)
			} else {
				mockClient.EXPECT().GetLedger(gomock.Any(), matchObjectsEquals(true, true)).Return(mockResponse, nil).Times(int(times))
			}

			os.MkdirAll("test", os.ModePerm)

			manifest := ledgers.NewManifest("test")
			manifest.SetLedgerRange(tt.startSeq-1, tt.startSeq-1)

			defer os.RemoveAll("test")

			exportFromDeltaLedgerImpl(mockClient, tt.startSeq, tt.endSeq, "test")

			seq1, seq2, err := manifest.Read()
			assert.Nil(t, err)
			assert.Equal(t, tt.startSeq-1, seq1)
			assert.Equal(t, tt.endSeq, seq2)
		})
	}
}

func TestExportFullLedgerData(t *testing.T) {
	tests := []struct {
		name     string
		startSeq uint32
		endSeq   uint32
	}{
		{"OneSeq", 1, 1},
		{"MultipleSeq", 1, 20},
		{"EndSeqLessThanStartSeq", 20, 1},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			mockClient := mocks.NewMockXRPLedgerAPIServiceClient(ctrl)

			mockDataResponse := &pb.GetLedgerDataResponse{}
			mockLedgerResponse := &pb.GetLedgerResponse{}

			timesLedgerDataCalled := 16
			timesLedgerCalled := tt.endSeq - tt.startSeq + 1

			if tt.endSeq < tt.startSeq {
				timesLedgerCalled = 0
			}

			mockClient.EXPECT().GetLedgerData(gomock.Any(), gomock.Any()).Return(mockDataResponse, nil).Times(timesLedgerDataCalled)

			mockClient.EXPECT().GetLedger(gomock.Any(), gomock.Any()).Return(mockLedgerResponse, nil).Times(int(timesLedgerCalled))

			defer os.RemoveAll("test")
			exportFromFullLedgerImpl(mockClient, tt.startSeq, tt.endSeq, "test")

			_, err := os.Stat("test")

			assert.False(t, os.IsNotExist(err))
		})
	}
}

func TestGenerateMarkers(t *testing.T) {
	tests := []struct {
		name string
		in   uint32
		out  [][32]byte
	}{
		{"GenerateMarkers1", 1, [][32]byte{{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}},
		{"GenerateMarkers2", 2, [][32]byte{{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
			{0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}},
		{"GenerateMarkers4", 4, [][32]byte{{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
			{0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
			{0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
			{0xc0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			assert.Equal(t, generateMarkers(tt.in), tt.out)
		})
	}
}
