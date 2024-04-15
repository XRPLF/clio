//
// Based off of https://github.com/scylladb/scylla-code-samples/blob/master/efficient_full_table_scan_example_code/efficient_full_table_scan.go
//

package main

import (
	"fmt"
	"log"
	"math"
	"math/rand"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/alecthomas/kingpin/v2"
	"github.com/gocql/gocql"
)

const (
	defaultNumberOfNodesInCluster = 3
	defaultNumberOfCoresInNode    = 8
	defaultSmudgeFactor           = 3
)

var (
	clusterHosts      = kingpin.Arg("hosts", "Your Scylla nodes IP addresses, comma separated (i.e. 192.168.1.1,192.168.1.2,192.168.1.3)").Required().String()
	earliestLedgerIdx = kingpin.Flag("ledgerIdx", "Sets the earliest ledger_index to keep untouched").Short('i').Required().Uint64()

	nodesInCluster        = kingpin.Flag("nodes-in-cluster", "Number of nodes in your Scylla cluster").Short('n').Default(fmt.Sprintf("%d", defaultNumberOfNodesInCluster)).Int()
	coresInNode           = kingpin.Flag("cores-in-node", "Number of cores in each node").Short('c').Default(fmt.Sprintf("%d", defaultNumberOfCoresInNode)).Int()
	smudgeFactor          = kingpin.Flag("smudge-factor", "Yet another factor to make parallelism cooler").Short('s').Default(fmt.Sprintf("%d", defaultSmudgeFactor)).Int()
	clusterConsistency    = kingpin.Flag("consistency", "Cluster consistency level. Use 'localone' for multi DC").Short('o').Default("localquorum").String()
	clusterTimeout        = kingpin.Flag("timeout", "Maximum duration for query execution in millisecond").Short('t').Default("15000").Int()
	clusterNumConnections = kingpin.Flag("cluster-number-of-connections", "Number of connections per host per session (in our case, per thread)").Short('b').Default("1").Int()
	clusterCQLVersion     = kingpin.Flag("cql-version", "The CQL version to use").Short('l').Default("3.0.0").String()
	clusterPageSize       = kingpin.Flag("cluster-page-size", "Page size of results").Short('p').Default("5000").Int()
	keyspace              = kingpin.Flag("keyspace", "Keyspace to use").Short('k').Default("clio_fh").String()

	userName = kingpin.Flag("username", "Username to use when connecting to the cluster").String()
	password = kingpin.Flag("password", "Password to use when connecting to the cluster").String()

	skipSuccessorTable          = kingpin.Flag("skip-successor", "Whether to skip deletion from successor table").Default("false").Bool()
	skipObjectsTable            = kingpin.Flag("skip-objects", "Whether to skip deletion from objects table").Default("false").Bool()
	skipLedgerHashesTable       = kingpin.Flag("skip-ledger-hashes", "Whether to skip deletion from ledger_hashes table").Default("false").Bool()
	skipTransactionsTable       = kingpin.Flag("skip-transactions", "Whether to skip deletion from transactions table").Default("false").Bool()
	skipDiffTable               = kingpin.Flag("skip-diff", "Whether to skip deletion from diff table").Default("false").Bool()
	skipLedgerTransactionsTable = kingpin.Flag("skip-ledger-transactions", "Whether to skip deletion from ledger_transactions table").Default("false").Bool()
	skipLedgersTable            = kingpin.Flag("skip-ledgers", "Whether to skip deletion from ledgers table").Default("false").Bool()
	skipWriteLatestLedger       = kingpin.Flag("skip-write-latest-ledger", "Whether to skip writing the latest ledger index").Default("false").Bool()

	workerCount = 1           // the calculated number of parallel goroutines the client should run
	ranges      []*tokenRange // the calculated ranges to be executed in parallel
)

type tokenRange struct {
	StartRange int64
	EndRange   int64
}

type deleteParams struct {
	Seq  uint64
	Blob []byte // hash, key, etc
}

type columnSettings struct {
	UseSeq  bool
	UseBlob bool
}

type deleteInfo struct {
	Query string
	Data  []deleteParams
}

func getTokenRanges() []*tokenRange {
	var n = workerCount
	var m = int64(n * 100)
	var maxSize uint64 = math.MaxInt64 * 2
	var rangeSize = maxSize / uint64(m)

	var start int64 = math.MinInt64
	var end int64
	var shouldBreak = false

	var ranges = make([]*tokenRange, m)

	for i := int64(0); i < m; i++ {
		end = start + int64(rangeSize)
		if start > 0 && end < 0 {
			end = math.MaxInt64
			shouldBreak = true
		}

		ranges[i] = &tokenRange{StartRange: start, EndRange: end}

		if shouldBreak {
			break
		}

		start = end + 1
	}

	return ranges
}

func splitDeleteWork(info *deleteInfo) [][]deleteParams {
	var n = workerCount
	var chunkSize = len(info.Data) / n
	var chunks [][]deleteParams

	if len(info.Data) == 0 {
		return chunks
	}

	if chunkSize < 1 {
		chunks = append(chunks, info.Data)
		return chunks
	}

	for i := 0; i < len(info.Data); i += chunkSize {
		end := i + chunkSize

		if end > len(info.Data) {
			end = len(info.Data)
		}

		chunks = append(chunks, info.Data[i:end])
	}

	return chunks
}

func shuffle(data []*tokenRange) {
	for i := 1; i < len(data); i++ {
		r := rand.Intn(i + 1)
		if i != r {
			data[r], data[i] = data[i], data[r]
		}
	}
}

func getConsistencyLevel(consistencyValue string) gocql.Consistency {
	switch consistencyValue {
	case "any":
		return gocql.Any
	case "one":
		return gocql.One
	case "two":
		return gocql.Two
	case "three":
		return gocql.Three
	case "quorum":
		return gocql.Quorum
	case "all":
		return gocql.All
	case "localquorum":
		return gocql.LocalQuorum
	case "eachquorum":
		return gocql.EachQuorum
	case "localone":
		return gocql.LocalOne
	default:
		return gocql.One
	}
}

func main() {
	log.SetOutput(os.Stdout)
	kingpin.Parse()

	workerCount = (*nodesInCluster) * (*coresInNode) * (*smudgeFactor)
	ranges = getTokenRanges()
	shuffle(ranges)

	hosts := strings.Split(*clusterHosts, ",")

	cluster := gocql.NewCluster(hosts...)
	cluster.Consistency = getConsistencyLevel(*clusterConsistency)
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

	if *earliestLedgerIdx == 0 {
		log.Println("Please specify ledger index to delete from")
		return
	}

	runParameters := fmt.Sprintf(`
Execution Parameters:
=====================

Range to be deleted           : %d -> latest
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

Will rite latest ledger       : %t

`,
		*earliestLedgerIdx,
		*clusterHosts,
		*keyspace,
		*clusterConsistency,
		cluster.Timeout/1000/1000,
		*clusterNumConnections,
		*clusterCQLVersion,
		*clusterPageSize,
		workerCount,
		len(ranges),
		*skipSuccessorTable,
		*skipObjectsTable,
		*skipLedgerHashesTable,
		*skipTransactionsTable,
		*skipDiffTable,
		*skipLedgerTransactionsTable,
		*skipLedgersTable,
		!*skipWriteLatestLedger)

	fmt.Println(runParameters)

	log.Printf("Will delete everything after ledger index %d (exclusive) and till latest\n", *earliestLedgerIdx)
	log.Println("WARNING: Please make sure that there are no Clio writers operating on the DB while this script is running")
	log.Println("Are you sure you want to continue? (y/n)")

	var continueFlag string
	if fmt.Scanln(&continueFlag); continueFlag != "y" {
		log.Println("Aborting...")
		return
	}

	startTime := time.Now().UTC()

	earliestLedgerIdxInDB, latestLedgerIdxInDB, err := getLedgerRange(cluster)
	if err != nil {
		log.Fatal(err)
	}

	if earliestLedgerIdxInDB > *earliestLedgerIdx {
		log.Fatal("Earliest ledger index in DB is greater than the one specified. Aborting...")
	}

	if latestLedgerIdxInDB < *earliestLedgerIdx {
		log.Fatal("Latest ledger index in DB is smaller than the one specified. Aborting...")
	}

	if err := deleteLedgerData(cluster, *earliestLedgerIdx+1, latestLedgerIdxInDB); err != nil {
		log.Fatal(err)
	}

	fmt.Printf("Total Execution Time: %s\n\n", time.Since(startTime))
	fmt.Println("NOTE: Cassandra/ScyllaDB only writes tombstones. You need to run compaction to free up disk space.")
}

func getLedgerRange(cluster *gocql.ClusterConfig) (uint64, uint64, error) {
	var (
		firstLedgerIdx  uint64
		latestLedgerIdx uint64
	)

	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}

	defer session.Close()

	if err := session.Query("select sequence from ledger_range where is_latest = ?", false).Scan(&firstLedgerIdx); err != nil {
		return 0, 0, err
	}

	if err := session.Query("select sequence from ledger_range where is_latest = ?", true).Scan(&latestLedgerIdx); err != nil {
		return 0, 0, err
	}

	log.Printf("DB ledger range is %d:%d\n", firstLedgerIdx, latestLedgerIdx)
	return firstLedgerIdx, latestLedgerIdx, nil
}

func deleteLedgerData(cluster *gocql.ClusterConfig, fromLedgerIdx uint64, toLedgerIdx uint64) error {
	var totalErrors uint64
	var totalRows uint64
	var totalDeletes uint64

	var info deleteInfo
	var rowsCount uint64
	var deleteCount uint64
	var errCount uint64

	log.Printf("Start scanning and removing data for %d -> latest (%d according to ledger_range table)\n\n", fromLedgerIdx, toLedgerIdx)

	// successor queries
	if !*skipSuccessorTable {
		log.Println("Generating delete queries for successor table")
		info, rowsCount, errCount = prepareDeleteQueries(cluster, fromLedgerIdx,
			"SELECT key, seq FROM successor WHERE token(key) >= ? AND token(key) <= ?",
			"DELETE FROM successor WHERE key = ? AND seq = ?")
		log.Printf("Total delete queries: %d\n", len(info.Data))
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalErrors += errCount
		totalRows += rowsCount
		deleteCount, errCount = performDeleteQueries(cluster, &info, columnSettings{UseBlob: true, UseSeq: true})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// objects queries
	if !*skipObjectsTable {
		log.Println("Generating delete queries for objects table")
		info, rowsCount, errCount = prepareDeleteQueries(cluster, fromLedgerIdx,
			"SELECT key, sequence FROM objects WHERE token(key) >= ? AND token(key) <= ?",
			"DELETE FROM objects WHERE key = ? AND sequence = ?")
		log.Printf("Total delete queries: %d\n", len(info.Data))
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalErrors += errCount
		totalRows += rowsCount
		deleteCount, errCount = performDeleteQueries(cluster, &info, columnSettings{UseBlob: true, UseSeq: true})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// ledger_hashes queries
	if !*skipLedgerHashesTable {
		log.Println("Generating delete queries for ledger_hashes table")
		info, rowsCount, errCount = prepareDeleteQueries(cluster, fromLedgerIdx,
			"SELECT hash, sequence FROM ledger_hashes WHERE token(hash) >= ? AND token(hash) <= ?",
			"DELETE FROM ledger_hashes WHERE hash = ?")
		log.Printf("Total delete queries: %d\n", len(info.Data))
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalErrors += errCount
		totalRows += rowsCount
		deleteCount, errCount = performDeleteQueries(cluster, &info, columnSettings{UseBlob: true, UseSeq: false})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// transactions queries
	if !*skipTransactionsTable {
		log.Println("Generating delete queries for transactions table")
		info, rowsCount, errCount = prepareDeleteQueries(cluster, fromLedgerIdx,
			"SELECT hash, ledger_sequence FROM transactions WHERE token(hash) >= ? AND token(hash) <= ?",
			"DELETE FROM transactions WHERE hash = ?")
		log.Printf("Total delete queries: %d\n", len(info.Data))
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalErrors += errCount
		totalRows += rowsCount
		deleteCount, errCount = performDeleteQueries(cluster, &info, columnSettings{UseBlob: true, UseSeq: false})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// diff queries
	if !*skipDiffTable {
		log.Println("Generating delete queries for diff table")
		info = prepareSimpleDeleteQueries(fromLedgerIdx, toLedgerIdx,
			"DELETE FROM diff WHERE seq = ?")
		log.Printf("Total delete queries: %d\n\n", len(info.Data))
		deleteCount, errCount = performDeleteQueries(cluster, &info, columnSettings{UseBlob: true, UseSeq: true})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// ledger_transactions queries
	if !*skipLedgerTransactionsTable {
		log.Println("Generating delete queries for ledger_transactions table")
		info = prepareSimpleDeleteQueries(fromLedgerIdx, toLedgerIdx,
			"DELETE FROM ledger_transactions WHERE ledger_sequence = ?")
		log.Printf("Total delete queries: %d\n\n", len(info.Data))
		deleteCount, errCount = performDeleteQueries(cluster, &info, columnSettings{UseBlob: false, UseSeq: true})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// ledgers queries
	if !*skipLedgersTable {
		log.Println("Generating delete queries for ledgers table")
		info = prepareSimpleDeleteQueries(fromLedgerIdx, toLedgerIdx,
			"DELETE FROM ledgers WHERE sequence = ?")
		log.Printf("Total delete queries: %d\n\n", len(info.Data))
		deleteCount, errCount = performDeleteQueries(cluster, &info, columnSettings{UseBlob: false, UseSeq: true})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// TODO: tbd what to do with account_tx as it got tuple for seq_idx
	// TODO: also, whether we need to take care of nft tables and other stuff like that

	if !*skipWriteLatestLedger {
		if err := updateLedgerRange(cluster, fromLedgerIdx-1); err != nil {
			log.Printf("ERROR failed updating ledger range: %s\n", err)
			return err
		}

		log.Printf("Updated latest ledger to %d in ledger_range table\n\n", fromLedgerIdx-1)
	}

	log.Printf("TOTAL ERRORS: %d\n", totalErrors)
	log.Printf("TOTAL ROWS TRAVERSED: %d\n", totalRows)
	log.Printf("TOTAL DELETES: %d\n\n", totalDeletes)

	log.Printf("Completed deletion for %d -> %d\n\n", fromLedgerIdx, toLedgerIdx)

	return nil
}

func prepareSimpleDeleteQueries(fromLedgerIdx uint64, toLedgerIdx uint64, deleteQueryTemplate string) deleteInfo {
	var info = deleteInfo{Query: deleteQueryTemplate}

	// Note: we deliberately add 1 extra ledger to make sure we delete any data Clio might have written
	// if it crashed or was stopped in the middle of writing just before it wrote ledger_range.
	for i := fromLedgerIdx; i <= toLedgerIdx+1; i++ {
		info.Data = append(info.Data, deleteParams{Seq: i})
	}

	return info
}

func prepareDeleteQueries(cluster *gocql.ClusterConfig, fromLedgerIdx uint64, queryTemplate string, deleteQueryTemplate string) (deleteInfo, uint64, uint64) {
	rangesChannel := make(chan *tokenRange, len(ranges))
	for i := range ranges {
		rangesChannel <- ranges[i]
	}

	close(rangesChannel)

	outChannel := make(chan deleteParams)
	var info = deleteInfo{Query: deleteQueryTemplate}

	go func() {
		for params := range outChannel {
			info.Data = append(info.Data, params)
		}
	}()

	var wg sync.WaitGroup
	var sessionCreationWaitGroup sync.WaitGroup
	var totalRows uint64
	var totalErrors uint64

	wg.Add(workerCount)
	sessionCreationWaitGroup.Add(workerCount)

	for i := 0; i < workerCount; i++ {
		go func(q string) {
			defer wg.Done()

			var session *gocql.Session
			var err error
			if session, err = cluster.CreateSession(); err == nil {
				defer session.Close()

				sessionCreationWaitGroup.Done()
				sessionCreationWaitGroup.Wait()
				preparedQuery := session.Query(q)

				for r := range rangesChannel {
					preparedQuery.Bind(r.StartRange, r.EndRange)

					var pageState []byte
					var rowsRetrieved uint64

					for {
						iter := preparedQuery.PageSize(*clusterPageSize).PageState(pageState).Iter()
						nextPageState := iter.PageState()
						scanner := iter.Scanner()

						for scanner.Next() {
							var key []byte
							var seq uint64

							err = scanner.Scan(&key, &seq)
							if err == nil {
								rowsRetrieved++

								// only grab the rows that are in the correct range of sequence numbers
								if fromLedgerIdx <= seq {
									outChannel <- deleteParams{Seq: seq, Blob: key}
								}
							} else {
								log.Printf("ERROR: page iteration failed: %s\n", err)
								fmt.Fprintf(os.Stderr, "FAILED QUERY: %s\n", fmt.Sprintf("%s [from=%d][to=%d][pagestate=%x]", queryTemplate, r.StartRange, r.EndRange, pageState))
								atomic.AddUint64(&totalErrors, 1)
							}
						}

						if len(nextPageState) == 0 {
							break
						}

						pageState = nextPageState
					}

					atomic.AddUint64(&totalRows, rowsRetrieved)
				}
			} else {
				log.Printf("ERROR: %s\n", err)
				fmt.Fprintf(os.Stderr, "FAILED TO CREATE SESSION: %s\n", err)
				atomic.AddUint64(&totalErrors, 1)
			}
		}(queryTemplate)
	}

	wg.Wait()
	close(outChannel)

	return info, totalRows, totalErrors
}

func performDeleteQueries(cluster *gocql.ClusterConfig, info *deleteInfo, colSettings columnSettings) (uint64, uint64) {
	var wg sync.WaitGroup
	var sessionCreationWaitGroup sync.WaitGroup
	var totalDeletes uint64
	var totalErrors uint64

	chunks := splitDeleteWork(info)
	chunksChannel := make(chan []deleteParams, len(chunks))
	for i := range chunks {
		chunksChannel <- chunks[i]
	}

	close(chunksChannel)

	wg.Add(workerCount)
	sessionCreationWaitGroup.Add(workerCount)

	query := info.Query
	bindCount := strings.Count(query, "?")

	for i := 0; i < workerCount; i++ {
		go func(number int, q string, bc int) {
			defer wg.Done()

			var session *gocql.Session
			var err error
			if session, err = cluster.CreateSession(); err == nil {
				defer session.Close()

				sessionCreationWaitGroup.Done()
				sessionCreationWaitGroup.Wait()
				preparedQuery := session.Query(q)

				for chunk := range chunksChannel {
					for _, r := range chunk {
						if bc == 2 {
							preparedQuery.Bind(r.Blob, r.Seq)
						} else if bc == 1 {
							if colSettings.UseSeq {
								preparedQuery.Bind(r.Seq)
							} else if colSettings.UseBlob {
								preparedQuery.Bind(r.Blob)
							}
						}

						if err := preparedQuery.Exec(); err != nil {
							log.Printf("DELETE ERROR: %s\n", err)
							fmt.Fprintf(os.Stderr, "FAILED QUERY: %s\n", fmt.Sprintf("%s [blob=0x%x][seq=%d]", info.Query, r.Blob, r.Seq))
							atomic.AddUint64(&totalErrors, 1)
						} else {
							atomic.AddUint64(&totalDeletes, 1)
						}
					}
				}
			} else {
				log.Printf("ERROR: %s\n", err)
				fmt.Fprintf(os.Stderr, "FAILED TO CREATE SESSION: %s\n", err)
				atomic.AddUint64(&totalErrors, 1)
			}
		}(i, query, bindCount)
	}

	wg.Wait()
	return totalDeletes, totalErrors
}

func updateLedgerRange(cluster *gocql.ClusterConfig, ledgerIndex uint64) error {
	log.Printf("Updating latest ledger to %d\n", ledgerIndex)

	if session, err := cluster.CreateSession(); err == nil {
		defer session.Close()

		query := "UPDATE ledger_range SET sequence = ? WHERE is_latest = ?"
		preparedQuery := session.Query(query, ledgerIndex, true)
		if err := preparedQuery.Exec(); err != nil {
			fmt.Fprintf(os.Stderr, "FAILED QUERY: %s [seq=%d][true]\n", query, ledgerIndex)
			return err
		}
	} else {
		fmt.Fprintf(os.Stderr, "FAILED TO CREATE SESSION: %s\n", err)
		return err
	}

	return nil
}
