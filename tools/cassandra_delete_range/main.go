package main

import (
	"flag"
	"log"

	"github.com/gocql/gocql"
)

func main() {
	var (
		host              string
		port              int
		earliestLedgerIdx uint64
	)

	flag.StringVar(&host, "host", "127.0.0.1", "Sets the host to connect to Cassandra/ScyllaDB")
	flag.IntVar(&port, "port", 9042, "Sets the port to connect to Cassandra/ScyllaDB")
	flag.Uint64Var(&earliestLedgerIdx, "ledgerIdx", 0, "Sets the earliest ledger_index to keep untouched") // todo make required somehow
	flag.Parse()

	if earliestLedgerIdx == 0 {
		log.Println("Please specify ledger index to delete from")
		return
	}

	log.Printf("Connecting to DB at `%s:%d`\n", host, port)
	log.Printf("Will delete everything from ledger index %d and till latest\n", earliestLedgerIdx)
	log.Println("Are you sure you want to continue? (y/n)")

	// var continueFlag string
	// if fmt.Scanln(&continueFlag); continueFlag != "y" {
	// 	log.Println("Aborting...")
	// 	return
	// }

	deletedLedgerCount, err := performDeletion(host, port, earliestLedgerIdx)
	if err != nil {
		log.Printf("Error: %v\n", err)
		return
	}

	log.Printf("Deleted %d ledgers\n", deletedLedgerCount)
}

func performDeletion(host string, port int, earliestLedgerIdxToKeep uint64) (uint, error) {
	var ledgersDeleted uint = 0

	cluster := gocql.NewCluster(host)
	cluster.Keyspace = "clio_new"
	cluster.Port = port
	cluster.ProtoVersion = 4
	cluster.Consistency = gocql.LocalQuorum
	cluster.PageSize = 1000

	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}

	defer session.Close()

	earliestLedgerIdxInDB, latestLedgerIdxInDB, err := getLedgerRange(session)
	if err != nil {
		log.Fatal(err)
	}

	if earliestLedgerIdxInDB > earliestLedgerIdxToKeep {
		log.Fatal("Earliest ledger index in DB is greater than the one specified. Aborting...")
	}

	if latestLedgerIdxInDB < earliestLedgerIdxToKeep {
		log.Fatal("Latest ledger index in DB is smaller than the one specified. Aborting...")
	}

	if err := deleteLedgerData(session, earliestLedgerIdxToKeep+1, latestLedgerIdxInDB); err != nil {
		log.Fatal(err)
	}

	return ledgersDeleted, nil
}

func getLedgerRange(session *gocql.Session) (uint64, uint64, error) {
	var (
		earliestLedgerIdx uint64
		latestLedgerIdx   uint64
	)

	if err := session.Query("select sequence from ledger_range where is_latest = ?", false).Scan(&earliestLedgerIdx); err != nil {
		return 0, 0, err
	}

	if err := session.Query("select sequence from ledger_range where is_latest = ?", true).Scan(&latestLedgerIdx); err != nil {
		return 0, 0, err
	}

	log.Printf("DB ledger range is %d:%d\n", earliestLedgerIdx, latestLedgerIdx)
	return earliestLedgerIdx, latestLedgerIdx, nil
}

func deleteLedgerData(session *gocql.Session, fromLedgerIdx uint64, toLedgerIdx uint64) error {
	log.Printf("Start deleting %d -> %d\n", fromLedgerIdx, toLedgerIdx)

	if err := deleteObjects(session, fromLedgerIdx, toLedgerIdx); err != nil {
		return err
	}

	if err := deleteSuccessors(session, fromLedgerIdx, toLedgerIdx); err != nil {
		return err
	}

	log.Printf("Finished deleting %d -> %d\n", fromLedgerIdx, toLedgerIdx)
	return nil
}

func deleteObjects(session *gocql.Session, fromLedgerIdx uint64, toLedgerIdx uint64) error {
	var pageState []byte

	type ToDelete struct {
		key      []byte
		sequence uint64
	}
	var toDelete []ToDelete

	for {
		iter := session.Query("select key, sequence from objects").PageState(pageState).Iter()
		nextPageState := iter.PageState()
		scanner := iter.Scanner()

		for scanner.Next() {
			var key []byte
			var sequence uint64

			if err := scanner.Scan(&key, &sequence); err != nil {
				return err
			}

			if sequence >= fromLedgerIdx && sequence <= toLedgerIdx {
				toDelete = append(toDelete, ToDelete{key, sequence})
				// log.Printf("Will delete key %x in %d\n", key, sequence)
			}
		}

		if err := scanner.Err(); err != nil {
			return err
		}

		if len(nextPageState) == 0 {
			break
		}

		pageState = nextPageState
	}

	log.Printf("Will delete %d objects\n", len(toDelete))

	return nil
}

func deleteSuccessors(session *gocql.Session, fromLedgerIdx uint64, toLedgerIdx uint64) error {
	var pageState []byte

	type ToDelete struct {
		key      []byte
		sequence uint64
	}
	var toDelete []ToDelete

	for {
		iter := session.Query("select key, seq from successor").PageState(pageState).Iter()
		nextPageState := iter.PageState()
		scanner := iter.Scanner()

		for scanner.Next() {
			var key []byte
			var sequence uint64

			if err := scanner.Scan(&key, &sequence); err != nil {
				return err
			}

			if sequence >= fromLedgerIdx && sequence <= toLedgerIdx {
				toDelete = append(toDelete, ToDelete{key, sequence})
				// log.Printf("Will delete key %x in %d\n", key, sequence)
			}
		}

		if err := scanner.Err(); err != nil {
			return err
		}

		if len(nextPageState) == 0 {
			break
		}

		pageState = nextPageState
	}

	log.Printf("Will delete %d successors\n", len(toDelete))

	return nil
}
