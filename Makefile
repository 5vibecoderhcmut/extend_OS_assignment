PYTHON ?= python3

.PHONY: run small large trace experiment generate clean

run: small

small:
	$(PYTHON) deadlock_simulator.py datasets/small_deadlock.csv -k 1 --algorithm dfs

large:
	$(PYTHON) deadlock_simulator.py datasets/large_deadlock.csv -k 10 --algorithm dfs

trace:
	$(PYTHON) deadlock_simulator.py datasets/small_deadlock.csv -k 1 --algorithm dfs --trace

experiment:
	$(PYTHON) run_experiments.py --output results/experiment_summary.csv

generate:
	$(PYTHON) generate_dataset.py datasets/large_deadlock.csv --processes 20 --resources 20 --events 200 --seed 42

clean:
	rm -rf results __pycache__ .pytest_cache
