package main

import "fmt"

func main() {
    args := parseArgs()
    fmt.Printf("%+v\n", args)
}
