package ledgers

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

const (
	fileName = "manifest.txt"
)

type Manifest struct {
	folderPath string
	filePath   string
}

func NewManifest(folderPath string) *Manifest {
	return &Manifest{
		folderPath: folderPath,
		filePath:   filepath.Join(folderPath, fileName),
	}
}

func (fm *Manifest) SetLedgerRange(start uint32, end uint32) error {
	content := fmt.Sprintf("%d|%d", start, end)
	return fm.writeToFile(content)
}

func (fm *Manifest) AppendDeltaLedger(delta1 uint32, delta2 uint32) error {
	start, end, err := fm.Read()
	if err != nil {
		return err
	}
	//rewrite the range if new delta can extend the current range continuously
	if delta1 >= start && (end+1) >= delta1 && delta2 >= delta1 {
		return fm.SetLedgerRange(start, delta2)
	}

	return fmt.Errorf("Invalid delta ledger range")
}

func (fm *Manifest) writeToFile(content string) error {
	os.MkdirAll(fm.folderPath, os.ModePerm)
	file, err := os.OpenFile(fm.filePath, os.O_RDWR|os.O_CREATE|os.O_TRUNC, 0666)
	if err != nil {
		return err
	}
	defer file.Close()

	_, err = file.WriteString(content)
	if err != nil {
		return err
	}
	return nil
}

func (fm *Manifest) IsExist() bool {
	_, err := os.Stat(fm.filePath)
	return !os.IsNotExist(err)
}

func (fm *Manifest) Read() (uint32, uint32, error) {
	content, err := os.ReadFile(fm.filePath)
	if err != nil {
		return 0, 0, err
	}
	if len(content) == 0 {
		return 0, 0, nil
	}

	parts := strings.Split(string(content), "|")
	if len(parts) != 2 {
		return 0, 0, fmt.Errorf("file content is not in expected format")
	}

	part1, err := strconv.ParseUint(strings.TrimSpace(parts[0]), 10, 32)
	if err != nil {
		return 0, 0, fmt.Errorf("error parsing the first part: %v", err)
	}

	part2, err := strconv.ParseUint(strings.TrimSpace(parts[1]), 10, 32)

	if err != nil {
		return 0, 0, fmt.Errorf("error parsing the second part: %v", err)
	}

	return uint32(part1), uint32(part2), nil
}
