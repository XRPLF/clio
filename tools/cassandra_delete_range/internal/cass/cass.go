package cass

import (
	"fmt"
	"log"
	"os"
	"slices"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"xrplf/clio/cassandra_delete_range/internal/util"

	"github.com/gocql/gocql"
	"github.com/pmorelli92/maybe"
)

type deleteInfo struct {
	Query string
	Data  []deleteParams
}

type deleteParams struct {
	Seq      uint64
	Blob     []byte // hash, key, etc
	tnxIndex uint64 //transaction index
}

type columnSettings struct {
	UseSeq  bool
	UseBlob bool
}

type deleteMethod struct {
	deleteObject      maybe.Maybe[bool]
	deleteTransaction maybe.Maybe[bool]
	deleteGeneral     maybe.Maybe[bool]
}

type Settings struct {
	SkipSuccessorTable          bool
	SkipObjectsTable            bool
	SkipLedgerHashesTable       bool
	SkipTransactionsTable       bool
	SkipDiffTable               bool
	SkipLedgerTransactionsTable bool
	SkipLedgersTable            bool
	SkipWriteLatestLedger       bool
	SkipAccTransactionsTable    bool

	WorkerCount int
	Ranges      []*util.TokenRange
	RangesRead  *util.StoredRange // Used to resume deletion
	Command     string
}

type Cass interface {
	GetLedgerRange() (uint64, uint64, error)
	DeleteBefore(ledgerIdx uint64)
	DeleteAfter(ledgerIdx uint64)
}

type ClioCass struct {
	settings      *Settings
	clusterConfig *gocql.ClusterConfig
}

func NewClioCass(settings *Settings, cluster *gocql.ClusterConfig) *ClioCass {
	return &ClioCass{settings, cluster}
}

func (c *ClioCass) DeleteBefore(ledgerIdx uint64) {
	firstLedgerIdxInDB, latestLedgerIdxInDB, err := c.GetLedgerRange()
	if err != nil {
		log.Fatal(err)
	}

	log.Printf("DB ledger range is %d -> %d\n", firstLedgerIdxInDB, latestLedgerIdxInDB)

	if firstLedgerIdxInDB >= ledgerIdx {
		log.Fatal("Earliest ledger index in DB is greater than the one specified. Aborting...")
	}

	if latestLedgerIdxInDB < ledgerIdx {
		log.Fatal("Latest ledger index in DB is smaller than the one specified. Aborting...")
	}

	var (
		from maybe.Maybe[uint64] // not used
		to   maybe.Maybe[uint64] = maybe.Set(ledgerIdx - 1)
	)

	c.settings.SkipSuccessorTable = true // skip successor update until we know how to do it
	if err := c.pruneData(from, to, firstLedgerIdxInDB, latestLedgerIdxInDB); err != nil {
		log.Fatal(err)
	}
}

func (c *ClioCass) DeleteAfter(ledgerIdx uint64) {
	firstLedgerIdxInDB, latestLedgerIdxInDB, err := c.GetLedgerRange()
	if err != nil {
		log.Fatal(err)
	}

	log.Printf("DB ledger range is %d -> %d\n", firstLedgerIdxInDB, latestLedgerIdxInDB)

	if firstLedgerIdxInDB > ledgerIdx {
		log.Fatal("Earliest ledger index in DB is greater than the one specified. Aborting...")
	}

	if latestLedgerIdxInDB <= ledgerIdx {
		log.Fatal("Latest ledger index in DB is smaller than the one specified. Aborting...")
	}

	var (
		from maybe.Maybe[uint64] = maybe.Set(ledgerIdx + 1)
		to   maybe.Maybe[uint64] // not used
	)

	if err := c.pruneData(from, to, firstLedgerIdxInDB, latestLedgerIdxInDB); err != nil {
		log.Fatal(err)
	}
}

func (c *ClioCass) GetLedgerRange() (uint64, uint64, error) {
	var (
		firstLedgerIdx  uint64
		latestLedgerIdx uint64
	)

	session, err := c.clusterConfig.CreateSession()
	if err != nil {
		log.Fatal(err)
	}

	defer session.Close()

	if err := session.Query("SELECT sequence FROM ledger_range WHERE is_latest = ?", false).Scan(&firstLedgerIdx); err != nil {
		return 0, 0, err
	}

	if err := session.Query("SELECT sequence FROM ledger_range WHERE is_latest = ?", true).Scan(&latestLedgerIdx); err != nil {
		return 0, 0, err
	}

	return firstLedgerIdx, latestLedgerIdx, nil
}

func (c *ClioCass) pruneData(
	fromLedgerIdx maybe.Maybe[uint64],
	toLedgerIdx maybe.Maybe[uint64],
	firstLedgerIdxInDB uint64,
	latestLedgerIdxInDB uint64,
) error {
	var totalErrors uint64
	var totalRows uint64
	var totalDeletes uint64

	var rowsCount uint64
	var deleteCount uint64
	var errCount uint64

	// calculate range of simple delete queries
	var (
		rangeFrom uint64 = firstLedgerIdxInDB
		rangeTo   uint64 = latestLedgerIdxInDB
	)

	if fromLedgerIdx.HasValue() {
		rangeFrom = fromLedgerIdx.Value()
	}

	if toLedgerIdx.HasValue() {
		rangeTo = toLedgerIdx.Value()
	}

	// calculate and print deletion plan
	fromStr := "beginning"
	if fromLedgerIdx.HasValue() {
		fromStr = strconv.Itoa(int(fromLedgerIdx.Value()))
	}

	toStr := "latest"
	if toLedgerIdx.HasValue() {
		toStr = strconv.Itoa(int(toLedgerIdx.Value()))
	}

	log.Printf("Start scanning and removing data for %s -> %s\n\n", fromStr, toStr)

	// successor queries
	if !c.settings.SkipSuccessorTable {
		file, err := createAndWriteToFile("successor", &c.settings.Command)
		if err != nil {
			return err
		}
		defer file.Close()

		log.Println("Generating delete queries for successor table")
		rowsCount, deleteCount, errCount = c.prepareAndExecuteDeleteQueries(file, fromLedgerIdx, toLedgerIdx,
			"SELECT key, seq FROM successor WHERE token(key) >= ? AND token(key) <= ?",
			"DELETE FROM successor WHERE key = ? AND seq = ?", deleteMethod{deleteGeneral: maybe.Set(true)}, columnSettings{UseBlob: true, UseSeq: false})
		log.Printf("Total delete queries: %d\n", deleteCount)
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalRows += rowsCount
		totalErrors += errCount
		totalDeletes += deleteCount

		os.Remove("continue.txt")
	}

	// objects queries
	if !c.settings.SkipObjectsTable {
		file, err := createAndWriteToFile("objects", &c.settings.Command)
		if err != nil {
			return err
		}
		defer file.Close()

		log.Println("Generating delete queries for objects table")
		rowsCount, deleteCount, errCount = c.prepareAndExecuteDeleteQueries(file, fromLedgerIdx, toLedgerIdx,
			"SELECT key, sequence FROM objects WHERE token(key) >= ? AND token(key) <= ?",
			"DELETE FROM objects WHERE key = ? AND sequence = ?", deleteMethod{deleteObject: maybe.Set(true)}, columnSettings{UseBlob: true, UseSeq: true})
		log.Printf("Total delete queries: %d\n", deleteCount)
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalErrors += errCount
		totalRows += rowsCount
		totalDeletes += deleteCount

		os.Remove("continue.txt")
	}

	// ledger_hashes queries
	if !c.settings.SkipLedgerHashesTable {
		file, err := createAndWriteToFile("ledger_hashes", &c.settings.Command)
		if err != nil {
			return err
		}
		defer file.Close()

		log.Println("Generating delete queries for ledger_hashes table")
		rowsCount, deleteCount, errCount = c.prepareAndExecuteDeleteQueries(file, fromLedgerIdx, toLedgerIdx,
			"SELECT hash, sequence FROM ledger_hashes WHERE token(hash) >= ? AND token(hash) <= ?",
			"DELETE FROM ledger_hashes WHERE hash = ?", deleteMethod{deleteGeneral: maybe.Set(true)}, columnSettings{UseBlob: true, UseSeq: false})
		log.Printf("Total delete queries: %d\n", deleteCount)
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalRows += rowsCount
		totalErrors += errCount
		totalDeletes += deleteCount

		os.Remove("continue.txt")
	}

	// transactions queries
	if !c.settings.SkipTransactionsTable {
		file, err := createAndWriteToFile("transactions", &c.settings.Command)
		if err != nil {
			return err
		}
		defer file.Close()

		log.Println("Generating delete queries for transactions table")
		rowsCount, deleteCount, errCount = c.prepareAndExecuteDeleteQueries(file, fromLedgerIdx, toLedgerIdx,
			"SELECT hash, ledger_sequence FROM transactions WHERE token(hash) >= ? AND token(hash) <= ?",
			"DELETE FROM transactions WHERE hash = ?", deleteMethod{deleteGeneral: maybe.Set(true)}, columnSettings{UseBlob: true, UseSeq: false})
		log.Printf("Total delete queries: %d\n", deleteCount)
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalRows += rowsCount
		totalErrors += errCount
		totalDeletes += deleteCount

		os.Remove("continue.txt")
	}

	// diff queries
	if !c.settings.SkipDiffTable {
		file, err := createAndWriteToFile("diff", &c.settings.Command)
		if err != nil {
			return err
		}
		defer file.Close()

		log.Println("Generating delete queries for diff table")
		deleteCount, errCount = c.prepareAndExecuteSimpleDeleteQueries(rangeFrom, rangeTo,
			"DELETE FROM diff WHERE seq = ?", columnSettings{UseBlob: false, UseSeq: true})
		log.Printf("Total delete queries: %d\n\n", deleteCount)
		totalErrors += errCount
		totalDeletes += deleteCount

		os.Remove("continue.txt")
	}

	// ledger_transactions queries
	if !c.settings.SkipLedgerTransactionsTable {
		file, err := createAndWriteToFile("ledger_transactions", &c.settings.Command)
		if err != nil {
			return err
		}
		defer file.Close()

		log.Println("Generating delete queries for ledger_transactions table")
		deleteCount, errCount = c.prepareAndExecuteSimpleDeleteQueries(rangeFrom, rangeTo,
			"DELETE FROM ledger_transactions WHERE ledger_sequence = ?", columnSettings{UseBlob: false, UseSeq: true})
		log.Printf("Total delete queries: %d\n\n", deleteCount)
		totalErrors += errCount
		totalDeletes += deleteCount

		os.Remove("continue.txt")
	}

	// ledgers queries
	if !c.settings.SkipLedgersTable {
		file, err := createAndWriteToFile("ledgers", &c.settings.Command)
		if err != nil {
			return err
		}
		defer file.Close()

		log.Println("Generating delete queries for ledgers table")
		deleteCount, errCount = c.prepareAndExecuteSimpleDeleteQueries(rangeFrom, rangeTo,
			"DELETE FROM ledgers WHERE sequence = ?", columnSettings{UseBlob: false, UseSeq: true})
		log.Printf("Total delete queries: %d\n\n", deleteCount)
		totalErrors += errCount
		totalDeletes += deleteCount

		os.Remove("continue.txt")
	}

	// account_tx queries
	if !c.settings.SkipAccTransactionsTable {
		file, err := createAndWriteToFile("account_tx", &c.settings.Command)
		if err != nil {
			return err
		}
		defer file.Close()

		log.Println("Generating delete queries for account transactions table")
		rowsCount, deleteCount, errCount = c.prepareAndExecuteDeleteQueries(file, fromLedgerIdx, toLedgerIdx,
			"SELECT account, seq_idx FROM account_tx WHERE token(account) >= ? AND token(account) <= ?",
			"DELETE FROM account_tx WHERE account = ? AND seq_idx = (?, ?)", deleteMethod{deleteTransaction: maybe.Set(true)}, columnSettings{UseBlob: true, UseSeq: false})
		log.Printf("Total delete queries: %d\n", deleteCount)
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalRows += rowsCount
		totalErrors += errCount
		totalDeletes += deleteCount

		os.Remove("continue.txt")
	}

	// TODO: take care of nft tables and other stuff like that

	if !c.settings.SkipWriteLatestLedger {
		var (
			first maybe.Maybe[uint64]
			last  maybe.Maybe[uint64]
		)

		if fromLedgerIdx.HasValue() {
			last = maybe.Set(fromLedgerIdx.Value() - 1)
		}

		if toLedgerIdx.HasValue() {
			first = maybe.Set(toLedgerIdx.Value() + 1)
		}

		if err := c.updateLedgerRange(first, last); err != nil {
			log.Printf("ERROR failed updating ledger range: %s\n", err)
			return err
		}
	}

	log.Printf("TOTAL ERRORS: %d\n", totalErrors)
	log.Printf("TOTAL ROWS TRAVERSED: %d\n", totalRows)
	log.Printf("TOTAL DELETES: %d\n\n", totalDeletes)

	log.Printf("Completed deletion for %s -> %s\n\n", fromStr, toStr)

	return nil
}

func (c *ClioCass) prepareAndExecuteSimpleDeleteQueries(
	fromLedgerIdx uint64,
	toLedgerIdx uint64,
	deleteQueryTemplate string,
	colSettings columnSettings,
) (uint64, uint64) {
	var totalDeletes uint64
	var totalErrors uint64

	var info = deleteInfo{Query: deleteQueryTemplate}

	if session, err := c.clusterConfig.CreateSession(); err == nil {
		defer session.Close()
		for i := fromLedgerIdx; i <= toLedgerIdx; i++ {
			info.Data = append(info.Data, deleteParams{Seq: i})
			// for every 1000 queries in data, delete
			if len(info.Data) == 1000 {
				_, err := c.performDeleteQueries(&info, session, colSettings)
				atomic.AddUint64(&totalDeletes, uint64(len(info.Data)))
				atomic.AddUint64(&totalErrors, err)
				info = deleteInfo{Query: deleteQueryTemplate}
			}
		}
		// delete the rest of queries if exists
		if len(info.Data) > 0 {
			_, err := c.performDeleteQueries(&info, session, colSettings)
			atomic.AddUint64(&totalDeletes, uint64(len(info.Data)))
			atomic.AddUint64(&totalErrors, err)
		}
	} else {
		log.Printf("ERROR: %s\n", err)
		fmt.Fprintf(os.Stderr, "FAILED TO CREATE SESSION: %s\n", err)
		atomic.AddUint64(&totalErrors, 1)
	}
	return totalDeletes, totalErrors
}

func (c *ClioCass) prepareDefaultDelete(
	scanner gocql.Scanner,
	info *deleteInfo,
	fromLedgerIdx maybe.Maybe[uint64],
	toLedgerIdx maybe.Maybe[uint64],
	rowsRetrieved *uint64,
) bool {
	for scanner.Next() {
		var key []byte
		var seq uint64

		err := scanner.Scan(&key, &seq)
		if err == nil {
			*rowsRetrieved++

			// only grab the rows that are in the correct range of sequence numbers
			if fromLedgerIdx.HasValue() && fromLedgerIdx.Value() <= seq {
				info.Data = append(info.Data, deleteParams{Seq: seq, Blob: key})
			} else if toLedgerIdx.HasValue() && seq <= toLedgerIdx.Value() {
				info.Data = append(info.Data, deleteParams{Seq: seq, Blob: key})
			}
		} else {
			return false
		}
	}
	return true
}

func (c *ClioCass) prepareObjectDelete(
	scanner gocql.Scanner,
	info *deleteInfo,
	fromLedgerIdx maybe.Maybe[uint64],
	toLedgerIdx maybe.Maybe[uint64],
	rowsRetrieved *uint64,
) bool {
	var previousKey []byte
	var foundLastValid bool
	var keepLastValid = true

	for scanner.Next() {
		var key []byte
		var seq uint64

		err := scanner.Scan(&key, &seq)
		if err == nil {
			*rowsRetrieved++

			if keepLastValid && !slices.Equal(previousKey, key) {
				previousKey = key
				foundLastValid = false
			}

			// only grab the rows that are in the correct range of sequence numbers
			if fromLedgerIdx.HasValue() && fromLedgerIdx.Value() <= seq {
				info.Data = append(info.Data, deleteParams{Seq: seq, Blob: key})
			} else if toLedgerIdx.HasValue() {
				if seq <= toLedgerIdx.Value() && (!keepLastValid || foundLastValid) {
					info.Data = append(info.Data, deleteParams{Seq: seq, Blob: key})
				} else if seq <= toLedgerIdx.Value()+1 {
					foundLastValid = true
				}
			}
		} else {
			return false
		}
	}
	return true
}

func (c *ClioCass) prepareAccTxnDelete(
	scanner gocql.Scanner,
	info *deleteInfo,
	fromLedgerIdx maybe.Maybe[uint64],
	toLedgerIdx maybe.Maybe[uint64],
	rowsRetrieved *uint64,
) bool {
	for scanner.Next() {
		var key []byte
		var ledgerIndex, txnIndex uint64

		// account_tx/nft table has seq_idx frozen<tuple<bigint, bigint>>
		err := scanner.Scan(&key, &ledgerIndex, &txnIndex)
		if err == nil {
			*rowsRetrieved++

			// only grab the rows that are in the correct range of sequence numbers
			if fromLedgerIdx.HasValue() && fromLedgerIdx.Value() <= ledgerIndex {
				info.Data = append(info.Data, deleteParams{Seq: ledgerIndex, Blob: key, tnxIndex: txnIndex})
			} else if toLedgerIdx.HasValue() && ledgerIndex <= toLedgerIdx.Value() {
				info.Data = append(info.Data, deleteParams{Seq: ledgerIndex, Blob: key, tnxIndex: txnIndex})
			}
		} else {
			return false
		}
	}
	return true
}

func (c *ClioCass) prepareAndExecuteDeleteQueries(
	file *os.File,
	fromLedgerIdx maybe.Maybe[uint64],
	toLedgerIdx maybe.Maybe[uint64],
	queryTemplate string,
	deleteQueryTemplate string,
	method deleteMethod,
	colSettings columnSettings,
) (uint64, uint64, uint64) {
	rangesChannel := make(chan *util.TokenRange, len(c.settings.Ranges))
	for i := range c.settings.Ranges {
		rangesChannel <- c.settings.Ranges[i]
	}

	close(rangesChannel)

	var wg sync.WaitGroup
	var sessionCreationWaitGroup sync.WaitGroup
	var totalRows uint64
	var totalDeletes uint64
	var totalErrors uint64
	counter := uint64(1000)

	wg.Add(c.settings.WorkerCount)
	sessionCreationWaitGroup.Add(c.settings.WorkerCount)

	for i := 0; i < c.settings.WorkerCount; i++ {
		go func(q string) {
			defer wg.Done()

			var session *gocql.Session
			var err error
			if session, err = c.clusterConfig.CreateSession(); err == nil {
				defer session.Close()

				sessionCreationWaitGroup.Done()
				sessionCreationWaitGroup.Wait()
				preparedQuery := session.Query(q)

				for r := range rangesChannel {
					if c.settings.RangesRead != nil {
						if value, exists := c.settings.RangesRead.TokenRange[r.StartRange]; exists {
							// Check for end range
							if value == r.EndRange {
								fmt.Fprintf(file, "%d, %d \n", r.StartRange, r.EndRange)
								continue
							}
						}
					}

					preparedQuery.Bind(r.StartRange, r.EndRange)

					var pageState []byte
					var rowsRetrieved uint64
					var info = deleteInfo{Query: deleteQueryTemplate}

					for {
						iter := preparedQuery.PageSize(c.clusterConfig.PageSize).PageState(pageState).Iter()
						nextPageState := iter.PageState()
						scanner := iter.Scanner()

						var prepareDeleteResult bool

						// query object table first as it is the largest table by far
						if method.deleteObject.HasValue() && method.deleteObject.Value() {
							prepareDeleteResult = c.prepareObjectDelete(scanner, &info, fromLedgerIdx, toLedgerIdx, &rowsRetrieved)
						} else if method.deleteTransaction.HasValue() && method.deleteTransaction.Value() {
							prepareDeleteResult = c.prepareAccTxnDelete(scanner, &info, fromLedgerIdx, toLedgerIdx, &rowsRetrieved)
						} else if method.deleteGeneral.HasValue() && method.deleteGeneral.Value() {
							prepareDeleteResult = c.prepareDefaultDelete(scanner, &info, fromLedgerIdx, toLedgerIdx, &rowsRetrieved)
						}

						if !prepareDeleteResult {
							log.Printf("ERROR: page iteration failed: %s\n", err)
							fmt.Fprintf(os.Stderr, "FAILED QUERY: %s\n", fmt.Sprintf("%s [from=%d][to=%d][pagestate=%x]", queryTemplate, r.StartRange, r.EndRange, pageState))
							atomic.AddUint64(&totalErrors, 1)
						}

						if len(nextPageState) == 0 {
							// Checks for delete queries after iterating all pages
							if len(info.Data) > 0 {
								_, numErr := c.performDeleteQueries(&info, session, colSettings)
								atomic.AddUint64(&totalErrors, numErr)
								atomic.AddUint64(&totalDeletes, uint64(len(info.Data)))
								if totalDeletes >= counter {
									log.Printf("... deleted %d queries ...", counter)
									counter += 1000
								}
								// reset back to the deleted query template after finishing executing delete
								info = deleteInfo{Query: deleteQueryTemplate}
							}
							break
						}
						pageState = nextPageState
					}
					fmt.Fprintf(file, "%d, %d \n", r.StartRange, r.EndRange)
					atomic.AddUint64(&totalRows, rowsRetrieved)
				}
				// after finishing deletion of one table, set to nil, because we continue to delete normally now
				c.settings.RangesRead = nil
			} else {
				log.Printf("ERROR: %s\n", err)
				fmt.Fprintf(os.Stderr, "FAILED TO CREATE SESSION: %s\n", err)
				atomic.AddUint64(&totalErrors, 1)
			}
		}(queryTemplate)
	}

	wg.Wait()
	return totalRows, totalDeletes, totalErrors
}

func (c *ClioCass) splitDeleteWork(info *deleteInfo) [][]deleteParams {
	var n = c.settings.WorkerCount
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

func (c *ClioCass) performDeleteQueries(info *deleteInfo, session *gocql.Session, colSettings columnSettings) (uint64, uint64) {
	var wg sync.WaitGroup
	var sessionCreationWaitGroup sync.WaitGroup
	var totalDeletes uint64
	var totalErrors uint64

	chunks := c.splitDeleteWork(info)
	chunksChannel := make(chan []deleteParams, len(chunks))
	for i := range chunks {
		chunksChannel <- chunks[i]
	}

	close(chunksChannel)

	wg.Add(c.settings.WorkerCount)
	sessionCreationWaitGroup.Add(c.settings.WorkerCount)

	query := info.Query
	bindCount := strings.Count(query, "?")

	for i := 0; i < c.settings.WorkerCount; i++ {
		go func(number int, q string, bc int) {
			defer wg.Done()

			sessionCreationWaitGroup.Done()
			sessionCreationWaitGroup.Wait()
			preparedQuery := session.Query(q)

			for chunk := range chunksChannel {
				for _, r := range chunk {
					if bc == 3 {
						preparedQuery.Bind(r.Blob, r.Seq, r.tnxIndex)
					} else if bc == 2 {
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
						if atomic.LoadUint64(&totalDeletes)%10000 == 0 {
							log.Printf("... %d deletes ...\n", totalDeletes)
						}
					}
				}
			}
		}(i, query, bindCount)
	}

	wg.Wait()
	return totalDeletes, totalErrors
}

func (c *ClioCass) updateLedgerRange(newStartLedger maybe.Maybe[uint64], newEndLedger maybe.Maybe[uint64]) error {
	if session, err := c.clusterConfig.CreateSession(); err == nil {
		defer session.Close()

		query := "UPDATE ledger_range SET sequence = ? WHERE is_latest = ?"

		if newEndLedger.HasValue() {
			log.Printf("Updating ledger range end to %d\n", newEndLedger.Value())

			preparedQuery := session.Query(query, newEndLedger.Value(), true)
			if err := preparedQuery.Exec(); err != nil {
				fmt.Fprintf(os.Stderr, "FAILED QUERY: %s [seq=%d][true]\n", query, newEndLedger.Value())
				return err
			}
		}

		if newStartLedger.HasValue() {
			log.Printf("Updating ledger range start to %d\n", newStartLedger.Value())

			preparedQuery := session.Query(query, newStartLedger.Value(), false)
			if err := preparedQuery.Exec(); err != nil {
				fmt.Fprintf(os.Stderr, "FAILED QUERY: %s [seq=%d][false]\n", query, newStartLedger.Value())
				return err
			}
		}
	} else {
		fmt.Fprintf(os.Stderr, "FAILED TO CREATE SESSION: %s\n", err)
		return err
	}

	return nil
}

func createAndWriteToFile(tableName string, command *string) (*os.File, error) {
	file, err := os.Create("continue.txt")
	if err != nil {
		fmt.Printf("Error creating file for %s table: %v\n", tableName, err)
		return nil, err
	}
	fmt.Fprintf(file, "%s\n", *command)
	file.WriteString(fmt.Sprintf("%s\n", tableName))

	return file, nil
}
