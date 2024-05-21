package ammo_provider

import (
	"bufio"
	"io"
	"sync/atomic"
)

type AmmoProvider struct {
	ammo           []string
	current_bullet atomic.Uint64
}

func (ap *AmmoProvider) getIndex() uint64 {
	if ap.current_bullet.Load() >= uint64(len(ap.ammo)) {
		ap.current_bullet.Store(1)
		return 0
	}
	result := ap.current_bullet.Load()
	ap.current_bullet.Add(1)
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
