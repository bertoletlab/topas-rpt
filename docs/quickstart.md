# Quickstart

Once the extension is built (see [Installation](installation.md)), run the minimal example:

```bash
topas examples/canonical/uniform_minimal.txt
```

This runs the `uniform` mode: decays sampled uniformly within a geometry component, full
decay-chain tracking, time-binned normalization. It is the fastest way to confirm your
build works end to end.

## More examples

- [`examples/canonical/`](https://github.com/bertoletlab/topas-rpt/tree/main/examples/canonical) - one minimal parameter file per mode (`uniform`, `invitro_bind`, `activity_map`)
- [`examples/scientific/`](https://github.com/bertoletlab/topas-rpt/tree/main/examples/scientific) - fuller worked examples with interpretable contrasts

## Next steps

Read the [User Guide](user_guide.md) for the full parameter reference across all three
modes, and [Validation](validation.md) if you want to run the regression suite yourself.
