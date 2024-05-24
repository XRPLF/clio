package trigger

import (
	"log"
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
	interrupt := make(chan os.Signal, 1)
	signal.Notify(interrupt, os.Interrupt)

	wg := sync.WaitGroup{}
	ticker := time.NewTicker(time.Second)
	for {
		select {
		case s := <-interrupt:
			log.Println("Got signal:", s)
			log.Println("Stopping...")
			ticker.Stop()
			return
		case <-ticker.C:
			statistics := statistics{startTime: time.Now(), printErrors: args.PrintErrors}
			doShot := func() {
				defer wg.Done()
				bullet := ammoProvider.GetBullet()
				requestMaker := request_maker.NewHttp(args.Url, args.Port)
				responseData, err := requestMaker.MakeRequest(bullet)
				statistics.add(responseData, err)
			}

			secondStart := time.Now()
			requestsNumber := uint(0)
			for requestsNumber < args.TargetLoad && time.Since(secondStart) < time.Second {
				wg.Add(1)
				go doShot()
				requestsNumber++
			}
			wg.Wait()
			if time.Since(secondStart) > time.Second {
				log.Println("Requests took longer than a second, probably need to decrease the load.")
			}
			statistics.print()
		}
	}
}

type counters struct {
	totalRequests atomic.Uint64
	errors        atomic.Uint64
	badReply      atomic.Uint64
	goodReply     atomic.Uint64
}
type statistics struct {
	counters    counters
	startTime   time.Time
	printErrors bool
}

func (s *statistics) add(response *request_maker.ResponseData, err error) {
	s.counters.totalRequests.Add(1)
	if err != nil {
		if s.printErrors {
			log.Println("Error making request:", err)
		}
		s.counters.errors.Add(1)
		return
	}
	if response.StatusCode != 200 || response.Body["error"] != nil {
		if s.printErrors {
			log.Print("Response contains error: ", response.StatusStr)
			if response.Body["error"] != nil {
				log.Println(" ", response.Body["error"])
			} else {
				log.Println()
			}
		}
		s.counters.badReply.Add(1)
	} else {
		s.counters.goodReply.Add(1)
	}
}

func (s *statistics) print() {
	elapsed := time.Since(s.startTime)
	if elapsed < time.Second {
		elapsed = time.Second
	}
	log.Printf("Speed: %.1f rps, Errors: %.1f%%, Bad response: %.1f%%, Good response: %.1f%%\n",
		float64(s.counters.totalRequests.Load())/elapsed.Seconds(),
		float64(s.counters.errors.Load())/float64(s.counters.totalRequests.Load())*100,
		float64(s.counters.badReply.Load())/float64(s.counters.totalRequests.Load())*100,
		float64(s.counters.goodReply.Load())/float64(s.counters.totalRequests.Load())*100)
}
