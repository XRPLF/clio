package ammo_provider

import (
	"bufio"
	"io"
	"strings"
	"sync/atomic"
)

type AmmoProvider struct {
	ammo          []string
	currentBullet atomic.Uint64
}

func (ap *AmmoProvider) getIndex() uint64 {
	result := ap.currentBullet.Add(1)
	return result % uint64(len(ap.ammo))
}

func (ap *AmmoProvider) GetBullet() string {
	for {
		res := ap.ammo[ap.getIndex()]
		if !strings.HasPrefix(res, "#") {
			return res
		}
	}
}

func New(reader io.Reader) *AmmoProvider {
	scanner := bufio.NewScanner(reader)
	var ammo []string
	for scanner.Scan() {
		ammo = append(ammo, scanner.Text())
	}

	return &AmmoProvider{ammo: ammo}
}
