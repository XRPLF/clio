package util

import (
	"fmt"
	"os"
)

type Marker struct {
	Cmd  string
	File *os.File
}

func NewMarker(cmd string) *Marker {
	return &Marker{Cmd: cmd}
}

func CloseMarker(m *Marker) {
	if m.File != nil {
		m.File.Close()
	}
	os.Remove("continue.txt")
}

func (m *Marker) EnterTable(table string) error {
	// Create the file
	file, err := os.OpenFile("continue.txt", os.O_WRONLY|os.O_TRUNC|os.O_CREATE, 0644)
	m.File = file
	if err != nil {
		return fmt.Errorf("failed to create file: %w", err)
	}
	fmt.Fprintf(m.File, "%s\n", m.Cmd)
	m.File.WriteString(fmt.Sprintf("%s\n", table))
	return nil
}

func (m *Marker) MarkProgress(x int64, y int64) {
	fmt.Fprintf(m.File, "%d, %d \n", x, y)
}

func (m *Marker) ExitTable() {
	m.File.Close()
	m.File = nil
	os.Remove("continue.txt")
}
