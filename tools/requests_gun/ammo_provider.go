package main

import (
	"os"
	"strings"
	"sync/atomic"
)

type AmmoProvider struct {
	ammo           []string
	current_bullet atomic.Uint64
}

func (ap *AmmoProvider) getIndex() int {
	if int(ap.current_bullet.Load()) >= len(ap.ammo) {
		ap.current_bullet.Store(0)
	}
	result := int(ap.current_bullet.Load())
	ap.current_bullet.Add(1)
	return result
}

func (ap *AmmoProvider) GetBullet() string {
	return ap.ammo[ap.getIndex()]
}

func NewAmmoProvider(ammoFile string) AmmoProvider {
	file, err := os.ReadFile(ammoFile)
	CheckError(err, "Failed to open ammo file")
	ammo := strings.Split(string(file), "\n")
	return AmmoProvider{ammo, atomic.Uint64{}}
}
