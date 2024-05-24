package ammo_provider

import (
	"bufio"
	"io"
	"sync/atomic"
)

type AmmoProvider struct {
	ammo           []string
	currentBullet atomic.Uint64
}

func (ap *AmmoProvider) getIndex() uint64 {
	if ap.currentBullet.Load() >= uint64(len(ap.ammo)) {
		ap.currentBullet.Store(1)
		return 0
	}
	result := ap.currentBullet.Load()
	ap.currentBullet.Add(1)
	return result
}

func (ap *AmmoProvider) GetBullet() string {
	return ap.ammo[ap.getIndex()]
}

func New(reader io.Reader) *AmmoProvider {
	scanner := bufio.NewScanner(reader)
	var ammo []string
	for scanner.Scan() {
		ammo = append(ammo, scanner.Text())
	}

	return &AmmoProvider{ammo: ammo}
}
