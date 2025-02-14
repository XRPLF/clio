package server

import (
	"context"
	"os"
	"testing"

	"github.com/stretchr/testify/assert"

	"xrplf/clio/clio_snapshot/internal/ledgers"
	pb "xrplf/clio/clio_snapshot/org/xrpl/rpc/v1"
)

func TestUnavaibleMethods(t *testing.T) {
	srv := newServer("testdata")

	req := &pb.GetLedgerDiffRequest{}
	_, err := srv.GetLedgerDiff(context.Background(), req)

	assert.Error(t, err)
	assert.Equal(t, err.Error(), "GetLedgerDiff not supported")

	req2 := &pb.GetLedgerEntryRequest{}
	_, err = srv.GetLedgerEntry(context.Background(), req2)

	assert.Error(t, err)
	assert.Equal(t, err.Error(), "GetLedgerEntry not supported")
}

func TestWhenPathIsInvalid(t *testing.T) {
	srv := newServer("testdata")

	req := &pb.GetLedgerRequest{
		Ledger: &pb.LedgerSpecifier{
			Ledger: &pb.LedgerSpecifier_Sequence{
				Sequence: 2,
			},
		},
	}

	_, err := srv.GetLedger(context.Background(), req)

	assert.Error(t, err)
	assert.Equal(t, err.Error(), "open testdata/ledger_diff_0/2.dat: no such file or directory")

	req2 := &pb.GetLedgerDataRequest{
		Ledger: &pb.LedgerSpecifier{
			Ledger: &pb.LedgerSpecifier_Sequence{
				Sequence: 2,
			},
		},
	}
	_, err = srv.GetLedgerData(context.Background(), req2)
	assert.Error(t, err)
	assert.Equal(t, err.Error(), "open testdata/ledger_data_2/marker_0000000000000000000000000000000000000000000000000000000000000000/0000000000000000000000000000000000000000000000000000000000000000.dat: no such file or directory")
}

func TestWhenPathIsValid(t *testing.T) {
	srv := newServer("testdata")
	ledger := ledgers.NewLedgersHouse("testdata")
	defer os.RemoveAll("testdata")

	marker := [32]byte{}
	ledger.WriteLedgerData(1, marker[:], &pb.GetLedgerDataResponse{})
	ledger.WriteLedgerDeltaData(1, &pb.GetLedgerResponse{})

	req := &pb.GetLedgerRequest{
		Ledger: &pb.LedgerSpecifier{
			Ledger: &pb.LedgerSpecifier_Sequence{
				Sequence: 1,
			},
		},
	}
	res, err := srv.GetLedger(context.Background(), req)
	assert.NoError(t, err)
	assert.NotNil(t, res)

	req2 := &pb.GetLedgerDataRequest{
		Ledger: &pb.LedgerSpecifier{
			Ledger: &pb.LedgerSpecifier_Sequence{
				Sequence: 1,
			},
		},
	}
	res2, err := srv.GetLedgerData(context.Background(), req2)
	assert.NoError(t, err)
	assert.NotNil(t, res2)
}
