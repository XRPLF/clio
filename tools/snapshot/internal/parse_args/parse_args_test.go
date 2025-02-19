package parse_args

import (
	"flag"
	"fmt"
	"os"
	"reflect"
	"testing"
)

func TestParse(t *testing.T) {
	tests := []struct {
		name       string
		args       []string
		want       *Args
		expectErr  bool
		errMessage string
	}{
		{
			name: "Valid export full mode",
			args: []string{"cmd", "--export=full", "--start_seq=1", "--end_seq=10", "--path=/data", "--grpc_server=localhost:50051"},
			want: &Args{
				ExportMode: "full",
				StartSeq:   1,
				EndSeq:     10,
				Path:       "/data",
				GrpcServer: "localhost:50051",
				ServerMode: false,
			},
			expectErr: false,
		},
		{
			name:       "Missing required flags in export mode",
			args:       []string{"cmd", "--export=delta", "--start_seq=1"},
			want:       nil,
			expectErr:  true,
			errMessage: "Invalid usage: --start_seq, --end_seq, --grpc_server and --path are required for export",
		},
		{
			name:       "Invalid export mode",
			args:       []string{"cmd", "--export=invalid"},
			want:       nil,
			expectErr:  true,
			errMessage: "Invalid usage: Invalid export mode. Use 'full' or 'delta'.",
		},
		{
			name: "Server mode with default grpc server flags",
			args: []string{"cmd", "--server", "--ws_port=1234", "--grpc_port=22", "--path=/server_data"},
			want: &Args{
				ServerMode: true,
				GrpcPort:   22,
				WsPort:     1234,
				StartSeq:   0,
				EndSeq:     0,
				Path:       "/server_data",
				GrpcServer: "localhost:50051",
			},
			expectErr: false,
		},
		{
			name:       "Server and export mode together (error)",
			args:       []string{"cmd", "--server", "--export=full"},
			want:       nil,
			expectErr:  true,
			errMessage: "Invalid usage: --server and --export cannot be used at the same time.",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			flag.CommandLine = flag.NewFlagSet(os.Args[1], flag.ExitOnError)

			os.Args = tt.args

			fmt.Println("Running test with args:", tt.args)
			got, err := Parse()

			if tt.expectErr {
				if err == nil {
					t.Errorf("Expected error but got none")
				} else if err.Error() != tt.errMessage {
					t.Errorf("Expected error message '%s', got '%s'", tt.errMessage, err.Error())
				}
			} else {
				if err != nil {
					t.Errorf("Unexpected error: %v", err)
				}
				if !reflect.DeepEqual(got, tt.want) {
					t.Errorf("Expected %+v, got %+v", tt.want, got)
				}
			}
		})
	}
}
