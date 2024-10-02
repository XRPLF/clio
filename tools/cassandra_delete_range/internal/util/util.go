package util

import (
	"fmt"
	"log"
	"math"
	"math/rand"

	"github.com/gocql/gocql"
	"github.com/pmorelli92/maybe"
)

type TokenRange struct {
	StartRange int64
	EndRange   int64
}

// not stored as arrays of startRange/endRange because it will be O(n) lookup
// stored as Map with key startRange, value endRange so O(1) lookup for tokenRange
type StoredRange struct {
	TokenRange  maybe.Maybe[map[int64]int64] // all ranges that has been read and deleted
	LedgerRange maybe.Maybe[uint64]          // read up to this specific ledger index
}

func Shuffle(data []*TokenRange) {
	for i := 1; i < len(data); i++ {
		r := rand.Intn(i + 1)
		if i != r {
			data[r], data[i] = data[i], data[r]
		}
	}
}

func PromptContinue() bool {
	var continueFlag string

	log.Println("Are you sure you want to continue? (y/n)")
	if fmt.Scanln(&continueFlag); continueFlag != "y" {
		return false
	}

	return true
}

func GetTokenRanges(workerCount int) []*TokenRange {
	var n = workerCount
	var m = int64(n * 100)
	var maxSize uint64 = math.MaxInt64 * 2
	var rangeSize = maxSize / uint64(m)

	var start int64 = math.MinInt64
	var end int64
	var shouldBreak = false

	var ranges = make([]*TokenRange, m)

	for i := int64(0); i < m; i++ {
		end = start + int64(rangeSize)
		if start > 0 && end < 0 {
			end = math.MaxInt64
			shouldBreak = true
		}

		ranges[i] = &TokenRange{StartRange: start, EndRange: end}

		if shouldBreak {
			break
		}

		start = end + 1
	}

	return ranges
}

func GetConsistencyLevel(consistencyValue string) gocql.Consistency {
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
