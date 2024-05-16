package main

import "fmt"

func main() {
    args := ParseArgs()
    fmt.Print("Loading ammo... ")
	ammoProvider := NewAmmoProvider(args.ammo)
    fmt.Println("Done")
    requestMaker := NewHttpRequestMaker(args.url, args.port)
    fmt.Println("Firing requests...")
    fire(&ammoProvider, &requestMaker, args.target_load)
}
