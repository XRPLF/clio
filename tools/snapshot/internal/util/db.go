package util

import (
	"github.com/gocql/gocql"
)

type Range struct {
	FirstLedgerIdx  uint64
	LatestLedgerIdx uint64
}

func GetLedgerRange(cluster *gocql.ClusterConfig) (*Range, error) {
	session, err := cluster.CreateSession()
	if err != nil {
		return nil, err
	}
	defer session.Close()

	var (
		firstLedgerIdx  uint64
		latestLedgerIdx uint64
	)

	if err := session.Query("SELECT sequence FROM ledger_range WHERE is_latest = ?", false).Scan(&firstLedgerIdx); err != nil {
		return nil, err
	}

	if err := session.Query("SELECT sequence FROM ledger_range WHERE is_latest = ?", true).Scan(&latestLedgerIdx); err != nil {
		return nil, err
	}

	return &Range{
		FirstLedgerIdx:  firstLedgerIdx,
		LatestLedgerIdx: latestLedgerIdx,
	}, nil
}
