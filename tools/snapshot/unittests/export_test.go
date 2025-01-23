package unittests

import (
	"os"
	"testing"

	"github.com/stretchr/testify/assert"

	"xrplf/clio/clio_snapshot/internal/export"
	"xrplf/clio/clio_snapshot/mocks"
	pb "xrplf/clio/clio_snapshot/org/xrpl/rpc/v1"

	"github.com/golang/mock/gomock"
)

func TestExportDeltaLedgerData(t *testing.T) {

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

			mockResponse := &pb.GetLedgerResponse{}

			times := tt.endSeq - tt.startSeq + 1
			if tt.endSeq < tt.startSeq {
				times = 0
			}

			mockClient.EXPECT().GetLedger(gomock.Any(), gomock.Any()).Return(mockResponse, nil).Times(int(times))

			defer os.RemoveAll("test")

			export.ExportFromDeltaLedgerImpl(mockClient, tt.startSeq, tt.endSeq, "test")

			_, err := os.Stat("test")

			assert.Equal(t, os.IsNotExist(err), tt.endSeq < tt.startSeq)
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
			export.ExportFromFullLedgerImpl(mockClient, tt.startSeq, tt.endSeq, "test")

			_, err := os.Stat("test")

			assert.False(t, os.IsNotExist(err))
		})
	}
}
