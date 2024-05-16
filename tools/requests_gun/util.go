package main

import (
	"fmt"
	"os"
)

func CheckError(err error, message string) {
	if err != nil {
		fmt.Fprintf(os.Stderr, "%s: %s\n", message, err)
		os.Exit(1)
	}
}
