//
// Based off of https://github.com/scylladb/scylla-code-samples/blob/master/efficient_full_table_scan_example_code/efficient_full_table_scan.go
//

package main

import (
	"fmt"
	"log"
	"os"
	"strings"
	"time"
	"xrplf/clio/cassandra_delete_range/internal/cass"
	"xrplf/clio/cassandra_delete_range/internal/util"

	"github.com/alecthomas/kingpin/v2"
	"github.com/gocql/gocql"
)

const (
	defaultNumberOfNodesInCluster = 3
	defaultNumberOfCoresInNode    = 8
	defaultSmudgeFactor           = 3
)

var (
	app   = kingpin.New("cassandra_delete_range", "A tool that prunes data from the Clio DB.")
	hosts = app.Flag("hosts", "Your Scylla nodes IP addresses, comma separated (i.e. 192.168.1.1,192.168.1.2,192.168.1.3)").Required().String()

	deleteAfter          = app.Command("delete-after", "Prunes from the given ledger index until the end")
	deleteAfterLedgerIdx = deleteAfter.Arg("idx", "Sets the earliest ledger_index to keep untouched (delete everything after this ledger index)").Required().Uint64()

	deleteBefore          = app.Command("delete-before", "Prunes everything before the given ledger index")
	deleteBeforeLedgerIdx = deleteBefore.Arg("idx", "Sets the latest ledger_index to keep around (delete everything before this ledger index)").Required().Uint64()

	getLedgerRange = app.Command("get-ledger-range", "Fetch the current ledger_range table values")

	nodesInCluster        = app.Flag("nodes-in-cluster", "Number of nodes in your Scylla cluster").Short('n').Default(fmt.Sprintf("%d", defaultNumberOfNodesInCluster)).Int()
	coresInNode           = app.Flag("cores-in-node", "Number of cores in each node").Short('c').Default(fmt.Sprintf("%d", defaultNumberOfCoresInNode)).Int()
	smudgeFactor          = app.Flag("smudge-factor", "Yet another factor to make parallelism cooler").Short('s').Default(fmt.Sprintf("%d", defaultSmudgeFactor)).Int()
	clusterConsistency    = app.Flag("consistency", "Cluster consistency level. Use 'localone' for multi DC").Short('o').Default("localquorum").String()
	clusterTimeout        = app.Flag("timeout", "Maximum duration for query execution in millisecond").Short('t').Default("15000").Int()
	clusterNumConnections = app.Flag("cluster-number-of-connections", "Number of connections per host per session (in our case, per thread)").Short('b').Default("1").Int()
	clusterCQLVersion     = app.Flag("cql-version", "The CQL version to use").Short('l').Default("3.0.0").String()
	clusterPageSize       = app.Flag("cluster-page-size", "Page size of results").Short('p').Default("5000").Int()
	keyspace              = app.Flag("keyspace", "Keyspace to use").Short('k').Default("clio_fh").String()

	userName = app.Flag("username", "Username to use when connecting to the cluster").String()
	password = app.Flag("password", "Password to use when connecting to the cluster").String()

	skipSuccessorTable          = app.Flag("skip-successor", "Whether to skip deletion from successor table").Default("false").Bool()
	skipObjectsTable            = app.Flag("skip-objects", "Whether to skip deletion from objects table").Default("false").Bool()
	skipLedgerHashesTable       = app.Flag("skip-ledger-hashes", "Whether to skip deletion from ledger_hashes table").Default("false").Bool()
	skipTransactionsTable       = app.Flag("skip-transactions", "Whether to skip deletion from transactions table").Default("false").Bool()
	skipDiffTable               = app.Flag("skip-diff", "Whether to skip deletion from diff table").Default("false").Bool()
	skipLedgerTransactionsTable = app.Flag("skip-ledger-transactions", "Whether to skip deletion from ledger_transactions table").Default("false").Bool()
	skipLedgersTable            = app.Flag("skip-ledgers", "Whether to skip deletion from ledgers table").Default("false").Bool()
	skipWriteLatestLedger       = app.Flag("skip-write-latest-ledger", "Whether to skip writing the latest ledger index").Default("false").Bool()
	skipAccTransactionsTable    = app.Flag("skip-account-transactions", "Whether to skip deletion from account_transactions table").Default("false").Bool()

	workerCount = 1                // the calculated number of parallel goroutines the client should run
	ranges      []*util.TokenRange // the calculated ranges to be executed in parallel
)

func main() {
	log.SetOutput(os.Stdout)

	command := kingpin.MustParse(app.Parse(os.Args[1:]))
	cluster, err := prepareDb(hosts)
	if err != nil {
		log.Fatal(err)
	}

	clioCass := cass.NewClioCass(&cass.Settings{
		SkipSuccessorTable:          *skipSuccessorTable,
		SkipObjectsTable:            *skipObjectsTable,
		SkipLedgerHashesTable:       *skipLedgerHashesTable,
		SkipTransactionsTable:       *skipTransactionsTable,
		SkipDiffTable:               *skipDiffTable,
		SkipLedgerTransactionsTable: *skipLedgerHashesTable,
		SkipLedgersTable:            *skipLedgersTable,
		SkipWriteLatestLedger:       *skipWriteLatestLedger,
		SkipAccTransactionsTable:    *skipAccTransactionsTable,
		WorkerCount:                 workerCount,
		Ranges:                      ranges}, cluster)

	switch command {
	case deleteAfter.FullCommand():
		if *deleteAfterLedgerIdx == 0 {
			log.Println("Please specify ledger index to delete from")
			return
		}

		displayParams("delete-after", hosts, cluster.Timeout/1000/1000, *deleteAfterLedgerIdx)
		log.Printf("Will delete everything after ledger index %d (exclusive) and till latest\n", *deleteAfterLedgerIdx)
		log.Println("WARNING: Please make sure that there are no Clio writers operating on the DB while this script is running")

		if !util.PromptContinue() {
			log.Fatal("Aborted")
		}

		startTime := time.Now().UTC()
		clioCass.DeleteAfter(*deleteAfterLedgerIdx)

		fmt.Printf("Total Execution Time: %s\n\n", time.Since(startTime))
		fmt.Println("NOTE: Cassandra/ScyllaDB only writes tombstones. You need to run compaction to free up disk space.")

	case deleteBefore.FullCommand():
		if *deleteBeforeLedgerIdx == 0 {
			log.Println("Please specify ledger index to delete until")
			return
		}

		displayParams("delete-before", hosts, cluster.Timeout/1000/1000, *deleteBeforeLedgerIdx)
		log.Printf("Will delete everything before ledger index %d (exclusive)\n", *deleteBeforeLedgerIdx)
		log.Println("WARNING: Please make sure that there are no Clio writers operating on the DB while this script is running")

		if !util.PromptContinue() {
			log.Fatal("Aborted")
		}

		startTime := time.Now().UTC()
		clioCass.DeleteBefore(*deleteBeforeLedgerIdx)

		fmt.Printf("Total Execution Time: %s\n\n", time.Since(startTime))
		fmt.Println("NOTE: Cassandra/ScyllaDB only writes tombstones. You need to run compaction to free up disk space.")
	case getLedgerRange.FullCommand():
		from, to, err := clioCass.GetLedgerRange()
		if err != nil {
			log.Fatal(err)
		}

		fmt.Printf("Range: %d -> %d\n", from, to)
	}
}

func displayParams(command string, hosts *string, timeout time.Duration, ledgerIdx uint64) {
	runParameters := fmt.Sprintf(`
Execution Parameters:
=====================

Command                       : %s
Ledger index                  : %d
Scylla cluster nodes          : %s
Keyspace                      : %s
Consistency                   : %s
Timeout (ms)                  : %d
Connections per host          : %d
CQL Version                   : %s
Page size                     : %d
# of parallel threads         : %d
# of ranges to be executed    : %d

Skip deletion of:
- successor table             : %t
- objects table               : %t
- ledger_hashes table         : %t
- transactions table          : %t
- diff table                  : %t
- ledger_transactions table   : %t
- ledgers table               : %t

Will update ledger_range      : %t

`,
		command,
		ledgerIdx,
		*hosts,
		*keyspace,
		*clusterConsistency,
		timeout,
		*clusterNumConnections,
		*clusterCQLVersion,
		*clusterPageSize,
		workerCount,
		len(ranges),
		*skipSuccessorTable || command == "delete-before",
		*skipObjectsTable,
		*skipLedgerHashesTable,
		*skipTransactionsTable,
		*skipDiffTable,
		*skipLedgerTransactionsTable,
		*skipLedgersTable,
		!*skipWriteLatestLedger)

	fmt.Println(runParameters)
}

func prepareDb(dbHosts *string) (*gocql.ClusterConfig, error) {
	workerCount = (*nodesInCluster) * (*coresInNode) * (*smudgeFactor)
	ranges = util.GetTokenRanges(workerCount)
	util.Shuffle(ranges)

	hosts := strings.Split(*dbHosts, ",")

	cluster := gocql.NewCluster(hosts...)
	cluster.Consistency = util.GetConsistencyLevel(*clusterConsistency)
	cluster.Timeout = time.Duration(*clusterTimeout * 1000 * 1000)
	cluster.NumConns = *clusterNumConnections
	cluster.CQLVersion = *clusterCQLVersion
	cluster.PageSize = *clusterPageSize
	cluster.Keyspace = *keyspace

	if *userName != "" {
		cluster.Authenticator = gocql.PasswordAuthenticator{
			Username: *userName,
			Password: *password,
		}
	}

	return cluster, nil
}
