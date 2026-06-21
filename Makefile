CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L
LDFLAGS ?=

BIN_DIR := bin
BUILD_DIR := build
DATASET_DIR ?= datasets
RESULT_DIR ?= results
PLOT_DIR ?= plots

CORE_OBJ := $(BUILD_DIR)/deadlock_core.o

.PHONY: all binaries experiment plot simulator small medium large very_large clean

all: experiment

binaries: $(BIN_DIR)/deadlock_simulator $(BIN_DIR)/run_experiments $(BIN_DIR)/plot_results

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(CORE_OBJ): src/deadlock_core.c src/deadlock_core.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc -c src/deadlock_core.c -o $@

$(BIN_DIR)/deadlock_simulator: src/deadlock_simulator.c $(CORE_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -Isrc src/deadlock_simulator.c $(CORE_OBJ) $(LDFLAGS) -o $@

$(BIN_DIR)/run_experiments: src/run_experiments.c $(CORE_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -Isrc src/run_experiments.c $(CORE_OBJ) $(LDFLAGS) -o $@

$(BIN_DIR)/plot_results: src/plot_results.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -Isrc src/plot_results.c $(LDFLAGS) -lm -o $@

experiment: $(BIN_DIR)/run_experiments
	./$(BIN_DIR)/run_experiments --dataset-dir $(DATASET_DIR) --output-dir $(RESULT_DIR) --algorithms dfs bfs --k 1 2 5 10 --repeat 5

plot: $(BIN_DIR)/plot_results
	./$(BIN_DIR)/plot_results --results-dir $(RESULT_DIR) --output-dir $(PLOT_DIR)

simulator: $(BIN_DIR)/deadlock_simulator
	@echo "Usage: ./$(BIN_DIR)/deadlock_simulator datasets/small/small_random_5p.csv -k 5 --algorithm dfs --trace"

small: $(BIN_DIR)/run_experiments
	./$(BIN_DIR)/run_experiments --dataset-dir $(DATASET_DIR) --output-dir $(RESULT_DIR)/small --groups small --algorithms dfs bfs --k 1 2 5 10 --repeat 5

medium: $(BIN_DIR)/run_experiments
	./$(BIN_DIR)/run_experiments --dataset-dir $(DATASET_DIR) --output-dir $(RESULT_DIR)/medium --groups medium --algorithms dfs bfs --k 1 2 5 10 --repeat 5

large: $(BIN_DIR)/run_experiments
	./$(BIN_DIR)/run_experiments --dataset-dir $(DATASET_DIR) --output-dir $(RESULT_DIR)/large --groups large --algorithms dfs bfs --k 1 2 5 10 --repeat 5

very_large: $(BIN_DIR)/run_experiments
	./$(BIN_DIR)/run_experiments --dataset-dir $(DATASET_DIR) --output-dir $(RESULT_DIR)/very_large --groups very_large --algorithms dfs bfs --k 1 2 5 10 --repeat 5

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR) results plots
