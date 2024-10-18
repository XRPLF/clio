//
// Based off of https://github.com/scylladb/scylla-code-samples/blob/master/efficient_full_table_scan_example_code/efficient_full_table_scan.go
//

package main

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"strconv"
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
	resume                = app.Flag("resume", "Whether to resume deletion from the previous command due to something crashing").Default("false").Bool()

	userName = app.Flag("username", "Username to use when connecting to the cluster").String()
	password = app.Flag("password", "Password to use when connecting to the cluster").String()

	skipSuccessorTable           = app.Flag("skip-successor", "Whether to skip deletion from successor table").Default("false").Bool()
	skipObjectsTable             = app.Flag("skip-objects", "Whether to skip deletion from objects table").Default("false").Bool()
	skipLedgerHashesTable        = app.Flag("skip-ledger-hashes", "Whether to skip deletion from ledger_hashes table").Default("false").Bool()
	skipTransactionsTable        = app.Flag("skip-transactions", "Whether to skip deletion from transactions table").Default("false").Bool()
	skipDiffTable                = app.Flag("skip-diff", "Whether to skip deletion from diff table").Default("false").Bool()
	skipLedgerTransactionsTable  = app.Flag("skip-ledger-transactions", "Whether to skip deletion from ledger_transactions table").Default("false").Bool()
	skipLedgersTable             = app.Flag("skip-ledgers", "Whether to skip deletion from ledgers table").Default("false").Bool()
	skipWriteLatestLedger        = app.Flag("skip-write-latest-ledger", "Whether to skip writing the latest ledger index").Default("false").Bool()
	skipAccTransactionsTable     = app.Flag("skip-account-transactions", "Whether to skip deletion from account_transactions table").Default("false").Bool()
	skipNFTokenTable             = app.Flag("skip-nf-tokens", "Whether to skip deletion from nf_tokens table").Default("false").Bool()
	skipIssuerNFTokenTable       = app.Flag("skip-issuer-nf-tokens-v2", "Whether to skip deletion from issuer_nf_tokens_v2 table").Default("false").Bool()
	skipNFTokenURITable          = app.Flag("skip-nf-tokens-uri", "Whether to skip deletion from nf_token_uris table").Default("false").Bool()
	skipNFTokenTransactionsTable = app.Flag("skip-nf-token-transactions", "Whether to skip deletion from nf_token_transactions table").Default("false").Bool()

	workerCount        = 1                // the calculated number of parallel goroutines the client should run
	ranges             []*util.TokenRange // the calculated ranges to be executed in parallel
	ledgerOrTokenRange *util.StoredRange  // mapping of startRange -> endRange. Used for resume deletion
)

func main() {
	log.SetOutput(os.Stdout)

	command := kingpin.MustParse(app.Parse(os.Args[1:]))
	cluster, err := prepareDb(hosts)
	if err != nil {
		log.Fatal(err)
	}

	cmd := strings.Join(os.Args[1:], " ")
	if *resume {
		prepareResume(&cmd)
	}

	clioCass := cass.NewClioCass(&cass.Settings{
		SkipSuccessorTable:           *skipSuccessorTable,
		SkipObjectsTable:             *skipObjectsTable,
		SkipLedgerHashesTable:        *skipLedgerHashesTable,
		SkipTransactionsTable:        *skipTransactionsTable,
		SkipDiffTable:                *skipDiffTable,
		SkipLedgerTransactionsTable:  *skipLedgerHashesTable,
		SkipLedgersTable:             *skipLedgersTable,
		SkipWriteLatestLedger:        *skipWriteLatestLedger,
		SkipAccTransactionsTable:     *skipAccTransactionsTable,
		SkipNFTokenTable:             *skipNFTokenTable,
		SkipIssuerNFTokenTable:       *skipIssuerNFTokenTable,
		SkipNFTokenURITable:          *skipNFTokenURITable,
		SkipNFTokenTransactionsTable: *skipNFTokenTransactionsTable,

		WorkerCount: workerCount,
		Ranges:      ranges,
		RangesRead:  ledgerOrTokenRange,
		Command:     cmd}, cluster)

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
- account_tx table            : %t
- nf_tokens table             : %t
- issuer_nf_tokens_v2 table   : %t
- nf_token_uris table         : %t
- nf_token_transactions table : %t

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
		*skipAccTransactionsTable,
		*skipNFTokenTable,
		*skipIssuerNFTokenTable,
		*skipNFTokenURITable,
		*skipNFTokenTransactionsTable,
		!*skipWriteLatestLedger,
	)

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

	// skips table if tables doesn't exist on earliest ledger
	return cluster, nil
}

func prepareResume(cmd *string) {
	// format of file continue.txt is
	/*
	 Previous user command (must match the same command to resume deletion)
	 Table name (ie. objects, ledger_hashes etc)
	 Values of token_ranges (each pair of values seperated line by line)
	*/

	file, err := os.Open("continue.txt")
	if err != nil {
		log.Fatal("continue.txt does not exist. Aborted")
	}
	defer file.Close()

	if err != nil {
		log.Fatalf("Failed to open file: %v", err)
	}
	scanner := bufio.NewScanner(file)
	scanner.Scan()

	// --resume must be last flag passed; so can check command matches
	if os.Args[len(os.Args)-1] != "--resume" {
		log.Fatal("--resume must be the last flag passed")
	}

	// get rid of --resume at the end
	*cmd = strings.Join(os.Args[1:len(os.Args)-1], " ")

	// makes sure command that got aborted matches the user command they enter
	if scanner.Text() != *cmd {
		log.Fatalf("File continue.txt has %s command stored. \n You provided %s which does not match. \n Aborting...", scanner.Text(), *cmd)
	}

	scanner.Scan()
	// skip the neccessary tables based on where the program aborted
	// for example if account_tx, all tables before account_tx
	// should be already deleted so we skip for deletion
	tableFound := false
	switch scanner.Text() {
	case "account_tx":
		*skipLedgersTable = true
		fallthrough
	case "ledgers":
		*skipLedgerTransactionsTable = true
		fallthrough
	case "ledger_transactions":
		*skipDiffTable = true
		fallthrough
	case "diff":
		*skipTransactionsTable = true
		fallthrough
	case "transactions":
		*skipLedgerHashesTable = true
		fallthrough
	case "ledger_hashes":
		*skipObjectsTable = true
		fallthrough
	case "objects":
		*skipSuccessorTable = true
		fallthrough
	case "successor":
		tableFound = true
	}

	if !tableFound {
		log.Fatalf("Invalid table: %s", scanner.Text())
	}

	scanner.Scan()
	rangeRead := make(map[int64]int64)

	// now go through all the ledger range and load it to a set
	for scanner.Scan() {
		line := scanner.Text()
		tokenRange := strings.Split(line, ",")
		if len(tokenRange) != 2 {
			log.Fatalf("Range is not two integers. %s . Aborting...", tokenRange)
		}
		startStr := strings.TrimSpace(tokenRange[0])
		endStr := strings.TrimSpace(tokenRange[1])

		// convert string to int64
		start, err1 := strconv.ParseInt(startStr, 10, 64)
		end, err2 := strconv.ParseInt(endStr, 10, 64)

		if err1 != nil || err2 != nil {
			log.Fatalf("Error converting integer: %s, %s", err1, err2)
		}
		rangeRead[start] = end
	}
	ledgerOrTokenRange = &util.StoredRange{}
	ledgerOrTokenRange.TokenRange = rangeRead
}
