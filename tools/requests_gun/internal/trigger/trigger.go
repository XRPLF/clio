package trigger

import (
	"fmt"
	"os"
	"os/signal"
	"requests_gun/internal/ammo_provider"
	"requests_gun/internal/request_maker"
	"sync"
	"sync/atomic"
	"time"
)

func Fire(ammoProvider *ammo_provider.AmmoProvider, requestMaker request_maker.RequestMaker, load uint, printErrors bool) {
	wg := sync.WaitGroup{}

	stop := atomic.Bool{}
	interrupt := make(chan os.Signal, 1)
	signal.Notify(interrupt, os.Interrupt)

	wg.Add(1)
	go func() {
		defer wg.Done()

		s := <-interrupt
		fmt.Println("Got signal:", s)
		fmt.Println("Stopping...")
		stop.Store(true)
	}()

	statistics := statistics{startTime: time.Now(), printErrors: printErrors}
	do_shot := func() {
		defer wg.Done()
		bullet := ammoProvider.GetBullet()
		response_data, err := requestMaker.MakeRequest(bullet)
		statistics.add(response_data, err)
	}

	for !stop.Load() {
		second_start := time.Now()
		requests_number := uint(0)
		sleep_time := time.Second / time.Duration(load)
		for requests_number < load && time.Since(second_start) < time.Second {
			wg.Add(1)
			go do_shot()
			requests_number++
			time.Sleep(sleep_time)
		}
		statistics.print()
	}
}

type statistics struct {
	totalRequestsCount atomic.Uint64
	errorsCount        atomic.Uint64
	badReplyCount      atomic.Uint64
	goodReplyCount     atomic.Uint64
	startTime          time.Time
	printErrors        bool
}

func (s *statistics) add(response *request_maker.ResponseData, err error) {
	s.totalRequestsCount.Add(1)
	if err != nil {
		if s.printErrors {
			fmt.Println("Error making request:", err)
		}
		s.errorsCount.Add(1)
		return
	}
	if response.StatusCode != 200 || response.Body["error"] != nil {
		if s.printErrors {
			fmt.Print("Response contains error: ", response.StatusStr)
			if response.Body["error"] != nil {
				fmt.Println(" ", response.Body["error"])
			} else {
				fmt.Println()
			}
		}
		s.badReplyCount.Add(1)
	} else {
		s.goodReplyCount.Add(1)
	}
}

func (s *statistics) print() {
	elapsed := time.Since(s.startTime)
	fmt.Printf("Rps: %.1f Errors: %.1f%%, Bad response: %.1f%%, Good response: %.1f%%\n",
		float64(s.totalRequestsCount.Load())/elapsed.Seconds(),
		float64(s.errorsCount.Load())/float64(s.totalRequestsCount.Load())*100,
		float64(s.badReplyCount.Load())/float64(s.totalRequestsCount.Load())*100,
		float64(s.goodReplyCount.Load())/float64(s.totalRequestsCount.Load())*100)
}
