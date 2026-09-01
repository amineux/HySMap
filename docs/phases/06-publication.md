# Phase 6 — Publication quality

Clone it, build it, cite it as *software*. Don't pretend this tree is the source papers. The report is an implementation note: [`docs/technical-report.md`](../technical-report.md).

## Checklist

| Item | Where |
| --- | --- |
| C++20 library, RAII, typed IDs | `include/hysmap/`, `src/` |
| CMake ≥ 3.20, FetchContent Eigen + Catch2 | `CMakeLists.txt` |
| Unit tests + `ctest` | `tests/` |
| CLI | `hysmap` |
| Python bindings | `-DHYSMAP_BUILD_PYTHON=ON`, `python/` |
| Loihi-style research export | `hysmap export --format loihi-json` |
| Reproducible seeds | every generator / mapper path |
| Technical report | [`docs/technical-report.md`](../technical-report.md) |
| GitHub Actions | `.github/workflows/ci.yml` |
| License | MIT |

## Citations (inspiration only)

- Ronzani & Silvano, [arXiv:2601.16118](https://arxiv.org/abs/2601.16118)
- Khorasanian, [arXiv:2608.26223](https://arxiv.org/abs/2608.26223)

Cite those works for the *ideas*. Cite this repo for *this implementation and these measurements*.

## SDK-shaped extras in this phase

```bash
# Python
cmake -B build -DHYSMAP_BUILD_PYTHON=ON -DHYSMAP_BUILD_TESTS=OFF
cmake --build build -j
PYTHONPATH=build python python/examples/quickstart.py

# or: pip install -e .   (scikit-build-core + pybind11)

# Loihi-style research export (not official NxSDK)
./build/hysmap export --input examples/net_potjans_80.json --mesh 4 \
    --format loihi-json --out loihi.json
```

Back to the [README spine](../../README.md).
