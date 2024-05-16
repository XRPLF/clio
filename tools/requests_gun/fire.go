package main

import (
	"fmt"
	"os"
	"os/signal"
	"sync"
	"sync/atomic"
	"time"
)

func fire(ammoProvider *AmmoProvider, requestMaker RequestMaker, load uint) {
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

	statistics := Statistics{start_time: time.Now()}
	do_shot := func() {
		defer wg.Done()
		bullet := ammoProvider.GetBullet()
		response, status_code, err := requestMaker.MakeRequest(bullet)
		statistics.add(response, status_code, err)
	}

	for !stop.Load() {
		secondStart := time.Now()
		requests_number := uint(0)
		for requests_number < load && time.Since(secondStart) < time.Second {
			wg.Add(1)
			go do_shot()
		}
		statistics.print()
	}
}

type Statistics struct {
	total_requests_count atomic.Uint64
	errors_count         atomic.Uint64
	bad_reply_count      atomic.Uint64
	good_reply_count     atomic.Uint64
	start_time           time.Time
}

func (s *Statistics) add(response JsonMap, status StatusCode, err error) {
	s.total_requests_count.Add(1)
	if err != nil {
		s.errors_count.Add(1)
		return
	}
	if status != 200 || response["error"] != nil {
		s.bad_reply_count.Add(1)
	} else {
		s.good_reply_count.Add(1)
	}

}

func (s *Statistics) print() {
	elapsed := time.Since(s.start_time)
	fmt.Printf("Rps: %f Errors: %f%%, Bad responce: %f%%, Good responce: %f%%\n",
		float64(s.total_requests_count.Load())/elapsed.Seconds(),
		float64(s.errors_count.Load())/float64(s.total_requests_count.Load())*100,
		float64(s.bad_reply_count.Load())/float64(s.total_requests_count.Load())*100,
		float64(s.good_reply_count.Load())/float64(s.total_requests_count.Load())*100)
}
