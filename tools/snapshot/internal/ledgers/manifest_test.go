package ledgers

import (
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestManifest(t *testing.T) {
	manifest := NewManifest("testdata")
	defer os.RemoveAll("testdata")

	assert.False(t, manifest.IsExist())
	_, _, err := manifest.Read()
	assert.Error(t, err)

	err = manifest.SetLedgerRange(1, 10)
	assert.NoError(t, err)
	err = manifest.AppendDeltaLedger(11, 20)
	assert.NoError(t, err)
	assert.True(t, manifest.IsExist())
	err = manifest.AppendDeltaLedger(22, 30)
	assert.Error(t, err)

	start, end, err := manifest.Read()
	assert.NoError(t, err)
	assert.Equal(t, start, uint32(1))
	assert.Equal(t, end, uint32(20))
}

func TestManifestInvalidPath(t *testing.T) {
	manifest := NewManifest("/")

	assert.False(t, manifest.IsExist())
	_, _, err := manifest.Read()
	assert.Error(t, err)

	err = manifest.SetLedgerRange(1, 10)
	assert.Error(t, err)
}
