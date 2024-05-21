package trigger

import (
	"fmt"
	"os"
	"os/signal"
	"requests_gun/internal/ammo_provider"
	"requests_gun/internal/parse_args"
	"requests_gun/internal/request_maker"
	"sync"
	"sync/atomic"
	"time"
)

func Fire(ammoProvider *ammo_provider.AmmoProvider, args *parse_args.CliArgs) {
	stop := atomic.Bool{}
	interrupt := make(chan os.Signal, 1)
	signal.Notify(interrupt, os.Interrupt)

	go func() {

		s := <-interrupt
		fmt.Println("Got signal:", s)
		fmt.Println("Stopping...")
		stop.Store(true)
	}()

	wg := sync.WaitGroup{}
	for !stop.Load() {
		statistics := statistics{startTime: time.Now(), printErrors: args.PrintErrors}
		do_shot := func() {
			defer wg.Done()
			bullet := ammoProvider.GetBullet()
            requestMaker := request_maker.NewHttp(args.Url, args.Port)
			response_data, err := requestMaker.MakeRequest(bullet)
			statistics.add(response_data, err)
		}

		second_start := time.Now()
		requests_number := uint(0)
		for requests_number < args.TargetLoad && time.Since(second_start) < time.Second {
			wg.Add(1)
			go do_shot()
			requests_number++
		}
		wg.Wait()
		if time.Since(second_start) < time.Second {
			time.Sleep(time.Second - time.Since(second_start))
		} else {
			fmt.Println("Requests took longer than a second, probably need to decrease the load.")
		}
		statistics.print()
	}
}

type counters struct {
	totalRequestsCount atomic.Uint64
	errorsCount        atomic.Uint64
	badReplyCount      atomic.Uint64
	goodReplyCount     atomic.Uint64
}
type statistics struct {
	counters    counters
	startTime   time.Time
	printErrors bool
}

func (s *statistics) add(response *request_maker.ResponseData, err error) {
	s.counters.totalRequestsCount.Add(1)
	if err != nil {
		if s.printErrors {
			fmt.Println("Error making request:", err)
		}
		s.counters.errorsCount.Add(1)
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
		s.counters.badReplyCount.Add(1)
	} else {
		s.counters.goodReplyCount.Add(1)
	}
}

func (s *statistics) print() {
	elapsed := time.Since(s.startTime)
	if elapsed < time.Second {
		elapsed = time.Second
	}
	fmt.Printf("Speed: %.1f rps, Errors: %.1f%%, Bad response: %.1f%%, Good response: %.1f%%\n",
		float64(s.counters.totalRequestsCount.Load())/elapsed.Seconds(),
		float64(s.counters.errorsCount.Load())/float64(s.counters.totalRequestsCount.Load())*100,
		float64(s.counters.badReplyCount.Load())/float64(s.counters.totalRequestsCount.Load())*100,
		float64(s.counters.goodReplyCount.Load())/float64(s.counters.totalRequestsCount.Load())*100)
}
