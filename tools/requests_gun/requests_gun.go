package main

import (
	"fmt"
	"os"
	"requests_gun/internal/ammo_provider"
	"requests_gun/internal/parse_args"
	"requests_gun/internal/trigger"
)

func main() {
	args, err := parse_args.Parse()
    if err != nil {
        fmt.Fprintln(os.Stderr, "Error: ", err)
        parse_args.PrintUsage()
        os.Exit(1)
    }

	fmt.Print("Loading ammo... ")
    f, err := os.Open(args.Ammo)
    if err != nil {
        fmt.Println("Error opening file '", args.Ammo, "': ", err)
        os.Exit(1)
    }
	ammoProvider := ammo_provider.New(f)
    f.Close()
	fmt.Println("Done")

	fmt.Println("Firing requests...")
	trigger.Fire(ammoProvider, args)
}
