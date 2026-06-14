PYTHON ?= python3
DATASET_DIR ?= datasets
RESULT_DIR ?= results
PLOT_DIR ?= plots

.PHONY: all experiment plot small medium large very_large clean

all: experiment

experiment:
	$(PYTHON) run_experiments.py --dataset-dir $(DATASET_DIR) --output-dir $(RESULT_DIR) --algorithms dfs bfs --k 1 2 5 10 --repeat 5

plot:
	$(PYTHON) plot_results.py --results-dir $(RESULT_DIR) --output-dir $(PLOT_DIR)

small:
	$(PYTHON) run_experiments.py --dataset-dir $(DATASET_DIR) --output-dir $(RESULT_DIR)/small --groups small --algorithms dfs bfs --k 1 2 5 10 --repeat 5

medium:
	$(PYTHON) run_experiments.py --dataset-dir $(DATASET_DIR) --output-dir $(RESULT_DIR)/medium --groups medium --algorithms dfs bfs --k 1 2 5 10 --repeat 5

large:
	$(PYTHON) run_experiments.py --dataset-dir $(DATASET_DIR) --output-dir $(RESULT_DIR)/large --groups large --algorithms dfs bfs --k 1 2 5 10 --repeat 5

very_large:
	$(PYTHON) run_experiments.py --dataset-dir $(DATASET_DIR) --output-dir $(RESULT_DIR)/very_large --groups very_large --algorithms dfs bfs --k 1 2 5 10 --repeat 5

clean:
	rm -rf results plots __pycache__ .pytest_cache
