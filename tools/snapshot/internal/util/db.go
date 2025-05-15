package util

import (
	"github.com/gocql/gocql"
)

func GetLedgerRange(cluster *gocql.ClusterConfig) (uint64, uint64, error) {
	var (
		firstLedgerIdx  uint64
		latestLedgerIdx uint64
	)

	session, err := cluster.CreateSession()
	if err != nil {
		return 0, 0, err
	}

	defer session.Close()

	if err := session.Query("SELECT sequence FROM ledger_range WHERE is_latest = ?", false).Scan(&firstLedgerIdx); err != nil {
		return 0, 0, err
	}

	// Query the latest ledger index
	if err := session.Query("SELECT sequence FROM ledger_range WHERE is_latest = ?", true).Scan(&latestLedgerIdx); err != nil {
		return 0, 0, err
	}

	return firstLedgerIdx, latestLedgerIdx, nil
}
