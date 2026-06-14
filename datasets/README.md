# Datasets for Topic 3

There are four groups:

- `small`: <= 5 processes
- `medium`: 6 to 19 processes
- `large`: 20 to 35 processes
- `very_large`: > 35 processes

Each group contains five controlled pseudo-random datasets.

All files use:

```csv
time,process_id,action,resource_id
```

Only `request` is used, matching Topic 3's Wait-for Graph model.
