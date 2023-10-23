package main

import (
	"errors"
	"fmt"
	"log"
	"net/http"
	"os"

	"github.com/ethereum/go-ethereum/rpc"
)

const port = 8080

type CalculatorService struct{}

func (s *CalculatorService) Add(a, b int) int {
	return a + b
}

func (s *CalculatorService) Div(a, b int) (int, error) {
	if b == 0 {
		return 0, errors.New("divide by zero")
	}
	return a / b, nil
}

func (s *CalculatorService) Version() (string, error) {
	return fmt.Sprintf("%d", 1), nil
}

func main() {
	if err := run(); err != nil {
		log.Fatal(err)
	}
}

func run() error {
	server := rpc.NewServer()

	if err := server.RegisterName("calculator", &CalculatorService{}); err != nil {
		return err
	}

	return http.ListenAndServe(fmt.Sprintf(":%s", os.Getenv("PORT")), server)
}
