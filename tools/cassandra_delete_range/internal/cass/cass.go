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

	if firstLedgerIdxInDB > ledgerIdx {
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

	if firstLedgerIdxInDB >= ledgerIdx {
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

	var info deleteInfo
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
		log.Println("Generating delete queries for successor table")
		info, rowsCount, errCount = c.prepareDeleteQueries(fromLedgerIdx, toLedgerIdx,
			"SELECT key, seq FROM successor WHERE token(key) >= ? AND token(key) <= ?",
			"DELETE FROM successor WHERE key = ? AND seq = ?", deleteMethod{deleteGeneral: maybe.Set(true)})
		log.Printf("Total delete queries: %d\n", len(info.Data))
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalErrors += errCount
		totalRows += rowsCount
		deleteCount, errCount = c.performDeleteQueries(&info, columnSettings{UseBlob: true, UseSeq: true})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// objects queries
	if !c.settings.SkipObjectsTable {
		log.Println("Generating delete queries for objects table")
		info, rowsCount, errCount = c.prepareDeleteQueries(fromLedgerIdx, toLedgerIdx,
			"SELECT key, sequence FROM objects WHERE token(key) >= ? AND token(key) <= ?",
			"DELETE FROM objects WHERE key = ? AND sequence = ?", deleteMethod{deleteObject: maybe.Set(true)})
		log.Printf("Total delete queries: %d\n", len(info.Data))
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalErrors += errCount
		totalRows += rowsCount
		deleteCount, errCount = c.performDeleteQueries(&info, columnSettings{UseBlob: true, UseSeq: true})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// ledger_hashes queries
	if !c.settings.SkipLedgerHashesTable {
		log.Println("Generating delete queries for ledger_hashes table")
		info, rowsCount, errCount = c.prepareDeleteQueries(fromLedgerIdx, toLedgerIdx,
			"SELECT hash, sequence FROM ledger_hashes WHERE token(hash) >= ? AND token(hash) <= ?",
			"DELETE FROM ledger_hashes WHERE hash = ?", deleteMethod{deleteGeneral: maybe.Set(true)})
		log.Printf("Total delete queries: %d\n", len(info.Data))
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalErrors += errCount
		totalRows += rowsCount
		deleteCount, errCount = c.performDeleteQueries(&info, columnSettings{UseBlob: true, UseSeq: false})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// transactions queries
	if !c.settings.SkipTransactionsTable {
		log.Println("Generating delete queries for transactions table")
		info, rowsCount, errCount = c.prepareDeleteQueries(fromLedgerIdx, toLedgerIdx,
			"SELECT hash, ledger_sequence FROM transactions WHERE token(hash) >= ? AND token(hash) <= ?",
			"DELETE FROM transactions WHERE hash = ?", deleteMethod{deleteGeneral: maybe.Set(true)})
		log.Printf("Total delete queries: %d\n", len(info.Data))
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalErrors += errCount
		totalRows += rowsCount
		deleteCount, errCount = c.performDeleteQueries(&info, columnSettings{UseBlob: true, UseSeq: false})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// diff queries
	if !c.settings.SkipDiffTable {
		log.Println("Generating delete queries for diff table")
		info = c.prepareSimpleDeleteQueries(rangeFrom, rangeTo,
			"DELETE FROM diff WHERE seq = ?")
		log.Printf("Total delete queries: %d\n\n", len(info.Data))
		deleteCount, errCount = c.performDeleteQueries(&info, columnSettings{UseBlob: false, UseSeq: true})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// ledger_transactions queries
	if !c.settings.SkipLedgerTransactionsTable {
		log.Println("Generating delete queries for ledger_transactions table")
		info = c.prepareSimpleDeleteQueries(rangeFrom, rangeTo,
			"DELETE FROM ledger_transactions WHERE ledger_sequence = ?")
		log.Printf("Total delete queries: %d\n\n", len(info.Data))
		deleteCount, errCount = c.performDeleteQueries(&info, columnSettings{UseBlob: false, UseSeq: true})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	// ledgers queries
	if !c.settings.SkipLedgersTable {
		log.Println("Generating delete queries for ledgers table")

		info = c.prepareSimpleDeleteQueries(rangeFrom, rangeTo,
			"DELETE FROM ledgers WHERE sequence = ?")
		log.Printf("Total delete queries: %d\n\n", len(info.Data))
		deleteCount, errCount = c.performDeleteQueries(&info, columnSettings{UseBlob: false, UseSeq: true})
		totalErrors += errCount
		totalDeletes += deleteCount
	}

	if !c.settings.SkipAccTransactionsTable {
		log.Println("Generating delete queries for account transactions table")

		info, rowsCount, errCount = c.prepareDeleteQueries(fromLedgerIdx, toLedgerIdx,
			"SELECT account, seq_idx FROM account_tx WHERE token(account) >= ? AND token(account) <= ?",
			"DELETE FROM account_tx WHERE account = ? AND seq_idx = (?, ?)", deleteMethod{deleteTransaction: maybe.Set(true)})
		log.Printf("Total delete queries: %d\n", len(info.Data))
		log.Printf("Total traversed rows: %d\n\n", rowsCount)
		totalErrors += errCount
		totalRows += rowsCount
		deleteCount, errCount = c.performDeleteQueries(&info, columnSettings{UseBlob: true, UseSeq: false})
		totalErrors += errCount
		totalDeletes += deleteCount
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

func (c *ClioCass) prepareSimpleDeleteQueries(
	fromLedgerIdx uint64,
	toLedgerIdx uint64,
	deleteQueryTemplate string,
) deleteInfo {
	var info = deleteInfo{Query: deleteQueryTemplate}

	for i := fromLedgerIdx; i <= toLedgerIdx; i++ {
		info.Data = append(info.Data, deleteParams{Seq: i})
	}

	return info
}

func prepareDelete(
	scanner gocql.Scanner,
	outChannel chan deleteParams,
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
				outChannel <- deleteParams{Seq: seq, Blob: key}
			} else if toLedgerIdx.HasValue() && seq <= toLedgerIdx.Value() {
				outChannel <- deleteParams{Seq: seq, Blob: key}
			}
		} else {
			return false
		}
	}
	return true
}

func prepareObjectDelete(
	scanner gocql.Scanner,
	outChannel chan deleteParams,
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
				outChannel <- deleteParams{Seq: seq, Blob: key}
			} else if toLedgerIdx.HasValue() {
				if seq <= toLedgerIdx.Value() && (!keepLastValid || foundLastValid) {
					outChannel <- deleteParams{Seq: seq, Blob: key}
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

func prepareAccTxnDelete(
	scanner gocql.Scanner,
	outChannel chan deleteParams,
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
				outChannel <- deleteParams{Seq: ledgerIndex, Blob: key, tnxIndex: txnIndex}
			} else if toLedgerIdx.HasValue() && ledgerIndex <= toLedgerIdx.Value() {
				outChannel <- deleteParams{Seq: ledgerIndex, Blob: key, tnxIndex: txnIndex}
			}
		} else {
			return false
		}
	}
	return true
}

func (c *ClioCass) prepareDeleteQueries(
	fromLedgerIdx maybe.Maybe[uint64],
	toLedgerIdx maybe.Maybe[uint64],
	queryTemplate string,
	deleteQueryTemplate string,
	method deleteMethod,
) (deleteInfo, uint64, uint64) {
	rangesChannel := make(chan *util.TokenRange, len(c.settings.Ranges))
	for i := range c.settings.Ranges {
		rangesChannel <- c.settings.Ranges[i]
	}

	close(rangesChannel)

	outChannel := make(chan deleteParams)
	var info = deleteInfo{Query: deleteQueryTemplate}

	go func() {
		total := uint64(0)
		for params := range outChannel {
			total += 1
			if total%1000 == 0 {
				log.Printf("... %d queries ...\n", total)
			}
			info.Data = append(info.Data, params)
		}
	}()

	var wg sync.WaitGroup
	var sessionCreationWaitGroup sync.WaitGroup
	var totalRows uint64
	var totalErrors uint64

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
					preparedQuery.Bind(r.StartRange, r.EndRange)

					var pageState []byte
					var rowsRetrieved uint64

					for {
						iter := preparedQuery.PageSize(c.clusterConfig.PageSize).PageState(pageState).Iter()
						nextPageState := iter.PageState()
						scanner := iter.Scanner()

						var prepareDeleteResult bool

						// query object table first as it is the largest table by far
						if method.deleteObject.HasValue() && method.deleteObject.Value() {
							prepareDeleteResult = prepareObjectDelete(scanner, outChannel, fromLedgerIdx, toLedgerIdx, &rowsRetrieved)
						} else if method.deleteTransaction.HasValue() && method.deleteTransaction.Value() {
							prepareDeleteResult = prepareAccTxnDelete(scanner, outChannel, fromLedgerIdx, toLedgerIdx, &rowsRetrieved)
						} else if method.deleteGeneral.HasValue() && method.deleteGeneral.Value() {
							prepareDeleteResult = prepareDelete(scanner, outChannel, fromLedgerIdx, toLedgerIdx, &rowsRetrieved)
						}

						if !prepareDeleteResult {
							log.Printf("ERROR: page iteration failed: %s\n", err)
							fmt.Fprintf(os.Stderr, "FAILED QUERY: %s\n", fmt.Sprintf("%s [from=%d][to=%d][pagestate=%x]", queryTemplate, r.StartRange, r.EndRange, pageState))
							atomic.AddUint64(&totalErrors, 1)
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

func (c *ClioCass) performDeleteQueries(info *deleteInfo, colSettings columnSettings) (uint64, uint64) {
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

			var session *gocql.Session
			var err error
			if session, err = c.clusterConfig.CreateSession(); err == nil {
				defer session.Close()

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
